#ifndef RENDER_INPUT_H
# define RENDER_INPUT_H

# include <stdint.h>
# include "raylib.h"
# include "cube.h"
# include "render/geometry.h"

/// Which source is driving the cube right now. MODE_AUTOPLAY: a
/// solver-produced queue is playing or paused. MODE_MANUAL: idle,
/// waiting on keyboard, mouse-drag, or the timeline-scrub keys.
typedef enum e_render_mode
{
	MODE_AUTOPLAY,
	MODE_MANUAL,
}	t_render_mode;

/// Spherical-coordinates orbit camera. Deliberately hand-rolled instead
/// of raylib's stock UpdateCamera(CAMERA_ORBITAL): the button split
/// (right-drag orbits, left click/drag turns a layer) needs the drag
/// gated on the RIGHT mouse button specifically, which the stock helper
/// cannot do.
///
/// lerp_*/lerping: Phase 7 §9.1's camera presets ease into their target
/// over ORBIT_PRESET_LERP_SEC instead of snapping — orbit_camera_preset()
/// starts a lerp, orbit_camera_update() advances and ends it, and any
/// real right-drag/wheel input cancels it early so the user always keeps
/// control. idle_sec: same section's auto-orbit — orbit_camera_update()
/// increments it whenever no input is seen and slowly spins yaw_deg once
/// it crosses ORBIT_IDLE_SECONDS.
typedef struct s_orbit_camera
{
	float	yaw_deg;
	float	pitch_deg;
	float	distance;
	bool	lerping;
	float	lerp_from_yaw;
	float	lerp_from_pitch;
	float	lerp_from_distance;
	float	lerp_to_yaw;
	float	lerp_to_pitch;
	float	lerp_to_distance;
	float	lerp_elapsed_sec;
	float	idle_sec;
}	t_orbit_camera;

/// One hardcoded camera view (docs/en/03b-3d-implementation-plan.md
/// section 9.1), bound to 5-8. Struct exists so CAMERA_PRESETS
/// (input.c) has a named type, per this project's own norm that struct
/// definitions live in headers even when the table using them stays
/// private to one .c file.
typedef struct s_orbit_preset
{
	float	yaw_deg;
	float	pitch_deg;
	float	distance;
}	t_orbit_preset;

/// Mouse click-and-drag turning, rewritten per docs/en/11-drag-review.md
/// (§5.1) to fix the direction/flicker/seam bugs the field-list-only
/// version had. `active`: left button down on a valid pick. `locked`:
/// past the dead zone — axis/layer/sign/chosen are now fixed for the
/// rest of this drag and never re-decided from a later frame (B2).
/// `blocked`: locked onto a middle slice (no move exists for it, S1) —
/// still tracked with a rubber-band feel, always settles to 0.
/// `normal`: the clicked face's outward unit normal, used with
/// `plane_dir[k]` (the two in-plane world axes) to get a *signed*
/// rotation axis via a cross product at lock time (B1) instead of
/// discarding the sign the old plane_right/plane_up split lost.
/// `screen_dir[k]`: plane_dir[k] projected to screen space and kept
/// UN-normalised (S4) — its length is pixels-per-world-unit, so the
/// drag scales correctly with zoom/window size. `start_mouse`: the
/// press position; every frame measures TOTAL displacement from here,
/// never a per-frame delta (B2/B3). `cubie_pos`: the clicked cubie's
/// fixed slot, used both to resolve the eventual axis's layer and to
/// tell a middle-slice pick (blocked) from an outer one. `chosen`:
/// which of plane_dir[2]/screen_dir[2] this drag is measured along,
/// fixed at lock time. `sign`: +1/-1, the actual sign a positive drag
/// along `chosen` contributes to `axis`'s rotation (this is what B1 was
/// missing). `center_turn`: locked onto a face's dead-centre sticker
/// (§2.3/S1's optional extra) — every straight-line direction off a
/// centre is a middle slice with no move (see `blocked` above), so
/// instead the drag is read as a CIRCLE around the centre: `axis`/
/// `layer` become the face's own (not a cross-product result), and
/// `accum_deg` tracks accumulated sweep angle rather than a linear
/// projection — everything downstream (snap, flick, hand-off) is then
/// identical to a normal drag. `center_prev_angle_deg`: last frame's
/// raw angle, so the next frame can measure a signed DELTA instead of
/// re-deriving an absolute angle that would jump at +/-180. `vel_deg_
/// per_sec`: an EMA-smoothed angular velocity, used only for the
/// flick-snap look-ahead on release (S5).
typedef struct s_drag_state
{
	bool	active;
	bool	locked;
	bool	blocked;
	bool	center_turn;
	Vector3	normal;
	Vector3	plane_dir[2];
	Vector2	screen_dir[2];
	Vector2	start_mouse;
	int8_t	cubie_pos[3];
	int		chosen;
	t_axis	axis;
	int8_t	layer;
	float	sign;
	float	accum_deg;
	float	center_prev_angle_deg;
	float	vel_deg_per_sec;
}	t_drag_state;

