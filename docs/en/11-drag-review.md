# 11 — Click-and-drag turning: review & fix spec

**Date:** 2026-09-28 · **Scope:** `src/render/input.c`, `include/render/input.h`, the drag path in `src/render/app.c`, `anim_move_for_turn()` / release hand-off in `src/render/anim.c`, and the per-frame cost of `src/render/draw.c` (it sets how smooth a drag feels).
**Reviewer lens:** 3D interaction / raylib. Everything below was read against the vendored raylib (`6.1-dev`) source, and the sign analysis was checked numerically (script output reproduced in §1.1).

---

## 0. TL;DR

Both symptoms you reported are real bugs with clear root causes — the design in §8 of `03b` is close, it just lost two pieces of information on the way to code.

| ID | Sev | Symptom | Root cause (one line) |
|----|-----|---------|-----------------------|
| B1 | BLOCKER | Turns go the wrong way | Rotation **sign is discarded**: `vector_to_axis()` keeps only `|axis|`, and the horizontal-drag case needs a minus sign it never gets. 9 of 12 face/direction cases are inverted. |
| B2 | BLOCKER | Layer flickers between row/column, "moves multiple parts" | Axis + layer are **re-chosen every frame** from that frame's delta, while `accum_deg` keeps summing both. A still frame (delta 0,0) always flips to the "horizontal" axis — including the frame before release, so the committed move can be the wrong layer. |
| B3 | BLOCKER (multiplier) | Makes B2 much worse | **No frame cap / vsync**. At hundreds of FPS each frame's delta is 0 or ±1 px, so a diagonal-ish drag alternates `(1,0)`,`(0,1)` → axis flips nearly every frame. |
| B4 | BLOCKER | Glitch "when the mouse is near other parts of the cube" | Picking tests 26 separate boxes with 0.11-unit **gaps between them**. A click on a seam passes through and hits the **inner side face of a neighbour cubie** → wrong normal → wrong drag plane → wrong layer/axis. |
| B5 | SHOULD-FIX | Layer jumps back then replays on release | On release the drag angle is dropped and the queued move re-animates **from 0°**. A cancelled drag teleports back to 0°. |
| B6 | SHOULD-FIX | Some half-turn drags silently do nothing | `anim_move_for_turn()` matches `±180` exactly; `U2` is only `-180`, `D2` only `+180`, so dragging the "other" way 180° finds no move. |
| S1 | SHOULD-FIX | Dragging some stickers turns the middle, then it snaps back | Any drag resolving to `layer == 0` (all centre stickers, half of edge-sticker directions) is animated live but has no `t_move` → silently discarded. |
| S2 | SHOULD-FIX | Layer keeps following the mouse with the button up | Release is edge-detected only (`IsMouseButtonReleased`). Pressing `A` mid-drag, alt-tabbing, or any frame where manual tick doesn't run loses the edge → **stuck drag**. |
| S3 | SHOULD-FIX | Drag direction goes wrong after camera moves | Right-drag orbit, presets `5-8` and preset lerps still run mid-drag, but `screen_right/up` were cached at drag-start. |
| S4 | SHOULD-FIX | Feel changes with zoom/window size; edge-on faces go crazy | Fixed `0.5 deg/px`; screen vectors are normalised, throwing away the px-per-world-unit scale. |
| S5 | SHOULD-FIX | Needs a 45°+ drag to commit; flicks don't register | Snap = nearest multiple of 90, no velocity term. |
| P1 | SHOULD-FIX (perf) | — | `BeginShaderMode`/`EndShaderMode` **per cubie** → ~52 batch flushes/draw calls per frame. |
| P2 | SHOULD-FIX (perf) | Frame drops with rounded corners on | 208 × `DrawSphere` (16×16) ≈ **320k immediate-mode vertices/frame**, built on the CPU. |

**Memory / leaks:** none in the drag path — it allocates nothing, and every raylib resource it touches (`Camera3D`, `Ray`, `BoundingBox`) is a value type. Details in §4.

