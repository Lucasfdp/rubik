#ifndef RENDER_GEOMETRY_H
# define RENDER_GEOMETRY_H

# include <stdint.h>
# include <stdbool.h>
# include "raylib.h"
# include "cube.h"

/// One slot per visible cubie: 8 corners + 12 edges + 6 centres.
# define RENDER_CUBIE_COUNT (CORNER_COUNT + EDGE_COUNT + RENDER_FACE_COUNT)

/// World-unit distance between neighbouring cubie centres. Slightly over
/// 1 so a visible seam shows between cubies with no extra draw call.
# define CUBIE_SPACING 1.05f

/// World-unit size of a cubie's solid body (draw.c's DrawCube edge
/// length). Shared here, not kept private to draw.c, so input.c's
/// raycasting (Phase 6, docs/en/03b-3d-implementation-plan.md section
/// 8.3) hit-tests the exact same box draw.c renders instead of a second,
/// hand-copied magic number.
# define CUBIE_BODY_SIZE 0.94f

/// World axis, used both for a move's turn axis (MOVE_AXIS in anim.c) and
/// for describing which layer is transiently mid-turn (t_active_turn
/// below). Axis convention locked in docs/en/03b-3d-implementation-plan.md
/// section 2.3: +X = R, +Y = U, +Z = F (right-handed).
typedef enum e_axis
{
	AXIS_X,
	AXIS_Y,
	AXIS_Z
}	t_axis;

typedef enum e_render_face
{
	FACE_UP,
	FACE_DOWN,
	FACE_RIGHT,
	FACE_LEFT,
	FACE_FRONT,
	FACE_BACK,
	RENDER_FACE_COUNT
}	t_render_face;

/// One of the 26 visible cubies. x/y/z is its fixed SLOT position on the
/// lattice (each in {-1, 0, 1}) — set once by geometry_init() and never
/// changed again: a move never moves a cubie struct to a different slot,
/// it only ever repaints which colours sit in the 26 fixed slots. That is
/// what makes geometry_sync() a full, cheap, always-correct resync
/// instead of incremental bookkeeping that can drift.
typedef struct s_render_cubie
{
	int8_t	x;
	int8_t	y;
	int8_t	z;
	bool	has_face[RENDER_FACE_COUNT];
	Color	face[RENDER_FACE_COUNT];
}	t_render_cubie;

typedef struct s_render_scene
{
	t_render_cubie	cubies[RENDER_CUBIE_COUNT];
}	t_render_scene;

/// Non-NULL / active only while an animation or drag is transiently
/// rotating one layer; draw_scene() spins the matching cubies by
/// angle_deg around the world axis before translating them, and draws
/// every other cubie at its normal fixed position.
typedef struct s_active_turn
{
	bool	active;
	t_axis	axis;
	int8_t	layer;
	float	angle_deg;
}	t_active_turn;

/// One playable colour scheme: the 6 face colours geometry_sync() paints
/// stickers from (docs/en/03b-3d-implementation-plan.md section 9.2).
/// Field order matches that doc's own draft, not t_render_face's enum
/// order — rebuild_color_tables() (geometry.c) is what maps between them.
typedef struct s_palette
{
	Color	u;
	Color	d;
	Color	f;
	Color	b;
	Color	r;
	Color	l;
}	t_palette;

/// The two built-in schemes (geometry.c): WCA classic, and a
/// deliberately drastic alternate that changes all 6 faces (not just
/// some of them).
extern const t_palette	PALETTE_CLASSIC;
extern const t_palette	PALETTE_VIVID;

/// @brief One-time setup: assigns each of the 26 slots its fixed lattice
///        position and which of the 6 directions carry a sticker, then
///        calls geometry_sync() against SOLVED_CUBE.
void	geometry_init(t_render_scene *scene);

/// @brief Full resync: recomputes every cubie's face[] colours from the
///        CURRENT logical cube. Cheap (26 cubies, plain array reads), and
///        meant to be called after every committed move — never
///        incrementally updated, so there is no bookkeeping path that
///        can drift out of sync with cube.
void	geometry_sync(t_render_scene *scene, const t_cube *cube);

/// @brief Makes `palette` the active one — every future geometry_sync()
///        paints from it. Does not repaint by itself: call
///        geometry_sync() again right after to actually see it.
void	geometry_set_palette(const t_palette *palette);

/// @brief Switches to the other built-in palette and makes it active
///        (same caveat as geometry_set_palette()).
///
/// @return The newly active palette's short name, for the HUD.
const char	*geometry_palette_cycle(void);

#endif