/// @brief Right-drag to orbit, wheel to zoom; updates camera in place.
///        Also advances any in-progress camera-preset lerp (section 9.1)
///        and the idle-auto-orbit timer. `allow_input` gates all of
///        that (right-drag, wheel, idle/auto-orbit) — false while a
///        left-drag turn is active (docs/en/11-drag-review.md §2.5/S3),
///        so the camera can't move (and screen_dir[] can't go stale)
///        mid-turn; camera->position is still rebuilt from `orbit`
///        either way, so a preset lerp already in flight keeps playing.
void	orbit_camera_update(t_orbit_camera *orbit, Camera3D *camera,
			float dt, bool allow_input);

/// @brief Starts easing `orbit` toward hardcoded preset `index` (0-3)
///        over ORBIT_PRESET_LERP_SEC. No-op for an out-of-range index.
void	orbit_camera_preset(t_orbit_camera *orbit, int index);

/// @brief Arrow-key camera control: left/right orbits yaw, up/down
///        orbits pitch, at a fixed rate while held — the keyboard
///        equivalent of a right-mouse-drag orbit. Cancels any
///        in-progress preset lerp and resets the idle-auto-orbit timer
///        when a key is actually held, same as real mouse input would
///        (orbit_camera_update() has no way to know keys were the
///        source). Caller decides WHEN this applies — app.c only calls
///        it in Mode C (manual): Mode A's arrows already mean playback
///        speed/step, and it is skipped entirely while a left-drag turn
///        is active, same as the mouse-driven orbit (§2.5/S3) — moving
///        the camera mid-turn would go stale the same way.
void	orbit_camera_key_nudge(t_orbit_camera *orbit, float dt);

/// @brief Polls one frame of keyboard input for a manual face turn.
///
/// @return The move for whichever face key (U R F D L B) was pressed
///         THIS frame, modified by a held Shift (counter-clockwise) or a
///         held '2' (half turn); MOVE_COUNT (the "no real move" value)
///         when nothing relevant was pressed.
t_move	input_poll_keyboard(void);

/// @brief Casts a ray from the current mouse position against a SINGLE
///        box covering the whole puzzle (docs/en/11-drag-review.md §1.4
///        / B4 — not 26 per-cubie boxes with gaps a seam click could
///        fall through), then derives the clicked sticker's face normal
///        and cubie slot straight from the hit point. Call on left-
///        mouse-down.
///
/// @return false (and leaves *drag inactive) if the ray hit nothing.
bool	input_pick_start(t_drag_state *drag, Camera3D camera);

/// @brief Advances an active drag by one frame: on the first frame past
///        the dead zone, LOCKS axis/layer/sign/chosen from the total
///        displacement since press (never re-decided afterwards, B2);
///        every frame after that, recomputes the live, uncommitted
///        rotation angle (and its EMA-smoothed velocity, for S5) from
///        that same total displacement. No-op if *drag is not active.
///
/// @param mouse Current mouse position (absolute, not a delta — B2/B3
///               need total displacement from the press point).
/// @param dt     This frame's delta time, for the velocity EMA.
void	input_pick_drag(t_drag_state *drag, Vector2 mouse, float dt);

/// @brief Ends the drag: snaps the accumulated angle (plus a short
///        velocity look-ahead, S5) to the nearest quarter turn and
///        resolves it to a real move — MOVE_COUNT if the drag never left
///        the dead zone, is blocked (middle slice, S1), or snapped to 0.
///
/// @param from_deg Set to the angle the drag had reached at release —
///                  feed to anim_begin_from()/anim_begin_settle() so the
///                  layer animates onward from here instead of rewinding
///                  to 0 (B5).
/// @param to_deg   Set to the signed target angle (a multiple of 90, up
///                  to ±180) the move should animate TO — the user's own
///                  dragged direction, not necessarily MOVE_AXIS's sign
///                  for a half turn (B6). 0 when MOVE_COUNT is returned.
t_move	input_pick_release(t_drag_state *drag, float *from_deg,
			float *to_deg);

/// @brief Cancels an in-progress drag with no move: clears active/locked
///        and zeroes the accumulated angle. Call on any mode change or
///        interrupting key so a drag can never get stuck following the
///        mouse with the button no longer held (docs/en/11-drag-
///        review.md §2.4 / S2).
void	input_pick_cancel(t_drag_state *drag);

/// @brief Builds a t_active_turn from the drag currently in progress —
///        same shape anim_get_active_turn() returns, so render_run() can
///        feed draw_scene() whichever of the two (anim or drag) is
///        active without draw.c ever knowing t_drag_state exists.
///        Inactive until the drag is past the dead zone (`locked`), so
///        a small, still-undecided drag never shows a phantom rotation.
t_active_turn	input_drag_get_active_turn(const t_drag_state *drag);

#endif
