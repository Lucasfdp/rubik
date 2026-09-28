# 3D Bonus — Full Implementation Plan (Phase by Phase)

*(Part of the Rubik prep docs — see `00-overview.md` for the index and `GLOSSARY.md` for term definitions. This doc turns the design in `03a-3d-experience-and-interaction.md` into an actual build order: concrete data structures, function signatures, algorithms, file lists, and an exit gate for every phase. It assumes `03-graphics.md`'s stack decision (raylib) and `04-architecture.md`'s module boundary. Written against the repo as it stands today: `apply_move()`, `SOLVED_CUBE`, `t_cube`, `t_move`, and the Kociemba/Thistlethwaite solvers already exist and are untouched by anything below; raylib is already vendored and building (`lib/raylib/src/libraylib.a` exists) — the Sprint 5 build spike is done.)*

## 0. Decisions this doc locks in

Two of the open items in `08-risks-and-open-decisions.md` are now decided, not just recommended:

- **#6 — button-split.** Left-drag on a sticker turns a layer; right-drag anywhere orbits the camera. Confirmed, built into Phase 3 and Phase 6 below.
- **#7 — sequencing.** Keyboard-driven manual turning ships and is fully working *before* mouse click-and-drag is attempted. Phase 3 (keyboard) must pass its gate before Phase 6 (mouse) starts.

Scope for this plan: everything in `03a`'s MoSCoW table except the three **Won't** items (custom shader work, a draggable timeline scrub handle, VR/AR) — i.e. every Must, every Should, and every Could.

## 1. How the phases map onto `06-roadmap-bonus.md`

| This doc | `06-roadmap-bonus.md` | MoSCoW tier |
|---|---|---|
| Phase 0 — Render skeleton | Sprint 5 | (prerequisite plumbing, not its own MoSCoW row) |
| Phase 1 — Mode A: autoplay | Sprint 6 | Must |
| Phase 2 — Mode B: playback transport | Sprint 6 | Must |
| Phase 3 — Mode C: keyboard + button-split | Sprint 6 / start of 7 | Must |
| Phase 4 — Practice-mode extras | Sprint 7 | Should |
| Phase 5 — Visual-polish baseline | Sprint 7 | Should |
| Phase 6 — Mouse click-and-drag turning | Sprint 7 | Could |
| Phase 7 — Remaining polish & extras | Sprint 7 | Could |

Each phase lists: goal, dependency, new/changed files, the actual data structures and function signatures, the algorithm, the exit gate, and an effort estimate. Nothing here is pseudocode disguised as vague hand-waving — every signature is meant to be typed in close to as-is; every algorithm step is meant to be implementable directly from this description.

## 2. One design clarification worth flagging up front

`04-architecture.md` earmarks `cube/facelet.c` (54-sticker <-> cubie conversion) as something "needed by ... the renderer." Having worked through the actual rendering algorithm below (Phase 0), the renderer turns out **not** to need the 54-character facelet string at all — it reads `corner_perm`/`corner_orient`/`edge_perm`/`edge_orient` directly and maps them to colours via a small per-piece reference-colour table (§4.2 below), which is simpler and has no hand-built index-translation table to get wrong. `facelet.c` is still worth building at some point — it's genuinely useful for a future `-f`/debug facelet-string output and for comparing your solver's state against reference tools online — but it is **not on the critical path** for any phase below, and none of these phases depend on it. Build it whenever convenient, independently.

## Phase 0 — Render skeleton (window, camera, static scene)

**Goal:** `./rubik_bonus "<scramble>" -r` opens a window showing a correctly-coloured, correctly-shaped static cube (no animation yet), with mouse-orbit camera control. This is the foundation everything else attaches to.

**Depends on:** nothing — first phase.

### 2.1 New files

```
include/render.h              NEW — raylib-agnostic umbrella, the ONLY
                               header main.c is allowed to include from
                               this whole feature.
include/render/app.h          NEW
include/render/geometry.h     NEW
include/render/draw.h         NEW
src/render/app.c              NEW
src/render/geometry.c         NEW
src/render/draw.c             NEW
```

