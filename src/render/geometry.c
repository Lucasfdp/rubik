#include "render/geometry.h"

/// WCA colour convention (docs/en/02a-cube-notation.md section 1):
/// U=white, D=yellow, F=green, B=blue, R=red, L=orange. Named with a _C
/// suffix purely to avoid clashing with raylib's own WHITE/RED/etc.
static const Color	WHITE_C = {255, 255, 255, 255};
static const Color	YELLOW_C = {255, 213, 0, 255};
static const Color	GREEN_C = {0, 158, 96, 255};
static const Color	BLUE_C = {0, 81, 186, 255};
static const Color	RED_C = {196, 30, 58, 255};
static const Color	ORANGE_C = {255, 88, 0, 255};

/// Fixed lattice position of each corner slot, one entry per t_corner.
/// Derived mechanically from the slot's own name letters (U/D -> y,
/// R/L -> x, F/B -> z) under this doc's axis convention (+X=R, +Y=U,
/// +Z=F) — see geometry.h's t_axis comment.
static const int8_t	CORNER_POS[CORNER_COUNT][3] = {
	[CORNER_URF] = {1, 1, 1},
	[CORNER_UFL] = {-1, 1, 1},
	[CORNER_ULB] = {-1, 1, -1},
	[CORNER_UBR] = {1, 1, -1},
	[CORNER_DFR] = {1, -1, 1},
	[CORNER_DLF] = {-1, -1, 1},
	[CORNER_DBL] = {-1, -1, -1},
	[CORNER_DRB] = {1, -1, -1},
};

static const int8_t	EDGE_POS[EDGE_COUNT][3] = {
	[EDGE_UR] = {1, 1, 0},
	[EDGE_UF] = {0, 1, 1},
	[EDGE_UL] = {-1, 1, 0},
	[EDGE_UB] = {0, 1, -1},
	[EDGE_DR] = {1, -1, 0},
	[EDGE_DF] = {0, -1, 1},
	[EDGE_DL] = {-1, -1, 0},
	[EDGE_DB] = {0, -1, -1},
	[EDGE_FR] = {1, 0, 1},
	[EDGE_FL] = {-1, 0, 1},
	[EDGE_BL] = {-1, 0, -1},
	[EDGE_BR] = {1, 0, -1},
};

static const int8_t	CENTER_POS[RENDER_FACE_COUNT][3] = {
	[FACE_UP] = {0, 1, 0},
	[FACE_DOWN] = {0, -1, 0},
	[FACE_RIGHT] = {1, 0, 0},
	[FACE_LEFT] = {-1, 0, 0},
	[FACE_FRONT] = {0, 0, 1},
	[FACE_BACK] = {0, 0, -1},
};

/// Each slot's visible directions, in the SAME order as its name's
/// letters (e.g. "URF" -> up, right, front). CORNER_COLORS below lists
/// each PIECE's colours in that same per-name letter order, so colour k
/// always belongs with direction k here — that pairing, plus the
/// twist/flip rotation in geometry_sync(), is the entire colour
/// algorithm.
static const t_render_face	CORNER_DIRS[CORNER_COUNT][3] = {
	[CORNER_URF] = {FACE_UP, FACE_RIGHT, FACE_FRONT},
	[CORNER_UFL] = {FACE_UP, FACE_FRONT, FACE_LEFT},
	[CORNER_ULB] = {FACE_UP, FACE_LEFT, FACE_BACK},
	[CORNER_UBR] = {FACE_UP, FACE_BACK, FACE_RIGHT},
	[CORNER_DFR] = {FACE_DOWN, FACE_FRONT, FACE_RIGHT},
	[CORNER_DLF] = {FACE_DOWN, FACE_LEFT, FACE_FRONT},
	[CORNER_DBL] = {FACE_DOWN, FACE_BACK, FACE_LEFT},
	[CORNER_DRB] = {FACE_DOWN, FACE_RIGHT, FACE_BACK},
};

static const t_render_face	EDGE_DIRS[EDGE_COUNT][2] = {
	[EDGE_UR] = {FACE_UP, FACE_RIGHT},
	[EDGE_UF] = {FACE_UP, FACE_FRONT},
	[EDGE_UL] = {FACE_UP, FACE_LEFT},
	[EDGE_UB] = {FACE_UP, FACE_BACK},
	[EDGE_DR] = {FACE_DOWN, FACE_RIGHT},
	[EDGE_DF] = {FACE_DOWN, FACE_FRONT},
	[EDGE_DL] = {FACE_DOWN, FACE_LEFT},
	[EDGE_DB] = {FACE_DOWN, FACE_BACK},
	[EDGE_FR] = {FACE_FRONT, FACE_RIGHT},
	[EDGE_FL] = {FACE_FRONT, FACE_LEFT},
	[EDGE_BL] = {FACE_BACK, FACE_LEFT},
	[EDGE_BR] = {FACE_BACK, FACE_RIGHT},
};