**Recommended order:** B1 → B2+B3+B4 (they're one rewrite of `input_pick_*`, §5) → S2/S3 (app.c guards) → B5/B6 (anim hand-off) → S1 policy → P1/P2.

---

## 1. Blockers

### 1.1 B1 — Rotation direction inverted (sign dropped)

**Where:** `input.c` `input_pick_drag()` lines ~260-276, `vector_to_axis()`.

**What the code does:**
- vertical drag (along `plane_up`) → axis = `|plane_right|`, angle `+= along_up`
- horizontal drag (along `plane_right`) → axis = `|plane_up|`, angle `+= along_right`

**What the physics says:** a surface point on a face with outward normal **n**, dragged along world direction **d**, is moved by a rotation about **a = n × d** with a *positive* right-handed angle (because `v = a × p` and `p ≈ n`). Worked out:
- drag along `up` → `n × up = +right` ✔ axis right, but the **sign of `right`'s component** must multiply the angle.
- drag along `right` → `n × right = −up` ✘ the code is missing this minus sign **everywhere**, and also drops `up`'s own sign.

`vector_to_axis()` returns `AXIS_X` for both `+X` and `−X`, so the sign is lost before it ever reaches `accum_deg`.

**Numerical check** (same `face_plane_vectors()` logic, compared against `n × d`):

| Face | plane_right | plane_up | drag along up | drag along right |
|------|-------------|----------|---------------|------------------|
| +X (R) | +Z | +Y | OK | **INVERTED** |
| −X (L) | −Z | +Y | **INVERTED** | **INVERTED** |
| +Y (U) | −Z | +X | **INVERTED** | **INVERTED** |
| −Y (D) | +Z | +X | OK | **INVERTED** |
| +Z (F) | −X | +Y | **INVERTED** | **INVERTED** |
| −Z (B) | +X | +Y | OK | **INVERTED** |

The default camera (yaw 40°, pitch 25°) shows F, R and U — the two faces users touch most (F, U) are inverted in *both* directions. Concrete repro: default view, grab a front sticker in the right column, drag **up** → you get `R'`, a real cube gives `R`.

**Fix:** stop splitting into "right/up" special cases. At axis-lock time compute the rotation axis vector directly and keep its sign:

```c
rot = Vector3CrossProduct(drag->normal, drag->plane_dir[chosen]); // a = n x d
drag->axis  = vector_to_axis(rot);
drag->sign  = (axis_component(rot, drag->axis) > 0.0f) ? 1.0f : -1.0f;
drag->layer = drag->cubie_pos[drag->axis];
...
drag->accum_deg = drag->sign * world_dist / DRAG_RADIUS * RAD2DEG;
```

Full version in §5.

### 1.2 B2 — Axis/layer re-decided every frame

**Where:** `input_pick_drag()`.

```c
if (fabsf(along_up) > fabsf(along_right)) { accum += along_up;    chosen = right; }
else                                     { accum += along_right; chosen = up;    }
drag->axis  = vector_to_axis(chosen);   // every frame
drag->layer = drag->cubie_pos[drag->axis];
```

Three failure modes stack:
1. **Flip-flop.** Any drag that isn't perfectly aligned alternates which branch wins. The drawn layer jumps between a row and a column *with the same accumulated angle* — exactly "trying to move multiple parts at once".
2. **Mixed accumulator.** `accum_deg` sums components from both axes, so the angle shown on the current axis includes motion that belonged to the other one.
3. **Zero-delta frame = horizontal.** When the mouse is still, `0 > 0` is false → the `else` branch → axis becomes `|plane_up|`. Hold still for one frame before letting go (everyone does) and a vertical drag **commits a move on the wrong axis/layer**.

**Fix (standard pattern):**
- Measure **total displacement from the press point** (`GetMousePosition() - start_mouse`), never per-frame deltas. Robust to frame rate and dropped frames.
- **Dead zone** (~8 px) before deciding anything; below it, draw nothing rotating.
- **Lock** axis/layer/sign once, the first frame the dead zone is crossed. Never change it again during that drag.

### 1.3 B3 — Uncapped frame rate quantises the input

`render_run()` never calls `SetTargetFPS()` or `SetConfigFlags(FLAG_VSYNC_HINT)`. On Linux/XQuartz that means an uncapped loop. `GetMouseDelta()` is integer pixels, so at high FPS most frames report `(0,0)`, `(1,0)` or `(0,1)` — the worst possible input for the per-frame dominance test in B2. It also pins a CPU core and the GPU for no benefit.

**Fix (app.c):**
```c
SetConfigFlags(FLAG_VSYNC_HINT | FLAG_MSAA_4X_HINT); // before InitWindow
InitWindow(WINDOW_WIDTH, WINDOW_HEIGHT, "rubik -- 3D bonus");
SetTargetFPS(60);  // or GetMonitorRefreshRate(GetCurrentMonitor()) if > 0
```
MSAA is optional (it sharpens the 0.11 seams nicely) — test it under XQuartz/GL 2.1 before keeping it. Once B2 is fixed by using absolute displacement, the cap is no longer a correctness fix, only a smoothness/CPU one.

### 1.4 B4 — Seam clicks hit interior faces

**Where:** `input_pick_start()`.

Each cubie box is `0.94` wide on a `1.05` grid, leaving a `0.11` gap. A ray through that gap misses the sticker you were aiming at and hits the **side face of the neighbouring cubie, inside the puzzle** (e.g. normal `−X` while you clicked the front face). `face_plane_vectors()` then builds the drag plane for a face that isn't visible, so both the axis and the direction are nonsense. Closer to edges and at grazing camera angles the gap covers more screen pixels, which is why it feels worse "near other parts of the cube".

Second, smaller problem: `GetRayCollisionBox()` computes its normal by scaling and truncating `(point - centre)` to int. Exactly at a box edge/corner it can return two non-zero components.

**Fix:** raycast against **one box for the whole puzzle**, then derive the sticker from the hit point:
- half extent `H = CUBIE_SPACING + CUBIE_BODY_SIZE / 2` (= 1.52)
- normal axis = component of `hit.point` with the largest `|p|`; sign from that component
- the two other slot coordinates = `clamp(roundf(p / CUBIE_SPACING), -1, 1)`

Seam clicks now resolve to the nearest real sticker, interior faces can never be picked, and it's 1 box test instead of 26. The picking function no longer needs `scene` at all.

---

## 2. Should-fix (interaction)

### 2.1 B5 — Release rewinds the layer

On release, `input_pick_release()` sets `drag->active = false` and pushes a move. Next draw, `input_drag_get_active_turn()` is inactive and the anim starts that move at `angle_deg = 0` → the layer **jumps back** from wherever the finger left it (say 70°), then animates 0→90 again. A cancelled drag (snap to 0) just teleports back.

**Fix:** let the anim start from the drag angle.
- `anim_begin_from(state, move, from_deg, to_deg)` — starts the move immediately (release only happens while anim is idle, see `tick_manual()`), animating `from → to`, duration `max(|to - from| / speed, 0.06 s)`.
- `anim_begin_settle(state, axis, layer, from_deg)` — same, `to = 0`, **commits nothing** (current = `MOVE_COUNT` sentinel; `anim_update()` skips `apply_move()`/`fx_play_turn()` for it; `anim_get_active_turn()` reads the stored axis/layer instead of `MOVE_AXIS[current]`).
- `angle_deg = from + (to - from) * ease(frac)`; `from = 0` for normal queued moves, so autoplay is unchanged.
- Use **ease-out** (`1 - (1-t)^3`) for these release animations, not ease-in-out: the layer is already moving when you let go, and ease-in-out makes it visibly stall and restart.

### 2.2 B6 — Half-turns only work in one direction

`MOVE_AXIS` stores `U2` as `-180` and `D2` as `+180`. Drag the U layer to `+180` → no exact match → `MOVE_COUNT` → nothing happens (and with B5 it rewinds).

**Fix:** in `anim_move_for_turn()` treat `±180` as equal:
```c
if (MOVE_AXIS[i].axis == axis && MOVE_AXIS[i].layer == layer
    && (MOVE_AXIS[i].quarter_deg == quarter_deg
        || (fabsf(quarter_deg) == 180.0f
            && fabsf(MOVE_AXIS[i].quarter_deg) == 180.0f)))
```
and animate to the **signed** `to_deg` the user dragged toward (`anim_begin_from` takes an explicit target), so the layer doesn't reverse direction on release. The committed state is identical either way.

### 2.3 S1 — Middle-slice drags are animated then discarded

The drag layer is `cubie_pos[axis]`. For a **centre sticker** both in-plane coordinates are 0, so *every* drag direction is a slice (M/E/S). For an **edge sticker** one of the two directions is a slice. The move set has no slice moves (and the cubie model has fixed centres), so the release finds no move — but during the drag the middle layer was drawn turning. This reads as "it's broken".

Options:

| Option | Effort | UX | Notes |
|--------|--------|----|-------|
| **A. Block with rubber-band** (recommended) | ~20 lines | Clear "not allowed" feel | When `layer == 0` at lock time, draw `accum = 12° · tanh(raw / 12°)` and always settle to 0. Honest, no model change. |
| B. Map slice → two outer turns | Medium | Outer layers turn instead of the middle one — surprising | `M` ≡ `R L'` up to a whole-cube rotation (equivalent for the solver since centres are fixed). Needs `t_active_turn` to support a layer **mask** and two moves pushed. |
| C. Real slice moves | Large | Best | Needs centre orientation / cube rotations in `t_cube`; touches solver, parser, tests. Out of scope for the bonus. |

Nice extra for A: on a **centre** sticker, treat a *circular* drag as turning that face itself (angle = `atan2` of the cursor around the projected face centre). Optional.

### 2.4 S2 — Stuck drag

`tick_manual_mouse()` only ends the drag on the `IsMouseButtonReleased` edge. Ways to miss it:
- Press `A` mid-drag → `do_scramble()` → `MODE_AUTOPLAY`; `tick_manual` stops running, the release frame passes, `drag.active` stays true. Back in manual mode, the layer follows the mouse **with no button held** (`GetMouseDelta` keeps feeding it) until the next click.
- Window loses focus mid-drag (alt-tab / cmd-tab): GLFW may never deliver the release.

**Fix:** level-triggered release plus explicit cancel on mode changes:
```c
if (IsMouseButtonDown(MOUSE_BUTTON_LEFT) && IsWindowFocused())
    input_pick_drag(...);
else
    /* release path */
```
and call `input_pick_cancel(&app.drag)` (settle to 0, no move) whenever `app.mode` changes or `A` is pressed.

### 2.5 S3 — Camera can move mid-drag

`orbit_camera_update()` and the `5-8` preset keys run every frame regardless of `app.drag.active`. Right-button while left-dragging orbits the camera; a preset starts a 0.3 s lerp. The cached screen directions from drag-start are then wrong, and the layer drifts off the cursor.

**Fix (pick one):**
- Simple (recommended): skip orbit *input* and preset keys while `app.drag.active` (still rebuild `camera->position` from `orbit`). Also means the idle auto-orbit can't kick in mid-drag.
- Alternative: re-project `screen_dir[]` every frame from the live camera (2 × `GetWorldToScreen`, trivial cost). Only needed if you *want* orbit-while-dragging.

### 2.6 S4 — Sensitivity ignores zoom, window size and foreshortening

`DRAG_SENSITIVITY 0.5 deg/px` with **normalised** screen vectors means: zoomed in = the layer races ahead of the cursor, zoomed out = it lags; a bigger window makes turns feel slower. And when a face is nearly edge-on, `Vector2Normalize()` of a tiny vector amplifies noise.

**Fix:** keep the *un-normalised* projection (`screen_dir[k]` = pixels per world unit along `plane_dir[k]`). Convert the drag to world distance, then to an arc angle:
```
world = dot(d, sd) / max(dot(sd, sd), MIN_PX_PER_UNIT²)
deg   = sign * world / DRAG_RADIUS * RAD2DEG      // DRAG_RADIUS ≈ 1.5 * CUBIE_SPACING
```
The sticker now stays under the cursor at any zoom. `MIN_PX_PER_UNIT` (~20) caps the gain on edge-on faces. Clamp `accum_deg` to `±180`.

### 2.7 S5 — Snap has no intent/velocity

Nearest-of-`{−180,−90,0,90,180}` means 44° cancels and 46° commits, and a fast 30° flick cancels. Track a smoothed angular velocity during the drag and snap on a short look-ahead:
```c
projected = accum_deg + vel_deg_per_sec * DRAG_FLICK_LOOKAHEAD;  // ~0.08 s
quarters  = clamp(roundf(projected / 90.0f), -2, 2);
```
Smooth velocity with an EMA (`vel = 0.7 * vel + 0.3 * inst`) so one noisy frame can't fake a flick.

---

## 3. Performance (what a drag frame costs)

None of this causes the inversion/glitch, but frame-time spikes make a drag feel laggy, especially with rounded corners on.

### 3.1 P1 — Shader switch per cubie

`draw_cubie()` wraps every cubie in `BeginShaderMode()`/`EndShaderMode()`. In rlgl, `rlSetShader()` calls `rlDrawRenderBatch()` whenever the shader id changes → **~52 batch flushes (draw calls + buffer uploads) per frame**, plus extra splits from alternating triangles (`DrawCube`) and lines (`DrawCubeWires`) inside each cubie.

**Fix:** two passes in `draw_scene()`:
1. `BeginShaderMode` once → all 26 bodies + stickers (with the turn transform where needed) → `EndShaderMode`.
2. Default shader → all 26 wireframes (then fillets).

Drops to ~3-4 flushes/frame. The per-cubie `rlPushMatrix/rlRotatef/rlPopMatrix` is fine to keep (rlgl applies it on the CPU and, in this raylib version, transforms normals too, so lighting on the turning layer is correct).

### 3.2 P2 — Fillet spheres

`draw_corner_fillets()` draws 8 `DrawSphere()` per cubie. `DrawSphere` = `DrawSphereEx(…,16,16)` = 16·16·6 = **1,536 vertices**, × 208 ≈ **320k immediate-mode vertices per frame**, generated with `rlVertex3f` on the CPU and overflowing the 8192-quad batch several times.

Fixes, cheapest first:
1. `DrawSphereEx(pos, r, 4, 8, BODY_COLOR)` — 192 verts each, ~40k total (8× less), visually identical at r = 0.10.
2. Skip corners pointing into the puzzle (a corner sign opposite a non-zero cubie coordinate is hidden unless that layer is mid-turn) — roughly halves it again.
3. Proper fix: build one rounded-cubie `Mesh` once at startup (or `GenMeshCube` + precomputed fillets), upload it, and `DrawMesh(mesh, material, transform)` per cubie. GPU-side transforms, zero per-frame vertex generation.

### 3.3 Minor

- Picking cost is negligible either way (one click → 26 box tests); the §1.4 fix makes it 1.
- `orbit_camera_update()` drains `GetKeyPressed()` every frame to detect activity. It doesn't break `IsKeyPressed()`, but it silently eats the key queue — use a boolean "any key down/pressed" check instead if you ever need the queue (text input, rebinding).
- `geometry_sync()` on commit is 26 cheap array reads — fine.

---

## 4. Memory, leaks, lifetime

- **Drag path: no heap allocations, nothing to leak.** `t_drag_state` is a value inside `t_app`; `Ray`, `RayCollision`, `BoundingBox`, `Camera3D` are all by-value.
- `t_app` lives on `render_run()`'s stack: two `MAX_MOVES` (256) move arrays + the anim queue + two `t_cube`s ≈ a few KB. Fine.
- Lighting shader / fx are loaded once and unloaded before `CloseWindow()` — correct order.
- `input_pick_start()`: `RayCollision best` is read only when `best_index != -1`, but some GCC versions at `-O2 -Wall -Werror` emit `-Wmaybe-uninitialized` for this shape. Initialise it (`best = (RayCollision){0};`) — moot if you adopt §5.

---

## 5. Proposed implementation (handoff spec)

Keeps the existing module boundaries: `input.c` owns drag math, `anim.c` owns animation, `app.c` wires them. Signatures change; only `app.c` calls them.

### 5.1 `include/render/input.h`

```c
typedef struct s_drag_state
{
	bool	active;          // left button held on a sticker
	bool	locked;          // past the dead zone: axis/layer/sign fixed
	bool	blocked;         // locked onto a middle slice (S1 option A)
	Vector3	normal;          // clicked face's outward normal (unit axis)
	Vector3	plane_dir[2];    // world unit axes spanning that face
	Vector2	screen_dir[2];   // plane_dir[k] projected: px per world unit
	Vector2	start_mouse;
	int8_t	cubie_pos[3];
	int		chosen;          // index into plane_dir once locked
	t_axis	axis;
	int8_t	layer;
	float	sign;            // +1/-1 world-axis angle per unit of drag
	float	accum_deg;
	float	vel_deg_per_sec; // EMA-smoothed, for flick snapping
}	t_drag_state;

bool	input_pick_start(t_drag_state *drag, Camera3D camera);
void	input_pick_drag(t_drag_state *drag, Vector2 mouse, float dt);
t_move	input_pick_release(t_drag_state *drag, float *from_deg, float *to_deg);
void	input_pick_cancel(t_drag_state *drag);
t_active_turn	input_drag_get_active_turn(const t_drag_state *drag);
```

### 5.2 `src/render/input.c` (drag section)

```c
# define CUBE_HALF_EXTENT (CUBIE_SPACING + CUBIE_BODY_SIZE / 2.0f)
# define DRAG_DEADZONE_PX 8.0f
# define DRAG_RADIUS (1.5f * CUBIE_SPACING)
# define DRAG_MIN_PX_PER_UNIT 20.0f
# define DRAG_FLICK_LOOKAHEAD 0.08f
# define DRAG_BLOCK_DEG 12.0f

static float	vcomp(Vector3 v, int i)
{
	if (i == 0)
		return (v.x);
	if (i == 1)
		return (v.y);
	return (v.z);
}

static Vector3	axis_unit(int i)
{
	return ((Vector3){i == 0, i == 1, i == 2});
}

static int8_t	slot_from_coord(float c)
{
	return ((int8_t)clampf(roundf(c / CUBIE_SPACING), -1.0f, 1.0f));
}

/// One box for the whole puzzle: seams can no longer leak through to
/// interior faces (B4). The normal comes from the hit point itself, not
/// GetRayCollisionBox()'s truncated one.
bool	input_pick_start(t_drag_state *drag, Camera3D camera)
{
	RayCollision	hit;
	BoundingBox		box;
	Vector3			center;
	Vector2			origin;
	int				n;
	int				i;
	int				k;

	*drag = (t_drag_state){0};
	box.min = (Vector3){-CUBE_HALF_EXTENT, -CUBE_HALF_EXTENT, -CUBE_HALF_EXTENT};
	box.max = (Vector3){CUBE_HALF_EXTENT, CUBE_HALF_EXTENT, CUBE_HALF_EXTENT};
	hit = GetRayCollisionBox(GetScreenToWorldRay(GetMousePosition(), camera), box);
	if (!hit.hit)
		return (false);
	n = vector_to_axis(hit.point);                       // dominant |component|
	drag->normal = Vector3Scale(axis_unit(n), vcomp(hit.point, n) > 0 ? 1 : -1);
	i = -1;
	k = 0;
	while (++i < 3)
	{
		if (i == n)
			drag->cubie_pos[i] = (vcomp(hit.point, i) > 0) ? 1 : -1;
		else
		{
			drag->cubie_pos[i] = slot_from_coord(vcomp(hit.point, i));
			drag->plane_dir[k++] = axis_unit(i);
		}
	}
	center = (Vector3){drag->cubie_pos[0] * CUBIE_SPACING,
		drag->cubie_pos[1] * CUBIE_SPACING, drag->cubie_pos[2] * CUBIE_SPACING};
	origin = GetWorldToScreen(center, camera);
	k = -1;
	while (++k < 2)                                      // NOT normalised (S4)
		drag->screen_dir[k] = Vector2Subtract(GetWorldToScreen(
				Vector3Add(center, drag->plane_dir[k]), camera), origin);
	drag->start_mouse = GetMousePosition();
	drag->active = true;
	return (true);
}

/// Called once, the first frame the drag leaves the dead zone (B2).
static void	lock_axis(t_drag_state *drag, Vector2 d)
{
	float	score[2];
	float	len;
	int		k;
	Vector3	rot;

	k = -1;
	while (++k < 2)
	{
		len = Vector2Length(drag->screen_dir[k]);
		score[k] = 0.0f;
		if (len > 1e-3f)
			score[k] = fabsf(Vector2DotProduct(d, drag->screen_dir[k])) / len;
	}
	drag->chosen = (score[1] > score[0]);
	rot = Vector3CrossProduct(drag->normal, drag->plane_dir[drag->chosen]);
	drag->axis = vector_to_axis(rot);                    // a = n x d  (B1)
	drag->sign = (vcomp(rot, drag->axis) > 0.0f) ? 1.0f : -1.0f;
	drag->layer = drag->cubie_pos[drag->axis];
	drag->blocked = (drag->layer == 0);                  // S1 option A
	drag->locked = true;
}

void	input_pick_drag(t_drag_state *drag, Vector2 mouse, float dt)
{
	Vector2	d;
	Vector2	sd;
	float	len2;
	float	raw;
	float	prev;

	if (!drag->active)
		return ;
	d = Vector2Subtract(mouse, drag->start_mouse);       // total, not per-frame
	if (!drag->locked)
	{
		if (Vector2Length(d) < DRAG_DEADZONE_PX)
			return ;
		lock_axis(drag, d);
	}
	sd = drag->screen_dir[drag->chosen];
	len2 = fmaxf(Vector2DotProduct(sd, sd),
			DRAG_MIN_PX_PER_UNIT * DRAG_MIN_PX_PER_UNIT);
	raw = drag->sign * (Vector2DotProduct(d, sd) / len2) / DRAG_RADIUS * RAD2DEG;
	raw = clampf(raw, -180.0f, 180.0f);
	if (drag->blocked)
		raw = DRAG_BLOCK_DEG * tanhf(raw / DRAG_BLOCK_DEG);
	prev = drag->accum_deg;
	drag->accum_deg = raw;
	if (dt > 0.0f)
		drag->vel_deg_per_sec = 0.7f * drag->vel_deg_per_sec
			+ 0.3f * (raw - prev) / dt;
}

t_move	input_pick_release(t_drag_state *drag, float *from_deg, float *to_deg)
{
	float	quarters;

	drag->active = false;
	*from_deg = drag->accum_deg;
	*to_deg = 0.0f;
	if (!drag->locked || drag->blocked)
		return (MOVE_COUNT);
	quarters = roundf((drag->accum_deg
				+ drag->vel_deg_per_sec * DRAG_FLICK_LOOKAHEAD) / 90.0f);
	*to_deg = clampf(quarters, -2.0f, 2.0f) * 90.0f;
	if (*to_deg == 0.0f)
		return (MOVE_COUNT);
	return (anim_move_for_turn(drag->axis, drag->layer, *to_deg));
}

void	input_pick_cancel(t_drag_state *drag)
{
	drag->active = false;
	drag->locked = false;
	drag->accum_deg = 0.0f;
}

t_active_turn	input_drag_get_active_turn(const t_drag_state *drag)
{
	t_active_turn	turn;

	turn.active = drag->active && drag->locked;
	turn.axis = drag->axis;
	turn.layer = drag->layer;
	turn.angle_deg = drag->accum_deg;
	return (turn);
}
```

`face_plane_vectors()` and `DRAG_SENSITIVITY` are deleted. `vector_to_axis()` is kept (reused on `hit.point` and `rot`).

### 5.3 `src/render/anim.c` / `anim.h`

- Add to `t_anim_state`: `float from_deg; t_axis settle_axis; int8_t settle_layer; bool ease_out;`
- `anim_update()`: `angle_deg = from_deg + (target_deg - from_deg) * ease(frac)` with `ease = ease_out ? ease_out_cubic : ease_in_out_cubic`; on completion, only `apply_move()` / `geometry_sync()` / `fx_play_turn()` when `current != MOVE_COUNT`. Queue-started moves set `from_deg = 0`, `ease_out = false` (autoplay unchanged).
- New:
  ```c
  void	anim_begin_from(t_anim_state *s, t_move move, float from, float to);
  void	anim_begin_settle(t_anim_state *s, t_axis axis, int8_t layer, float from);
  ```
  Both set `active = true`, `elapsed = 0`, `ease_out = true`, `duration = fmaxf(fabsf(to - from) / speed, 0.06f)`. Settle sets `current = MOVE_COUNT`, `to = 0`.
- `anim_get_active_turn()`: if `current == MOVE_COUNT` use `settle_axis/settle_layer`.
- `anim_move_for_turn()`: accept `±180` for half turns (§2.2).
- `anim_pending_count()`: don't count a settle as a pending move (HUD "moves remaining").

### 5.4 `src/render/app.c`

```c
static void	tick_manual_mouse(t_app *app, float dt)
{
	t_move	move;
	float	from;
	float	to;

	if (!app->drag.active)
	{
		if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
			input_pick_start(&app->drag, app->camera);
		return ;
	}
	if (IsMouseButtonDown(MOUSE_BUTTON_LEFT) && IsWindowFocused())   // S2
	{
		input_pick_drag(&app->drag, GetMousePosition(), dt);
		return ;
	}
	move = input_pick_release(&app->drag, &from, &to);
	if (move != MOVE_COUNT)
		push_manual_move_from(app, move, from, to);   // bookkeeping + anim_begin_from
	else if (from != 0.0f)
		anim_begin_settle(&app->anim, app->drag.axis, app->drag.layer, from);
}
```
- Split `push_manual_move()` into the bookkeeping part (history, stats, `solution_scrubbable`) and the anim part, so the drag path can use `anim_begin_from()` while keyboard/undo keep `anim_push()`.
- `render_run()`:
  - `SetConfigFlags` / `SetTargetFPS` (§1.3).
  - Skip orbit **input** and `5-8` while `app.drag.active` (§2.5).
  - On `A` (and anywhere `app.mode` changes): `input_pick_cancel(&app.drag)`.
- Optional: left-drag that misses the cube (`input_pick_start` → false) orbits the camera. Very common convention; makes the cube usable with a trackpad and no right button.

### 5.5 `src/render/draw.c`

Two-pass `draw_scene()` (§3.1) and low-poly / culled fillets (§3.2). Factor the "maybe rotate this cubie" block into a helper taking a draw callback, or just duplicate the 6-line push/rotate/pop in each pass.

---

## 6. Test plan

### 6.1 Unit tests (no window needed)

Pull the pure math out so it's testable: `lock_axis()` only needs `normal`, `plane_dir`, `screen_dir`, `cubie_pos` — feed synthetic `screen_dir` where `screen_dir[k]` = the world axis projected by a simple ortho camera. Expected results (sticker at the face's `(+,+)` corner, a real cube as oracle):

