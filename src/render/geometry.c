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

/// Phase 7 §9.2's second scheme (PALETTE_VIVID): deliberately NOT a
/// subtle variation — every one of the 6 faces gets a different hue from
/// PALETTE_CLASSIC, including U/D/L, which the first cut of this palette
/// left unchanged (feedback: a palette switch should be obvious on every
/// face, not just some of them).
static const Color	VIVID_U = {255, 20, 147, 255};
static const Color	VIVID_D = {155, 0, 255, 255};
static const Color	VIVID_F = {160, 255, 0, 255};
static const Color	VIVID_B = {0, 191, 255, 255};
static const Color	VIVID_R = {255, 105, 0, 255};
static const Color	VIVID_L = {0, 200, 180, 255};

/// Fixed lattice position of each corner slot, one entry per t_corner.
/// Derived mechanically from the slot's own name letters (U/D -> y,
/// R/L -> x, F/B -> z) under this doc's axis convention (+X -> R, +Y ->
/// U, +Z -> F) — see geometry.h's t_axis comment.
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
/// letters (e.g. "URF" -> up, right, front). Before Phase 7, CORNER_
/// COLORS/EDGE_COLORS below listed each PIECE's colours in that same
/// per-name letter order as a literal table; colour k always belonged
/// with direction k here, which is exactly what let Phase 7 §9.2 replace
/// that literal table with CENTER_COLORS[CORNER_DIRS[piece][k]] instead
/// (rebuild_color_tables() below) — checked by hand against every one of
/// the old table's entries before it was removed.
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

/// The two built-in schemes (Phase 7 §9.2). Field order (u, d, f, b, r,
/// l) matches the design doc's own draft struct literal.
const t_palette	PALETTE_CLASSIC = {
	.u = WHITE_C, .d = YELLOW_C, .f = GREEN_C,
	.b = BLUE_C, .r = RED_C, .l = ORANGE_C,
};

const t_palette	PALETTE_VIVID = {
	.u = VIVID_U, .d = VIVID_D, .f = VIVID_F,
	.b = VIVID_B, .r = VIVID_R, .l = VIVID_L,
};

/// geometry_sync() reads these three exactly as before Phase 7 — the
/// only change is that they are no longer literal, they are GENERATED
/// from whichever t_palette is active (rebuild_color_tables() below), so
/// a palette switch is one rebuild instead of two hand-edited tables
/// that could disagree.
static Color		CENTER_COLORS[RENDER_FACE_COUNT];
static Color		CORNER_COLORS[CORNER_COUNT][3];
static Color		EDGE_COLORS[EDGE_COUNT][2];
static t_palette	g_active_palette;
static int			g_palette_index;

/// @brief Regenerates CENTER_COLORS/CORNER_COLORS/EDGE_COLORS from
///        g_active_palette. CORNER_DIRS/EDGE_DIRS above already say
///        which face each colour slot belongs to — exactly what
///        CENTER_COLORS is keyed on too, so CORNER_COLORS[p][k] is just
///        CENTER_COLORS[CORNER_DIRS[p][k]] (same idea for edges).
static void	rebuild_color_tables(void)
{
	int	i;
	int	k;

	CENTER_COLORS[FACE_UP] = g_active_palette.u;
	CENTER_COLORS[FACE_DOWN] = g_active_palette.d;
	CENTER_COLORS[FACE_RIGHT] = g_active_palette.r;
	CENTER_COLORS[FACE_LEFT] = g_active_palette.l;
	CENTER_COLORS[FACE_FRONT] = g_active_palette.f;
	CENTER_COLORS[FACE_BACK] = g_active_palette.b;
	i = 0;
	while (i < CORNER_COUNT)
	{
		k = 0;
		while (k < 3)
		{
			CORNER_COLORS[i][k] = CENTER_COLORS[CORNER_DIRS[i][k]];
			k++;
		}
		i++;
	}
	i = 0;
	while (i < EDGE_COUNT)
	{
		k = 0;
		while (k < 2)
		{
			EDGE_COLORS[i][k] = CENTER_COLORS[EDGE_DIRS[i][k]];
			k++;
		}
		i++;
	}
}

void	geometry_set_palette(const t_palette *palette)
{
	g_active_palette = *palette;
	rebuild_color_tables();
}

const char	*geometry_palette_cycle(void)
{
	g_palette_index = !g_palette_index;
	if (g_palette_index == 0)
	{
		geometry_set_palette(&PALETTE_CLASSIC);
		return ("classic");
	}
	geometry_set_palette(&PALETTE_VIVID);
	return ("vivid");
}

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

	geometry_set_palette(&PALETTE_CLASSIC);
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
	i = 0;
	while (i < RENDER_FACE_COUNT)
	{
		scene->cubies[CORNER_COUNT + EDGE_COUNT + i].face[i]
			= CENTER_COLORS[i];
		i++;
	}
}