`include/render.h` deliberately declares nothing raylib-flavoured, because `main.c`'s `BONUS_ALGO` translation unit is compiled *without* raylib's include path (only objects under `src/render/` get `-I$(RAYLIB_SRC_DIR)`, per the Makefile's pattern-specific `CPPFLAGS` rule). Keep the boundary real, not just conventional:

```c
#ifndef RENDER_H
# define RENDER_H
# include <stdbool.h>
# include "cube.h"

/// @brief Opens the 3D window and runs the render loop until the user
///        closes it. Owns everything under src/render/ internally; this
///        is the entire public surface main.c ever touches.
///
/// @param start_cube  The cube to open on. When has_scramble is false,
///                    this is SOLVED_CUBE and the window opens straight
///                    into Mode C (manual/practice).
/// @param has_scramble True to auto-solve start_cube and play the
///                    solution back on open (Mode A); false to open on a
///                    solved cube ready for manual turning.
/// @return true on a clean exit, false on an internal render error.
bool	render_run(const t_cube *start_cube, bool has_scramble);
# endif
```

### 2.2 `main.c` change

Inside the existing `#ifdef BONUS_ALGO` branch, extend `parse_args()` (or add a second, small parse step after it) to accept a `-r` flag, and make the scramble argument optional when `-r` is present:

```c
/// usage: rubik_bonus ["<scramble>"] [-a kociemba|thistlethwaite] [-r]
```

- `-r` absent: exactly today's behaviour (text solve, `-a` selectable). Nothing about the existing tested CLI path changes — `tests/test_cli.sh` and any `-a` comparison scripting keep working untouched.
- `-r` present with a scramble: parse + validate as today, then instead of `solve_and_print()` / `solve_and_print_thistle()`, call `render_run(&cube, true)`.
- `-r` present with no scramble: skip parsing entirely, call `render_run(&SOLVED_CUBE, false)`.

This keeps `main.c`'s existing anti-cheat pipeline (parse -> apply -> validate -> hand only the cube onward) completely intact for the auto-play case, and is a strict superset of today's flag grammar — no existing usage breaks.

### 2.3 Data structures (`include/render/geometry.h`)

```c
typedef enum e_render_face
{
	FACE_UP, FACE_DOWN, FACE_RIGHT, FACE_LEFT, FACE_FRONT, FACE_BACK,
	RENDER_FACE_COUNT
}	t_render_face;

/// One of the 26 visible cubies. `x/y/z` is its SLOT position on the
/// lattice — fixed forever, one entry per physical slot, never permuted.
/// Only `face[]` changes, and only in response to geometry_sync(). This
/// is deliberate: it means a move never moves a cubie struct to a
/// different slot; it only ever repaints which colours sit in the 26
/// fixed slots, which is what makes geometry_sync() a full, cheap,
/// always-correct resync rather than incremental bookkeeping that can
/// drift.
typedef struct s_render_cubie
{
	int8_t	x;
	int8_t	y;
	int8_t	z;
	Color	face[RENDER_FACE_COUNT];	// BLANK_COLOR where this cubie has
										// no sticker facing that direction
}	t_render_cubie;

typedef struct s_render_scene
{
	t_render_cubie	cubies[26];
}	t_render_scene;

/// @brief One-time setup: assigns each of the 26 slots its fixed lattice
///        position, then calls geometry_sync() against SOLVED_CUBE.
void	geometry_init(t_render_scene *scene);

/// @brief Full resync: recomputes every cubie's `face[]` colours from the
///        CURRENT logical cube. Cheap (26 cubies, plain array reads), and
///        called after every committed move — never incrementally
///        updated, so there is no bookkeeping path that can drift out of
///        sync with `cube`.
void	geometry_sync(t_render_scene *scene, const t_cube *cube);
```

Axis convention (pick once, write it here, never revisit): **+X = R, +Y = U, +Z = F** (right-handed). `FACE_RIGHT` sticker faces +X, `FACE_UP` faces +Y, `FACE_FRONT` faces +Z, and the opposite three are the negative directions.

### 2.4 The colour algorithm — no facelet string needed

Every corner and edge's name already tells you its solved-state colours, because names are written face-letter by face-letter (`CORNER_URF` touches U, R, F). Build one static table per piece kind, directly from `cube.h`'s existing enum ordering:

```c
/// Indexed by t_corner. Each entry is the piece's 3 sticker colours,
/// always listed starting with its U/D-type sticker first (matching how
/// corner_orient is defined in 02a-cube-notation.md §4), then the other
/// two in a fixed clockwise order (viewed from outside the corner).
static const Color	CORNER_COLORS[CORNER_COUNT][3] = {
	[CORNER_URF] = {WHITE_C, RED_C,   GREEN_C},
	[CORNER_UFL] = {WHITE_C, GREEN_C, ORANGE_C},
	[CORNER_ULB] = {WHITE_C, ORANGE_C, BLUE_C},
	[CORNER_UBR] = {WHITE_C, BLUE_C,  RED_C},
	[CORNER_DFR] = {YELLOW_C, GREEN_C, RED_C},
	[CORNER_DLF] = {YELLOW_C, ORANGE_C, GREEN_C},
	[CORNER_DBL] = {YELLOW_C, BLUE_C, ORANGE_C},
	[CORNER_DRB] = {YELLOW_C, RED_C,  BLUE_C},
};

/// Indexed by t_edge. Each entry is {U/D-or-F/B-type sticker, the other}.
static const Color	EDGE_COLORS[EDGE_COUNT][2] = {
	[EDGE_UR] = {WHITE_C, RED_C},   [EDGE_UF] = {WHITE_C, GREEN_C},
	[EDGE_UL] = {WHITE_C, ORANGE_C},[EDGE_UB] = {WHITE_C, BLUE_C},
	[EDGE_DR] = {YELLOW_C, RED_C},  [EDGE_DF] = {YELLOW_C, GREEN_C},
	[EDGE_DL] = {YELLOW_C, ORANGE_C},[EDGE_DB] = {YELLOW_C, BLUE_C},
	[EDGE_FR] = {GREEN_C, RED_C},   [EDGE_FL] = {GREEN_C, ORANGE_C},
	[EDGE_BL] = {BLUE_C, ORANGE_C}, [EDGE_BR] = {BLUE_C, RED_C},
};
```

(`WHITE_C` etc. are your own `Color` constants, WCA convention: U=white, D=yellow, F=green, B=blue, R=red, L=orange — per `02a-cube-notation.md` §1. Named with a `_C` suffix here only to avoid clashing with raylib's own `WHITE`/`RED`/etc. — use whichever you actually prefer, just be consistent.)

`geometry_sync()`'s algorithm, per corner slot `s` (0..7) and mirrored for edges:

1. `piece = cube->corner_perm[s]` — which piece currently sits in slot `s`.
2. `twist = cube->corner_orient[s]` — 0, 1, or 2.
3. Rotate `CORNER_COLORS[piece]` left by `twist` positions to get the 3 colours **in slot order** (i.e. which colour is now on the slot's U/D-facing side, which on the next, which on the last) — a fixed 3-entry rotate, trivial.
4. Slot `s`'s 3 visible world-directions are static and already known (e.g. slot `CORNER_URF`'s position is `(1,1,1)`, so its 3 visible faces are always `FACE_RIGHT, FACE_UP, FACE_FRONT` in that fixed order). Assign the 3 rotated colours to those 3 fixed directions on `scene->cubies[piece]`.

Edges: same idea with 2 colours and `edge_orient` (0 or 1: swap or don't).

Centres: 6 static entries, never touched by `geometry_sync()` after `geometry_init()` sets them once — a face turn never permutes or reorients a centre.

**Verify empirically, once:** the exact "clockwise order" and which physical rotation direction `twist == 1` corresponds to is exactly the kind of thing `cube/moves.c`'s own header comment warns about ("these values were NOT typed from memory... checked with tests"). Do the same here: render `SOLVED_CUBE`, apply one `R` via `apply_move()` + `geometry_sync()`, and visually confirm the colours land where a real turned cube would show them. If the twist rotation direction is backwards, it is a one-line fix (rotate right instead of left) — cheap to get wrong and cheap to fix, but only if you actually look at the first render instead of assuming.

### 2.5 `app.c` — the loop

```c
bool	render_run(const t_cube *start_cube, bool has_scramble)
{
	t_render_scene	scene;
	t_cube			cube;
	Camera3D		camera;

	InitWindow(1280, 720, "rubik — 3D bonus");
	camera = (Camera3D){ .position = {6, 6, 8}, .target = {0, 0, 0},
		.up = {0, 1, 0}, .fovy = 45, .projection = CAMERA_PERSPECTIVE };
	cube = *start_cube;
	geometry_init(&scene);
	geometry_sync(&scene, &cube);
	while (!WindowShouldClose())
	{
		// Phase 3 adds camera-orbit + keyboard input here.
		// Phase 1 adds anim_update() here.
		BeginDrawing();
		ClearBackground(RAYWHITE);
		BeginMode3D(camera);
		draw_scene(&scene);
		EndMode3D();
		EndDrawing();
	}
	CloseWindow();
	return (true);
}
```

`draw.c`'s `draw_scene()` for this phase: for each of the 26 cubies, draw a `DrawCube` at `(x*SPACING, y*SPACING, z*SPACING)` sized slightly under `SPACING` (so a visible black grid line shows between cubies — free, and looks far better than touching cubes), then `DrawCubeWires` in dark grey for a subtle edge line. Sticker-level colour detail (§4.2 in Phase 5) comes later; for Phase 0, colouring the *whole visible cubie face* with the dominant sticker colour is enough to prove the pipeline.

### 2.6 Exit gate

- `make bonus && ./rubik_bonus -r` opens a window showing a solved cube with correct WCA colours on all 6 faces, matching a real solved cube's layout exactly (spot-check a few slots by eye against §4.2's tables).
- `./rubik_bonus "R U R' U'" -r` opens the same way (Mode A doesn't animate yet — that's Phase 1 — but nothing crashes, and the window at least opens on the *scrambled* starting position once you wire `cube` to reflect the parsed+applied scramble instead of always `SOLVED_CUBE`... actually for Phase 0 it's fine to just show `SOLVED_CUBE` regardless of `has_scramble`; wiring the actual scramble-then-solve-then-play sequence is Phase 1's job).
- Mouse can orbit (stock raylib `UpdateCamera(&camera, CAMERA_ORBITAL)` is fine as a Phase-0 placeholder — Phase 3 replaces it with the custom right-drag-only version for the button split).

**Effort:** ~1 day.

## Phase 1 — Mode A: autoplay of the solver's move list

**Goal:** `./rubik_bonus "<scramble>" -r` solves in-process and animates the solution, each move turning correctly and committing without drift.

**Depends on:** Phase 0.

### 3.1 New files

```
include/render/anim.h   NEW
src/render/anim.c       NEW
```

### 3.2 Data structures

```c
typedef enum e_axis { AXIS_X, AXIS_Y, AXIS_Z }	t_axis;

/// One entry per t_move (18), computed once, by hand, from the axis
/// convention in Phase 0 (+X=R, +Y=U, +Z=F) and each move's suffix
/// (1=90 deg one way, 2=180, 3=90 the other way). `layer` is which slice
/// on that axis this move touches: +1 or -1 (a quarter/half turn of any
/// of the 18 moves always touches exactly one outer layer — never the
/// middle slice, since M/E/S are rejected by parse_notation()).
typedef struct s_move_axis
{
	t_axis	axis;
	int8_t	layer;
	float	quarter_deg;	// signed: the angle ONE 90 deg step of this
							// move covers; sign TBD empirically per axis
							// (see note below), magnitude always 90 or
							// 180 depending on move suffix.
}	t_move_axis;

# define ANIM_QUEUE_CAP MAX_MOVES	// reuse parse.h's own bound — a
									// queued solution can never be longer
									// than the longest input this program
									// accepts anywhere else.

typedef struct s_anim_state
{
	t_move	queue[ANIM_QUEUE_CAP];
	size_t	head;
	size_t	tail;
	bool	active;
	t_move	current;
	float	elapsed_sec;
	float	duration_sec;		// |degrees| / speed_deg_per_sec
	float	angle_deg;			// 0 -> target_deg over duration_sec
	float	target_deg;
	float	speed_deg_per_sec;	// Phase 2 exposes this as the speed dial
	bool	paused;				// Phase 2
}	t_anim_state;

void	anim_init(t_anim_state *s);
bool	anim_push(t_anim_state *s, t_move move);	// false if queue full
void	anim_update(t_anim_state *s, t_render_scene *scene, t_cube *cube,
			float dt);
bool	anim_is_idle(const t_anim_state *s);	// no active move, empty queue
```

### 3.3 The MOVE_AXIS table

```c
static const t_move_axis	MOVE_AXIS[MOVE_COUNT] = {
	[MOVE_U1] = {AXIS_Y,  1,  90}, [MOVE_U2] = {AXIS_Y,  1, 180},
	[MOVE_U3] = {AXIS_Y,  1, -90},
	[MOVE_D1] = {AXIS_Y, -1, -90}, [MOVE_D2] = {AXIS_Y, -1, 180},
	[MOVE_D3] = {AXIS_Y, -1,  90},
	[MOVE_R1] = {AXIS_X,  1, -90}, [MOVE_R2] = {AXIS_X,  1, 180},
	[MOVE_R3] = {AXIS_X,  1,  90},
	[MOVE_L1] = {AXIS_X, -1,  90}, [MOVE_L2] = {AXIS_X, -1, 180},
	[MOVE_L3] = {AXIS_X, -1, -90},
	[MOVE_F1] = {AXIS_Z,  1,  90}, [MOVE_F2] = {AXIS_Z,  1, 180},
	[MOVE_F3] = {AXIS_Z,  1, -90},
	[MOVE_B1] = {AXIS_Z, -1, -90}, [MOVE_B2] = {AXIS_Z, -1, 180},
	[MOVE_B3] = {AXIS_Z, -1,  90},
};
```

**The signed degrees above are a starting guess, not a verified fact** — exactly like `moves.c`'s own `FACE_TABLES`, the actual sign only matters once you look at it turning on screen. The pattern to sanity-check: a face's "1" suffix (clockwise, viewed from outside that face) and its "3" suffix (counter-clockwise) must always be opposite signs of each other for the *same* face, and `U`/`R`/`F`'s clockwise-from-outside direction is *not* automatically the same sign in a shared world-axis convention as `D`/`L`/`B`'s (which is exactly why the table above already flips sign between e.g. `U1`=+90 and `D1`=-90 — U and D spin the "same way" as world-Y rotations only if you're consistent about which face you're looking from). Build it as a best guess from the table above, turn one `R` on screen, and fix any wrong signs — there are at most 6 signs to get right (one per face; `2`-suffix moves are always ±180 either way, so their sign never matters visually).

### 3.4 `anim_update()` algorithm

```
if not active:
    if queue is empty: return
    current = pop_front(queue)
    elapsed_sec = 0
    axis_info = MOVE_AXIS[current]
    target_deg = axis_info.quarter_deg
    duration_sec = |target_deg| / speed_deg_per_sec
    active = true

if active and not paused:
    elapsed_sec += dt
    frac = clamp(elapsed_sec / duration_sec, 0, 1)
    eased = ease_in_out_cubic(frac)       // see below
    angle_deg = target_deg * eased
    if frac >= 1.0:
        apply_move(cube, current)          // THE existing cube.h function
        geometry_sync(scene, cube)         // full resync, see Phase 0 §2.4
        active = false
        angle_deg = 0
```

`ease_in_out_cubic(t)` — a standard, tiny, well-known curve, safe to hardcode:

```c
static float	ease_in_out_cubic(float t)
{
	if (t < 0.5f)
		return (4.0f * t * t * t);
	return (1.0f - powf(-2.0f * t + 2.0f, 3) / 2.0f);
}
```

(This needs `-lm`; `LDLIBS_BONUS` in the Makefile already links `-lm` on Linux — confirm the macOS branch's frameworks also resolve `powf`, which they do via libSystem, so no Makefile change needed either way.)

### 3.5 `draw.c` change — the transient rotation

`draw_scene()` now takes the current `t_anim_state` (or just its `axis`/`layer`/`angle_deg` when active) and, for any cubie whose fixed `(x, y, z)` matches the animating move's `axis`/`layer`, draws it with an *extra* rotation of `angle_deg` around that axis's centreline before the normal per-cubie translation — raylib's `rlPushMatrix()` / `rlRotatef(angle_deg, axis_x, axis_y, axis_z)` / `rlTranslatef(...)` / draw / `rlPopMatrix()` sequence, or equivalently `DrawModelEx` with an explicit rotation axis+angle if you're using loaded models instead of `DrawCube` calls directly. Every other cubie draws exactly as in Phase 0 — untouched.

### 3.6 Wiring `render_run()` for Mode A

When `has_scramble` is true: after `geometry_init`/`geometry_sync`, run the solver in-process exactly like `solve_and_print()` does (reuse `solver_init`/`solve`/`solver_free` from `solve.h` directly — same anti-cheat call shape, just capturing the move array instead of printing it), then `anim_push()` every move in the returned solution, in order, before entering the loop. The loop's body gains one line: `anim_update(&anim, &scene, &cube, GetFrameTime());`.

### 3.7 Exit gate — the drift test from `06-roadmap-bonus.md` Sprint 6

Generate a random 1,000-move sequence, `anim_push()` all of it, let it play at high speed (temporarily crank `speed_deg_per_sec` for this test only) with no user interaction, and assert **byte-for-byte** that the final `cube` (the one `anim_update` has been mutating via `apply_move`) equals what applying the same 1,000 moves directly via `cube_apply_moves()` produces on a fresh `SOLVED_CUBE` copy. Zero floating-point drift is possible here by construction — `angle_deg` is a purely visual, disposable value that's discarded and reset every commit; it never feeds back into `cube`. This test exists to catch a *logic* bug (e.g. wrong axis/layer picked for some move), not the float-drift *arithmetic* bug the roadmap doc originally worried about — worth running anyway, since a wrong `MOVE_AXIS` entry would otherwise stay invisible until someone happens to watch that exact face turn.

**Effort:** ~1.5 days (most of it is the empirical sign-checking in §3.3).

## Phase 2 — Mode B: playback transport (Must)

**Goal:** pause/resume, single-step, and a speed control. Exactly the three Must-have Mode B controls from `03a` §2 — scrubbing, reverse, and auto-loop are Could-tier and land in Phase 7.

**Depends on:** Phase 1.

### 4.1 Changed files

```
include/render/anim.h   CHANGED (no new fields — paused already added
                         in Phase 1's struct; this phase is the first to
                         actually use it)
src/render/anim.c       CHANGED
include/render/hud.h    NEW
src/render/hud.c        NEW
```

### 4.2 Controls

```c
void	anim_toggle_pause(t_anim_state *s);          // s->paused = !s->paused
void	anim_step_one(t_anim_state *s);               // if paused and idle
                                                       // between moves: force
                                                       // one anim_update()
                                                       // worth of progress by
                                                       // temporarily
                                                       // unpausing for exactly
                                                       // the current move's
                                                       // remaining duration
void	anim_set_speed(t_anim_state *s, float deg_per_sec);
```

`anim_step_one()`'s cleanest implementation: don't special-case it at all — just call `anim_update(s, scene, cube, s->duration_sec - s->elapsed_sec + EPSILON)` once (a single, large enough `dt` to guarantee `frac` reaches 1.0 and commits), then immediately re-set `s->paused = true`. This reuses the exact same commit path as normal playback instead of a parallel "instant apply" code path — consistent with the whole plan's "one code path" principle from `03a` §6.

Keybindings (raylib `IsKeyPressed`): `Space` = toggle pause, `Right arrow` = step, `Up`/`Down arrow` = speed +/- (multiply `speed_deg_per_sec` by 1.25/0.8 per press, clamp to a sane range e.g. 60-720 deg/sec).

### 4.3 `hud.c` — baseline HUD

```c
void	hud_draw(const t_anim_state *anim, const t_render_mode *mode,
			int move_count, int total_moves);
```

Draws, via raylib's `DrawText`/`DrawRectangle` (2D overlay, called *outside* `BeginMode3D`/`EndMode3D`, in screen space): current move in notation (look it up via the existing `format_moves()` on a single-move array, or just a small static string table), a move counter (`"move 14 / 42"`), and a simple progress bar (`DrawRectangle` proportional to `move_count/total_moves`). This is intentionally minimal — Phase 4 and Phase 7 add to it.

### 4.4 Exit gate

Play a solve at default speed, pause mid-solve, single-step through 3 moves individually (each one fully completing its turn and committing before the next step-press does anything), resume, change speed twice, let it finish. No crash, no skipped move, no move that visually "jumps" instead of animating when stepped.

**Effort:** ~1 day.

## Phase 3 — Mode C: keyboard manual turns + button-split camera (Must)

**Goal:** the actual "use it like a real cube" ask. Free turning via keyboard, camera orbit that never fights it. This is the phase the user explicitly asked to build first, fully, before any mouse-drag work starts.

**Depends on:** Phase 2 (reuses `anim_push`/`anim_update` unchanged — Mode C's moves enter the exact same queue Mode A's solver output does).

### 5.1 New files

```
include/render/input.h   NEW
src/render/input.c       NEW
```

### 5.2 Data structures

```c
typedef enum e_render_mode
{
	MODE_AUTOPLAY,	// a solver-produced queue is playing or paused
	MODE_MANUAL,	// idle, waiting on keyboard/mouse turns
}	t_render_mode;

typedef struct s_orbit_camera
{
	float	yaw_deg;
	float	pitch_deg;	// clamped to [-80, 80] so the camera can't flip
	float	distance;	// clamped to a sane [4, 20] range
}	t_orbit_camera;

void	orbit_camera_update(t_orbit_camera *orbit, Camera3D *camera,
			float dt);
t_move	input_poll_keyboard(void);	// returns MOVE_COUNT (18, the
									// sentinel "no real move" value) when
									// nothing was pressed this frame
```

### 5.3 Camera orbit — the "right-drag only" half of the button split

```c
void	orbit_camera_update(t_orbit_camera *orbit, Camera3D *camera, float dt)
{
	Vector2	delta;

	if (IsMouseButtonDown(MOUSE_BUTTON_RIGHT))
	{
		delta = GetMouseDelta();
		orbit->yaw_deg -= delta.x * 0.3f;
		orbit->pitch_deg = Clamp(orbit->pitch_deg - delta.y * 0.3f, -80, 80);
	}
	orbit->distance = Clamp(orbit->distance - GetMouseWheelMove(), 4, 20);
	camera->position = (Vector3){
		orbit->distance * cosf(DEG2RAD * orbit->pitch_deg)
			* sinf(DEG2RAD * orbit->yaw_deg),
		orbit->distance * sinf(DEG2RAD * orbit->pitch_deg),
		orbit->distance * cosf(DEG2RAD * orbit->pitch_deg)
			* cosf(DEG2RAD * orbit->yaw_deg),
	};
	camera->target = (Vector3){0, 0, 0};
}
```

This deliberately does **not** use raylib's stock `UpdateCamera(&camera, CAMERA_ORBITAL)` helper (which auto-rotates or binds left-drag depending on mode) — the button split needs the drag gated on the *right* button specifically, so it's a dozen lines of hand-rolled spherical-coordinates math instead. `(void)dt;` for now — not needed until you want frame-rate-independent easing on the orbit itself, which is a nice-to-have, not a requirement.

### 5.4 Keyboard notation input

```c
static const struct { KeyboardKey key; t_move face_base; } FACE_KEYS[6] = {
	{KEY_U, MOVE_U1}, {KEY_R, MOVE_R1}, {KEY_F, MOVE_F1},
	{KEY_D, MOVE_D1}, {KEY_L, MOVE_L1}, {KEY_B, MOVE_B1},
};

t_move	input_poll_keyboard(void)
{
	int	i;

	i = 0;
	while (i < 6)
	{
		if (IsKeyPressed(FACE_KEYS[i].key))
		{
			if (IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT))
				return (FACE_KEYS[i].face_base + 2);	// ' — ccw
			if (IsKeyDown(KEY_TWO))
				return (FACE_KEYS[i].face_base + 1);	// 2 — half turn
			return (FACE_KEYS[i].face_base);			// plain — cw
		}
		i++;
	}
	return (MOVE_COUNT);
}
```

Chording note worth testing by feel once it's running: holding `2` and then pressing a face key works cleanly with `IsKeyDown` since `2` is checked as "currently held," but two face keys pressed in the same frame only honours the first in `FACE_KEYS` order — acceptable, a real cube doesn't let you turn two faces at once either.

### 5.5 The mode state machine, in `app.c`'s loop

```
each frame:
    orbit_camera_update(&orbit, &camera, dt)
    if mode == MODE_AUTOPLAY:
        anim_update(&anim, &scene, &cube, dt)
        if IsKeyPressed(KEY_SPACE): anim_toggle_pause(&anim)
        if IsKeyPressed(KEY_ESCAPE) and anim not idle:
            flush the queue (head = tail = 0), let the in-flight move
            finish committing on its own, then mode = MODE_MANUAL
    else if mode == MODE_MANUAL:
        move = input_poll_keyboard()
        if move != MOVE_COUNT and anim_is_idle(&anim):
            anim_push(&anim, move)
        anim_update(&anim, &scene, &cube, dt)   // drains the 1-deep queue
```

Manual turns go through **the exact same `anim_push`/`anim_update`/`apply_move` pipeline** as autoplay — this is the single most important sentence in this whole phase. There is no second "apply this turn directly, no animation" code path anywhere. That's what makes a manual scramble trustworthy input to the solver later (Phase 4's "solve for me"), and it's a direct continuation of the anti-cheat principle already in `01-requirements.md` and `04-architecture.md`.

### 5.6 Exit gate — the anti-cheat consistency test, exercised from the keyboard

With the window open in `MODE_MANUAL`, physically press ~25 random face keys (mix of plain/shift/2), then dump the resulting `cube` (a debug `printf` of `format_moves` isn't applicable here since there's no move list — print the raw `t_cube` struct, or temporarily wire a hidden debug key that runs it through `solve_and_print()` in-process and prints to stdout/stderr while the window stays open). Confirm the solver returns a valid solution and that solution, replayed, produces `SOLVED_CUBE`. This is the exact test `06-roadmap-bonus.md` Sprint 6 already specifies as a gate — Phase 3 is where it's actually exercised from real keyboard input instead of a fixed test scramble.

**Effort:** ~1.5 days.


## Phase 4 — Practice-mode extras: scramble, timer, counter, undo/redo, "solve for me" (Should)

**Goal:** the quality-of-life layer that makes Mode C feel like a finished feature instead of a tech demo. Five small, independent additions.

**Depends on:** Phase 3.

### 6.1 New files

```
include/scramble.h      NEW — top-level, not render-only: this is a
                         generally useful module (it's also exactly the
                         "integrated scramble generator" named as its own
                         item in 06-roadmap-bonus.md Sprint 7 #2), so it
                         does not live under include/render/.
src/cube/scramble.c     NEW
include/render/history.h NEW
src/render/history.c    NEW
```

### 6.2 Scramble generator

```c
/// @brief Fills `out[0..count)` with a random legal move sequence, no two
///        consecutive moves on the same face (the one quality rule that
///        matters for "does this look like a real scramble" — WCA-grade
///        scramblers do more, but this project doesn't need that bar).
///
/// @param out   Buffer of at least `count` moves.
/// @param count How many moves to generate.
/// @param seed  In/out PRNG state (caller owns it — seed once from
///              time(NULL) at program start, not per call).
void	scramble_generate(t_move *out, size_t count, unsigned int *seed);
```

Algorithm: `rand_r(seed) % MOVE_COUNT` for a candidate; if `candidate / 3 == previous / 3` (same face as the last move), re-roll. Nothing more is needed — this is intentionally the simple version, not a WCA-compliant scrambler.

In Mode C, a "scramble" keypress (e.g. `S`) calls `scramble_generate()` for a fixed length (20 is a common default), then `anim_push()`s all of them and switches to `MODE_AUTOPLAY` just long enough to visually play the scramble in — reusing the exact same playback path as a solve, which is free once Phase 1-2 exist. Once the queue drains, drop back to `MODE_MANUAL`.

### 6.3 Timer, move counter, TPS

Three plain fields added to whatever top-level state `app.c` already owns (not `t_anim_state` — these are session stats, not animation state):

```c
typedef struct s_session_stats
{
	double	timer_start_sec;	// GetTime() when timing started; -1 if idle
	int		move_count;
	bool	timing;
}	t_session_stats;
```

Start timing on the first manual move after a scramble (i.e. the first `anim_push` call in `MODE_MANUAL` after a scramble completes), stop it the moment `cube_is_solved(&cube)` becomes true (checked once per frame in `MODE_MANUAL`, it's an O(1) struct comparison, free to call every frame). TPS is just `move_count / (GetTime() - timer_start_sec)`, computed for display only, never stored.

### 6.4 Undo / redo

```c
# define HISTORY_CAP MAX_MOVES

typedef struct s_history
{
	t_move	undo_stack[HISTORY_CAP];
	size_t	undo_top;
	t_move	redo_stack[HISTORY_CAP];
	size_t	redo_top;
}	t_history;

/// @brief The exact inverse of one move — derived directly from the
///        MOVE_U1/U2/U3 enum layout (face * 3 + turn, turn 0=cw,1=180,
///        2=ccw): inverting just flips cw<->ccw and leaves 180 alone.
static inline t_move	move_inverse(t_move m)
{
	return ((t_move)((m / 3) * 3 + (2 - m % 3)));
}

void	history_record(t_history *h, t_move applied);	// push applied
														// onto undo_stack,
														// clear redo_stack
bool	history_undo(t_history *h, t_move *out);		// pop undo_stack,
														// push its inverse
														// onto redo_stack,
														// *out = inverse to
														// anim_push()
bool	history_redo(t_history *h, t_move *out);		// pop redo_stack,
														// push back onto
														// undo_stack,
														// *out = the
														// original move
```

Call `history_record()` at the exact point `MODE_MANUAL` calls `anim_push()` for a keyboard-driven move (not for autoplay/scramble moves — undo is a Mode C concept only). `Ctrl+Z` / `Ctrl+Y` (or `Z`/`Y` plain, your call) drive `history_undo`/`history_redo`, each pushing the resulting move through the normal `anim_push()` path — once again, no second "apply directly" code path.

### 6.5 "Solve for me"

```c
/// @brief Runs the existing in-process solver against the CURRENT live
///        cube and queues its output for autoplay. No-op if already
///        solved. Mirrors solve_and_print()'s pipeline exactly, minus
///        the printf.
static void	solve_for_me(t_cube *cube, t_anim_state *anim,
				t_render_mode *mode)
{
	t_solver	solver;
	t_move		solution[SOLVE_MAX_MOVES];
	int			count;
	int			i;

	if (cube_is_solved(cube))
		return ;
	if (!solver_init(&solver))
		return ;
	count = solve(&solver, cube, solution);
	solver_free(&solver);
	if (count < 0)
		return ;
	i = 0;
	while (i < count)
		anim_push(anim, solution[i++]);
	*mode = MODE_AUTOPLAY;
}
```

Bound to a dedicated key (e.g. `Enter`). Note this calls `solve()` on the *live, currently-displayed* `cube` — the same variable Mode C's keyboard/undo/scramble code has been mutating via `apply_move()` all along, never a separately-tracked "what the user thinks the state is" copy. That single shared `t_cube` is what makes this button trustworthy.

### 6.6 Exit gate

Scramble via the `S` key, watch the move counter and timer start, solve half of it by hand, undo 3 moves, redo 1, then hit "solve for me" and watch it finish and the timer stop the instant the cube reads solved. No desync between the HUD's move counter and the actual number of committed moves.

**Effort:** ~2.5 days combined (scramble ~0.5, timer/counter ~0.5, undo/redo ~0.5, solve-for-me ~0.5, HUD wiring for all of it ~0.5).

## Phase 5 — Visual-polish baseline: lighting + sticker gaps (Should)

**Goal:** the two changes `03a` §4.1-4.2 flags as the highest visual value per hour. Purely a `draw.c` phase — no gameplay/logic files change.

**Depends on:** Phase 0 (geometry/draw only; independent of Phases 1-4, can be built in parallel by the other dev).

### 7.1 Lighting shader

raylib ships a working example of exactly this under its own `examples/shaders/` folder (`rlights.h` plus a `lighting.vs`/`lighting.fs` GLSL pair) — **locate the current filenames in the vendored `lib/raylib/examples/shaders/` tree** (exact paths can shift slightly between raylib versions) and copy the three files into a new `assets/shaders/` directory in this repo, rather than reaching into `lib/`'s own example tree at runtime. Load once in `render_run()`'s setup:

```c
Shader	lighting = LoadShader("assets/shaders/lighting.vs",
					"assets/shaders/lighting.fs");
// rlights.h's CreateLight(...) for 1-2 lights, per its own example code.
```

then set every cubie's material to use `lighting` instead of the default unlit material before the draw loop starts. This is copy-adapt work from raylib's own example, not new algorithm design — budget time for reading that example once, not for inventing shader code from scratch.

### 7.2 Sticker gaps / plastic frame

Replace the single `DrawCube` per cubie (Phase 0's placeholder) with two draws:

1. One slightly-larger dark-grey/black `DrawCube` — the "plastic body."
2. For each of the cubie's populated `face[]` entries (i.e. not the internal/hidden directions), one thin coloured quad — either `DrawCubeV` with a near-zero depth (e.g. `0.85 * cubie_size` wide/tall, `0.02 * cubie_size` deep) positioned at the face centre offset outward by half the body's size plus a small epsilon (to avoid z-fighting), or a raw `DrawTriangleStrip3D` quad if you want to avoid the depth dimension entirely. Either is fine; the cube version is less code.

This is purely additive to `draw_scene()` — no other file changes.

### 7.3 Exit gate

Visual only: the rendered cube should read as "a real plastic cube with lighting" rather than "flat coloured boxes," side by side with a Phase-0 screenshot for comparison. No functional test — this phase can't break anything logic-side since it never touches `t_cube`, `t_anim_state`, or `t_render_scene`'s `x/y/z` fields, only how `face[]` colours get drawn.

**Effort:** ~1 day (half lighting, half sticker gaps).

## Phase 6 — Mouse click-and-drag turning (Could)

**Goal:** completes the button split. Left-drag on a sticker turns its layer; right-drag (already built in Phase 3) still orbits. Only start this once Phase 3's keyboard path has passed its gate — per the user's explicit sequencing decision in §0 above.

**Depends on:** Phase 3.

### 8.1 Changed/new files

```
include/render/input.h   CHANGED — add the drag-state API below
src/render/input.c       CHANGED
```

### 8.2 Data structures

```c
typedef struct s_drag_state
{
	bool	active;
	t_axis	axis;
	int8_t	layer;
	Vector3	plane_right;	// unit vectors spanning the clicked face's
	Vector3	plane_up;		// local plane, in world space
	float	accum_deg;		// live, uncommitted rotation while dragging
}	t_drag_state;

bool	input_pick_start(t_drag_state *drag, const t_render_scene *scene,
			Camera3D camera);				// call on left-mouse-down
void	input_pick_drag(t_drag_state *drag, Vector2 mouse_delta);
t_move	input_pick_release(t_drag_state *drag);	// returns MOVE_COUNT
													// if the drag was too
													// small to count as a
													// turn (treat as a
													// no-op click)
```

### 8.3 `input_pick_start()` — raycasting + face identification

1. `Ray ray = GetMouseRay(GetMousePosition(), camera);`
2. For each of the 26 cubies, build its `BoundingBox` from `(x, y, z) * SPACING ± half_size` and call `GetRayCollisionBox(ray, box)`. Track the closest hit (`RayCollision.distance`).
3. On the closest hit, `RayCollision.normal` gives the world-space face direction that was clicked (one of the 6 unit axis directions). That direction, plus the cubie's fixed `(x, y, z)`, determines `axis` (the axis perpendicular to the clicked face — a click on `FACE_RIGHT`/`FACE_LEFT` implies the drag axis is either Y or Z, decided in the *next* step, not this one) and confirms which `layer` on that plane the eventual turn would touch (the coordinate value shared by the whole clicked face — e.g. clicking anywhere on a cubie sitting at `x=1` with normal `+X` means the turn, whatever it turns out to be, touches `layer = 1` on whichever of Y/Z axis the drag resolves to).
4. Compute `plane_right`/`plane_up`: two world-space unit vectors spanning the clicked face's plane (e.g. for a `+X`-normal face, `plane_right = (0,0,1)` and `plane_up = (0,1,0)` under this doc's axis convention). These are what the next step projects the drag onto.

### 8.4 `input_pick_drag()` — resolving the turn axis from drag direction

Each frame while the left button is held:

1. Project `mouse_delta` (2D screen space) onto `plane_right` and `plane_up` — cheapest correct way: use `GetWorldToScreen` on two points (`cubie_center` and `cubie_center + plane_right`) once at drag-start to get a 2D screen-space direction for `plane_right`, likewise for `plane_up`, then dot the raw 2D `mouse_delta` against those two 2D directions to get how much of the drag is "along right" vs "along up."
2. Whichever of the two has the larger magnitude decides the rotation axis: a drag mostly along `plane_up` rotates around the axis defined by `plane_right` (and vice versa) — this is the standard "drag perpendicular to the axis you're rotating around" relationship, the same one a real cube's geometry enforces.
3. Accumulate `accum_deg += chosen_component * DRAG_SENSITIVITY` (a tuned constant, start around `0.5` and adjust by feel).

### 8.5 `input_pick_release()` — snap and resolve to a real move

1. Snap `accum_deg` to the nearest of `{-180, -90, 0, 90, 180}`.
2. If the snapped value is `0`, return `MOVE_COUNT` (too small a drag — treat as a non-event, e.g. the user just clicked without meaningfully dragging).
3. Otherwise, look up which `t_move` has that exact `(axis, layer, quarter_deg)` combination in `MOVE_AXIS` (a linear scan of 18 entries is plenty fast) and return it.
4. `app.c`'s loop calls this on mouse-up; if the result isn't `MOVE_COUNT`, it goes through **the same `anim_push()` path everything else uses** — the drag itself was purely visual bookkeeping in `t_drag_state`, never touching `cube` or `scene` directly.

### 8.6 The one integration point in `draw.c`

While `drag.active` is true, `draw_scene()` needs to show the affected layer visually following the drag *before* it's committed — reuse the exact transient-rotation mechanism from Phase 1 §3.5 (which already knows how to draw one layer rotated by an arbitrary angle around an axis), just fed `drag.accum_deg` instead of `anim.angle_deg`. No new drawing code — one extra `if` branch picking which of the two live values to use.

### 8.7 Exit gate

Click-drag every one of the 6 faces from a few different camera angles (including from "behind," where the drag direction that means clockwise flips) and confirm each produces the intended move — this is inherently a by-feel calibration pass, not something a unit test can fully cover; budget real interactive testing time, not just implementation time. Once it feels right, re-run Phase 3's anti-cheat consistency test (§5.6) but scramble by mouse-drag instead of keyboard, to confirm mouse-driven turns are exactly as trustworthy as keyboard-driven ones.

**Effort:** ~2.5-3 days (the calibration/feel pass is the long pole, not the code).

## Phase 7 — Remaining Could-have polish

**Goal:** everything else in `03a`'s MoSCoW "Could" tier. Each of these is independent of the others — build in any order, drop any one under time pressure without affecting the rest.

**Depends on:** Phase 4 (timeline/reverse/auto-loop reuse `solve_for_me`'s solution-array pattern and `history`'s `move_inverse()`); the rest depend only on Phase 0/5.

### 9.1 Camera presets + idle auto-orbit

Three or four hardcoded `t_orbit_camera` constants (e.g. `{yaw: 45, pitch: 30, distance: 8}` for a standard iso view) bound to number keys, each lerped into over ~0.3s rather than snapped (reuse `ease_in_out_cubic` from Phase 1). Idle auto-orbit: if no keyboard/mouse input for N seconds (track `last_input_time = GetTime()`, updated anywhere input is detected), slowly increment `orbit.yaw_deg` by a small constant per frame until input resumes.

**Files:** `input.c`/`app.c` only. **Effort:** ~0.5 day.

### 9.2 Colour themes / skins

```c
typedef struct s_palette { Color u, d, f, b, r, l; } t_palette;
static const t_palette PALETTE_CLASSIC = { WHITE_C, YELLOW_C, GREEN_C, BLUE_C, RED_C, ORANGE_C };
static const t_palette PALETTE_COLORBLIND_SAFE = { /* swap red/green for
	something more distinguishable, e.g. a blue/orange-heavy scheme */ };
```

`geometry.c`'s `CORNER_COLORS`/`EDGE_COLORS` tables (§4.2) become generated from whichever `t_palette` is active, rather than hardcoded literals — one small refactor (build the tables from the 6 base colours via the same U/R/F/D/L/B-letter-per-name logic already described, instead of writing them out literally), then a hotkey cycles `active_palette` and calls `geometry_sync()` once to repaint.

**Files:** `geometry.c`/`geometry.h`. **Effort:** ~0.5 day.

### 9.3 Turn sound + solve-complete celebration ("juice")

```c
include/render/fx.h   NEW
src/render/fx.c        NEW
```

`InitAudioDevice()` once at startup, `LoadSound("assets/sfx/turn.wav")` (any short click/whoosh — record one or grab a CC0 one), `PlaySound()` on every commit inside `anim_update()`'s commit branch (Phase 1 §3.4 — one extra line there). Celebration: a small fixed-size array of "particles" (`Vector3 pos, vel; float life;`), spawned with random outward velocities the instant `cube_is_solved()` flips true, each drawn as a shrinking `DrawSphere` and updated (`pos += vel * dt; life -= dt`) until `life <= 0`.

**Effort:** ~0.5 day.

### 9.4 Rounded-corner cubie geometry

The cheapest version that still reads well: keep the "plastic body + inset sticker quads" approach from Phase 5, but round the *body* cube's corners via a small pre-baked mesh (a beveled cube, generated once — e.g. with a script using raylib's `GenMeshCube` as a base and manually offsetting corner vertices, or an external tool exporting a `.obj`/`.glb` loaded once with `LoadModel`) instead of trying to fake rounding with textures. This is the most time-variable item on the list — treat the estimate as soft.

**Effort:** ~1 day, could run over.

### 9.5 Ground shadow / skybox

Ground shadow: one semi-transparent dark `DrawCircle3D` (or a flattened `DrawCube`) at `y = -SPACING*1.6` under the cube, drawn before the cube itself. Skybox: simplest version is a large inward-facing sphere (`DrawSphere` with a flag/trick to render its inside, or just a `ClearBackground` vertical gradient faked via a full-screen quad drawn first in 2D) — the gradient version is far cheaper than a real textured skybox and looks fine here.

**Effort:** ~0.5 day.

### 9.6 Move/timeline scrubbing (non-draggable — buttons/text box only)

The "jump to move N" version, explicitly **not** the draggable-handle version (that one's in the Won't tier). Requires the autoplay solution array to survive past the point it's queued (currently `solve_for_me`/`render_run`'s Mode-A setup pushes moves into `anim`'s queue and could discard the array — keep it around instead, plus a saved copy of the cube state at the start of the solve):

```c
t_move	solution_moves[SOLVE_MAX_MOVES];
int		solution_count;
t_cube	solution_start_cube;	// snapshot taken once, before move 0 plays
```

"Jump to move N": reset `cube = solution_start_cube`, replay `solution_moves[0..N)` instantly via a tight `apply_move()` loop (no animation), call `geometry_sync()` once at the end, flush `anim`'s queue and push `solution_moves[N..solution_count)` so playback resumes from there normally. **Reverse playback**: push `move_inverse(solution_moves[i])` (the exact same helper from Phase 4 §6.4) for `i` from `solution_count-1` down to `0`. **Auto-loop**: when `MODE_AUTOPLAY`'s queue drains and `cube_is_solved()`, wait ~2 seconds (track a timer), then call `scramble_generate()` + `solve_for_me()`-style solve-and-queue again.

**Files:** `app.c` mainly, small `hud.c` addition for the "jump to move N" UI (a text box or +10/-10 buttons, per `03a` §2's recommendation against a draggable handle). **Effort:** ~1 day.

## 10. Consolidated file manifest

```
include/render.h                NEW  Phase 0  raylib-agnostic entry point
include/render/app.h            NEW  Phase 0
include/render/geometry.h       NEW  Phase 0
include/render/draw.h           NEW  Phase 0
include/render/anim.h           NEW  Phase 1
include/render/hud.h            NEW  Phase 2
include/render/input.h          NEW  Phase 3  (extended Phase 6)
include/scramble.h              NEW  Phase 4  (top-level, shared)
include/render/history.h        NEW  Phase 4
include/render/fx.h             NEW  Phase 7

src/render/app.c                NEW  Phase 0  (changed every later phase)
src/render/geometry.c           NEW  Phase 0  (extended Phase 7 §9.2)
src/render/draw.c               NEW  Phase 0  (changed Phases 1,5,6)
src/render/anim.c               NEW  Phase 1
src/render/hud.c                NEW  Phase 2  (extended Phases 4,7)
src/render/input.c              NEW  Phase 3  (extended Phase 6)
src/cube/scramble.c             NEW  Phase 4  (top-level, shared)
src/render/history.c            NEW  Phase 4
src/render/fx.c                 NEW  Phase 7

src/main.c                      CHANGED Phase 0 only (-r flag, optional
                                 scramble, one new call to render_run())

assets/shaders/{lighting.vs,lighting.fs,rlights.h}   Phase 5 (copied from
                                 raylib's own examples)
assets/sfx/turn.wav             Phase 7 (any short sound, CC0 or recorded)
```

**Makefile impact: none, for every phase.** `src/render/*.c` is auto-discovered by the existing `RENDER_SRCS := $(shell find $(RENDER_DIR) -name '*.c' ...)` rule — a new file under `src/render/` needs zero Makefile edits, exactly as the Makefile's own header comment promises. `src/cube/scramble.c` is equally auto-discovered by the mandatory `SRCS` rule, which is fine — it's a general-purpose module, not render-only, and being in the mandatory binary too costs nothing (it's just never called from `main()`'s mandatory path). The only line that ever needs a human hand is `src/main.c` itself, once, in Phase 0.

## 11. Total effort estimate

| Phase | Tier | Effort |
|---|---|---|
| 0 — Render skeleton | prerequisite | ~1 day |
| 1 — Mode A autoplay | Must | ~1.5 days |
| 2 — Mode B transport | Must | ~1 day |
| 3 — Mode C keyboard | Must | ~1.5 days |
| 4 — Practice extras | Should | ~2.5 days |
| 5 — Visual baseline | Should | ~1 day |
| 6 — Mouse drag | Could | ~2.5-3 days |
| 7 — Remaining polish | Could | ~3.5 days (sum of §9.1-9.6) |
| **Total** | | **~14.5-15 days** for two people, most of it parallelizable along the same `anim.c`/`geometry.c`+`draw.c` seam `06-roadmap-bonus.md` Sprint 6 already defines. |

Phases 0-3 (the Must tier, ~5 days) are the part that must exist for the bonus to count at all and for "usable like a real cube" to be true. Phases 4-5 (~3.5 days) are worth doing if Sprint 7's time budget allows. Phases 6-7 (~6-6.5 days) are genuinely bonus-on-the-bonus — the MoSCoW ordering within them (drag-turning before pure polish) still applies if only some of Phase 6-7 ends up fitting.