| Face (sticker) | +axis drag | −axis drag | +axis drag | −axis drag |
|----------------|-----------|-----------|-----------|-----------|
| R `(1,1,1)`  | +Y → `F'` | −Y → `F`  | +Z → `U`  | −Z → `U'` |
| L `(−1,1,1)` | +Y → `F`  | −Y → `F'` | +Z → `U'` | −Z → `U`  |
| U `(1,1,1)`  | +X → `F`  | −X → `F'` | +Z → `R'` | −Z → `R`  |
| D `(1,−1,1)` | +X → `F'` | −X → `F`  | +Z → `R`  | −Z → `R'` |
| F `(1,1,1)`  | +X → `U'` | −X → `U`  | +Y → `R`  | −Y → `R'` |
| B `(1,1,−1)` | +X → `U`  | −X → `U'` | +Y → `R'` | −Y → `R`  |

Plus:
- Centre sticker, any direction → `MOVE_COUNT`, `blocked == true`.
- Drag to `+180` and `−180` on every face → the `X2` move (B6).
- Seam hit: `hit.point = (0.525, 0.3, 1.52)` → normal `+Z`, `cubie_pos = (1 or 0 by rounding, 0, 1)` — never an interior normal.
- Path `(0,0)→(20,1)→(20,1)` (still frame) → axis stays the one locked at step 1 (B2 regression).

