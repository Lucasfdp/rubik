#include <math.h>
#include "raylib.h"
#include "render/input.h"

# define ORBIT_MIN_DISTANCE 4.0f
# define ORBIT_MAX_DISTANCE 20.0f
# define ORBIT_MIN_PITCH -80.0f
# define ORBIT_MAX_PITCH 80.0f
# define ORBIT_SENSITIVITY 0.3f

typedef struct s_face_key
{
	int		key;
	t_move	base;
}	t_face_key;

/// One entry per face letter, mapped to that face's clockwise move
/// (MOVE_x1); a held Shift or '2' shifts it to the ccw/double variant
/// (t_move's own layout: face * 3 + turn, see cube.h).
static const t_face_key	FACE_KEYS[6] = {
	{KEY_U, MOVE_U1},
	{KEY_R, MOVE_R1},
	{KEY_F, MOVE_F1},
	{KEY_D, MOVE_D1},
	{KEY_L, MOVE_L1},
	{KEY_B, MOVE_B1},
};

static float	clampf(float value, float lo, float hi)
{
	if (value < lo)
		return (lo);
	if (value > hi)
		return (hi);
	return (value);
}

void	orbit_camera_update(t_orbit_camera *orbit, Camera3D *camera,
	float dt)
{
	Vector2	delta;

	(void)dt;
	if (IsMouseButtonDown(MOUSE_BUTTON_RIGHT))
	{
		delta = GetMouseDelta();
		orbit->yaw_deg -= delta.x * ORBIT_SENSITIVITY;
		orbit->pitch_deg = clampf(orbit->pitch_deg - delta.y
				* ORBIT_SENSITIVITY, ORBIT_MIN_PITCH, ORBIT_MAX_PITCH);
	}
	orbit->distance = clampf(orbit->distance - GetMouseWheelMove(),
			ORBIT_MIN_DISTANCE, ORBIT_MAX_DISTANCE);
	camera->position.x = orbit->distance * cosf(DEG2RAD * orbit->pitch_deg)
		* sinf(DEG2RAD * orbit->yaw_deg);
	camera->position.y = orbit->distance * sinf(DEG2RAD * orbit->pitch_deg);
	camera->position.z = orbit->distance * cosf(DEG2RAD * orbit->pitch_deg)
		* cosf(DEG2RAD * orbit->yaw_deg);
	camera->target = (Vector3){0.0f, 0.0f, 0.0f};
}

t_move	input_poll_keyboard(void)
{
	int	i;

	i = 0;
	while (i < 6)
	{
		if (IsKeyPressed(FACE_KEYS[i].key))
		{
			if (IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT))
				return ((t_move)(FACE_KEYS[i].base + 2));
			if (IsKeyDown(KEY_TWO))
				return ((t_move)(FACE_KEYS[i].base + 1));
			return (FACE_KEYS[i].base);
		}
		i++;
	}
	return (MOVE_COUNT);
}