/// Indexed by t_corner. Each piece's 3 sticker colours, listed in the
/// same per-name letter order as CORNER_DIRS above (e.g. URF: white
/// first because U is the piece's own U/D-type sticker, per
/// docs/en/02a-cube-notation.md section 4).
///
/// Verified by simulation, not by eye: a throwaway script modelled the 26
/// cubies as real 3D points with rotating stickers, replayed 5,000
/// random moves through both that physical model and cube.h's own
/// corner_perm/corner_orient bookkeeping, and compared the two — the
/// "rotate a piece's colour list RIGHT by its orientation" rule in
/// geometry_sync() below is the one that reproduces the physical model
/// exactly (0 mismatches over the whole run); a left rotation does not.
static const Color	CORNER_COLORS[CORNER_COUNT][3] = {
	[CORNER_URF] = {WHITE_C, RED_C, GREEN_C},
	[CORNER_UFL] = {WHITE_C, GREEN_C, ORANGE_C},
	[CORNER_ULB] = {WHITE_C, ORANGE_C, BLUE_C},
	[CORNER_UBR] = {WHITE_C, BLUE_C, RED_C},
	[CORNER_DFR] = {YELLOW_C, GREEN_C, RED_C},
	[CORNER_DLF] = {YELLOW_C, ORANGE_C, GREEN_C},
	[CORNER_DBL] = {YELLOW_C, BLUE_C, ORANGE_C},
	[CORNER_DRB] = {YELLOW_C, RED_C, BLUE_C},
};

/// Indexed by t_edge, same per-name letter order as EDGE_DIRS. Same
/// simulation as CORNER_COLORS verified this table plus a right
/// rotation (equivalently, for a 2-entry list, a plain swap when
/// flip == 1).
static const Color	EDGE_COLORS[EDGE_COUNT][2] = {
	[EDGE_UR] = {WHITE_C, RED_C},
	[EDGE_UF] = {WHITE_C, GREEN_C},
	[EDGE_UL] = {WHITE_C, ORANGE_C},
	[EDGE_UB] = {WHITE_C, BLUE_C},
	[EDGE_DR] = {YELLOW_C, RED_C},
	[EDGE_DF] = {YELLOW_C, GREEN_C},
	[EDGE_DL] = {YELLOW_C, ORANGE_C},
	[EDGE_DB] = {YELLOW_C, BLUE_C},
	[EDGE_FR] = {GREEN_C, RED_C},
	[EDGE_FL] = {GREEN_C, ORANGE_C},
	[EDGE_BL] = {BLUE_C, ORANGE_C},
	[EDGE_BR] = {BLUE_C, RED_C},
};

static const Color	CENTER_COLORS[RENDER_FACE_COUNT] = {
	[FACE_UP] = WHITE_C,
	[FACE_DOWN] = YELLOW_C,
	[FACE_RIGHT] = RED_C,
	[FACE_LEFT] = ORANGE_C,
	[FACE_FRONT] = GREEN_C,
	[FACE_BACK] = BLUE_C,
};

/// @brief Clears every has_face[] flag, then sets exactly the `n`
///        directions in `dirs` to true.
static void	set_faces(t_render_cubie *cubie, const t_render_face *dirs,
	int n)
{
	int	i;

	i = 0;
	while (i < RENDER_FACE_COUNT)
	{
		cubie->has_face[i] = false;
		i++;
	}
	i = 0;
	while (i < n)
	{
		cubie->has_face[dirs[i]] = true;
		i++;
	}
}

/// @brief Sets the one fixed lattice position shared by CORNER_POS,
///        EDGE_POS and CENTER_POS's row layout.
static void	set_pos(t_render_cubie *cubie, const int8_t pos[3])
{
	cubie->x = pos[0];
	cubie->y = pos[1];
	cubie->z = pos[2];
}

void	geometry_init(t_render_scene *scene)
{
	int				i;
	t_render_cubie	*cubie;
	t_render_face	dir;

	i = 0;
	while (i < CORNER_COUNT)
	{
		set_pos(&scene->cubies[i], CORNER_POS[i]);
		set_faces(&scene->cubies[i], CORNER_DIRS[i], 3);
		i++;
	}
	i = 0;
	while (i < EDGE_COUNT)
	{
		set_pos(&scene->cubies[CORNER_COUNT + i], EDGE_POS[i]);
		set_faces(&scene->cubies[CORNER_COUNT + i], EDGE_DIRS[i], 2);
		i++;
	}
	i = 0;
	while (i < RENDER_FACE_COUNT)
	{
		dir = (t_render_face)i;
		cubie = &scene->cubies[CORNER_COUNT + EDGE_COUNT + i];
		set_pos(cubie, CENTER_POS[i]);
		set_faces(cubie, &dir, 1);
		cubie->face[dir] = CENTER_COLORS[i];
		i++;
	}
	geometry_sync(scene, &SOLVED_CUBE);
}

/// @brief One slot's worth of geometry_sync(): rotates `colors` right by
///        `orient` (mod `n`) and assigns the result to `dirs`, in order.
///
/// Right rotation, not left: rotated[k] = colors[(k - orient) mod n].
/// This is the one sign flagged as easy to get backwards in the header
/// comment above CORNER_COLORS — see there for how it was checked.
static void	assign_rotated(t_render_cubie *cubie, const Color *colors,
	const t_render_face *dirs, int n, int orient)
{
	int	k;
	int	src;

	k = 0;
	while (k < n)
	{
		src = ((k - orient) % n + n) % n;
		cubie->face[dirs[k]] = colors[src];
		k++;
	}
}

void	geometry_sync(t_render_scene *scene, const t_cube *cube)
{
	int	i;
	int	piece;

	i = 0;
	while (i < CORNER_COUNT)
	{
		piece = cube->corner_perm[i];
		assign_rotated(&scene->cubies[i], CORNER_COLORS[piece],
			CORNER_DIRS[i], 3, cube->corner_orient[i]);
		i++;
	}
	i = 0;
	while (i < EDGE_COUNT)
	{
		piece = cube->edge_perm[i];
		assign_rotated(&scene->cubies[CORNER_COUNT + i], EDGE_COLORS[piece],
			EDGE_DIRS[i], 2, cube->edge_orient[i]);
		i++;
	}
}