Add these to `tests/` next to `test_moves.c`; they fit the existing `testlib.h` style.

### 6.2 Manual gate (update `03b` §8.7)

1. Default view: every face you can see, both directions, 3 stickers each (corner, edge, centre). Direction matches a physical cube.
2. Orbit to look from below/behind — directions still correct (this was the §8.7 trap; the `n × d` formula handles it for free).
3. Slow diagonal drag → layer picks one axis and never flickers.
4. Click exactly on seams, near cube edges, at grazing angles → always the sticker under the cursor.
5. Release at 60°, 30°, and a fast 30° flick → smooth settle from the release angle, no rewind; flick commits.
6. Hold left, press `A`, release, move mouse → nothing follows the cursor. Same with cmd/alt-tab mid-drag.
7. Right-drag during left-drag → camera doesn't move (or the drag stays glued if you chose re-projection).
8. `C` on (rounded corners), check FPS with `DrawFPS(10, 10)` before/after P1/P2.
9. Re-run the §5.6 anti-cheat test scrambling by mouse only.

---

## 7. Nitpicks

- [NITPICK] `input.h` comment on `t_drag_state` still describes the old per-frame, cached-at-start model — update after §5.
- [NITPICK] `ease_in_out_cubic`/`clampf` are duplicated in `input.c` and `anim.c` — one shared `render/mathx.h` inline header.
- [NITPICK] HUD says "Left-drag a sticker: turn" — worth a hover highlight on the sticker under the cursor (reuse the §1.4 pick, draw a thin outline) so users see what they'll grab before clicking.
- [NITPICK] If you ever set `FLAG_WINDOW_HIGHDPI`, re-check `GetMousePosition()` vs `GetWorldToScreen()` scale on Retina; today both are in the same (logical) space, so it's fine.
