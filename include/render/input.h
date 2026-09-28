#ifndef RENDER_INPUT_H
# define RENDER_INPUT_H

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

/// Mouse click-and-drag turning (Phase 6, docs/en/03b-3d-implementation-
/// plan.md section 8). active/axis/layer/accum_deg are exactly the doc's
/// own field list — the rest are implementation details input_pick_drag()
/// needs every frame but has no camera to recompute (screen_right/up are
/// cached once, at drag-start, per section 8.4 step 1's "once at
/// drag-start" instruction); cubie_pos is the clicked cubie's fixed slot,
/// kept so the eventual rotation axis (only known once the drag direction
/// resolves it) can look up ITS layer coordinate, not the clicked face's
/// own.
typedef struct s_drag_state
{
	bool	active;
	t_axis	axis;
	int8_t	layer;
	Vector3	plane_right;
	Vector3	plane_up;
	float	accum_deg;
	Vector2	screen_right;
	Vector2	screen_up;
	int8_t	cubie_pos[3];
}	t_drag_state;

/// @brief Right-drag to orbit, wheel to zoom; updates camera in place.
///        Also advances any in-progress camera-preset lerp (section 9.1)
///        and the idle-auto-orbit timer.
void	orbit_camera_update(t_orbit_camera *orbit, Camera3D *camera,
			float dt);

/// @brief Starts easing `orbit` toward hardcoded preset `index` (0-3)
///        over ORBIT_PRESET_LERP_SEC. No-op for an out-of-range index.
void	orbit_camera_preset(t_orbit_camera *orbit, int index);

/// @brief Polls one frame of keyboard input for a manual face turn.
///
/// @return The move for whichever face key (U R F D L B) was pressed
///         THIS frame, modified by a held Shift (counter-clockwise) or a
///         held '2' (half turn); MOVE_COUNT (the "no real move" value)
///         when nothing relevant was pressed.
t_move	input_poll_keyboard(void);

/// @brief Casts a ray from the current mouse position and, on hitting
///        one of the 26 cubies, starts a drag on it. Call on left-mouse-
///        down.
///
/// @return false (and leaves *drag inactive) if the ray hit nothing.
bool	input_pick_start(t_drag_state *drag, const t_render_scene *scene,
			Camera3D camera);

/// @brief Advances an active drag by one frame's mouse movement: resolves
///        (or re-resolves) which axis/layer the drag is turning and
///        accumulates the live, uncommitted rotation angle. No-op if
///        *drag is not active.
void	input_pick_drag(t_drag_state *drag, Vector2 mouse_delta);

/// @brief Ends the drag: snaps the accumulated angle to the nearest of
///        {-180, -90, 0, 90, 180} and resolves it to a real move.
///
/// @return MOVE_COUNT if the drag was too small to count as a turn
///         (treat as a no-op click).
t_move	input_pick_release(t_drag_state *drag);

/// @brief Builds a t_active_turn from the drag currently in progress —
///        same shape anim_get_active_turn() returns, so render_run() can
///        feed draw_scene() whichever of the two (anim or drag) is
///        active without draw.c ever knowing t_drag_state exists.
t_active_turn	input_drag_get_active_turn(const t_drag_state *drag);

#endif
