#ifndef RENDER_INPUT_H
# define RENDER_INPUT_H

# include "raylib.h"
# include "cube.h"

/// Which source is driving the cube right now. MODE_AUTOPLAY: a
/// solver-produced queue is playing or paused. MODE_MANUAL: idle,
/// waiting on keyboard (or, later, mouse-drag) turns.
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
typedef struct s_orbit_camera
{
	float	yaw_deg;
	float	pitch_deg;
	float	distance;
}	t_orbit_camera;

/// @brief Right-drag to orbit, wheel to zoom; updates camera in place.
void	orbit_camera_update(t_orbit_camera *orbit, Camera3D *camera,
			float dt);

/// @brief Polls one frame of keyboard input for a manual face turn.
///
/// @return The move for whichever face key (U R F D L B) was pressed
///         THIS frame, modified by a held Shift (counter-clockwise) or a
///         held '2' (half turn); MOVE_COUNT (the "no real move" value)
///         when nothing relevant was pressed.
t_move	input_poll_keyboard(void);

#endif
