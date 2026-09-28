#include <math.h>
#include "raylib.h"
#include "raymath.h"
#include "render/input.h"
#include "render/anim.h"

# define ORBIT_MIN_DISTANCE 4.0f
# define ORBIT_MAX_DISTANCE 20.0f
# define ORBIT_MIN_PITCH -80.0f
# define ORBIT_MAX_PITCH 80.0f
# define ORBIT_SENSITIVITY 0.3f
# define ORBIT_IDLE_SECONDS 6.0f
# define ORBIT_AUTO_ORBIT_DEG_PER_SEC 6.0f
# define ORBIT_PRESET_LERP_SEC 0.3f
# define DRAG_SENSITIVITY 0.5f
# define CAMERA_PRESET_COUNT 4

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

/// Four hardcoded views (Phase 7 §9.1): a standard iso angle (the app's
/// own startup default), then front/right/top-ish angles.
static const t_orbit_preset	CAMERA_PRESETS[CAMERA_PRESET_COUNT] = {
	{40.0f, 25.0f, 11.0f},
	{0.0f, 0.0f, 9.0f},
	{90.0f, 15.0f, 9.0f},
	{45.0f, 60.0f, 10.0f},
};

static float	clampf(float value, float lo, float hi)
{
	if (value < lo)
		return (lo);
	if (value > hi)
		return (hi);
	return (value);
}

static float	ease_in_out_cubic(float t)
{
	if (t < 0.5f)
		return (4.0f * t * t * t);
	return (1.0f - powf(-2.0f * t + 2.0f, 3.0f) / 2.0f);
}

static void	advance_lerp(t_orbit_camera *orbit, float dt)
{
	float	t;

	orbit->lerp_elapsed_sec += dt;
	t = orbit->lerp_elapsed_sec / ORBIT_PRESET_LERP_SEC;
	if (t >= 1.0f)
	{
		t = 1.0f;
		orbit->lerping = false;
	}
	t = ease_in_out_cubic(t);
	orbit->yaw_deg = orbit->lerp_from_yaw
		+ (orbit->lerp_to_yaw - orbit->lerp_from_yaw) * t;
	orbit->pitch_deg = orbit->lerp_from_pitch
		+ (orbit->lerp_to_pitch - orbit->lerp_from_pitch) * t;
	orbit->distance = orbit->lerp_from_distance
		+ (orbit->lerp_to_distance - orbit->lerp_from_distance) * t;
}

void	orbit_camera_preset(t_orbit_camera *orbit, int index)
{
	if (index < 0 || index >= CAMERA_PRESET_COUNT)
		return ;
	orbit->lerp_from_yaw = orbit->yaw_deg;
	orbit->lerp_from_pitch = orbit->pitch_deg;
	orbit->lerp_from_distance = orbit->distance;
	orbit->lerp_to_yaw = CAMERA_PRESETS[index].yaw_deg;
	orbit->lerp_to_pitch = CAMERA_PRESETS[index].pitch_deg;
	orbit->lerp_to_distance = CAMERA_PRESETS[index].distance;
	orbit->lerp_elapsed_sec = 0.0f;
	orbit->lerping = true;
}

void	orbit_camera_update(t_orbit_camera *orbit, Camera3D *camera,
	float dt)
{
	Vector2	delta;
	bool	input_seen;

	input_seen = false;
	while (GetKeyPressed() != 0)
		input_seen = true;
	if (IsMouseButtonDown(MOUSE_BUTTON_RIGHT))
	{
		delta = GetMouseDelta();
		if (delta.x != 0.0f || delta.y != 0.0f)
		{
			orbit->lerping = false;
			input_seen = true;
		}
		orbit->yaw_deg -= delta.x * ORBIT_SENSITIVITY;
		orbit->pitch_deg = clampf(orbit->pitch_deg - delta.y
				* ORBIT_SENSITIVITY, ORBIT_MIN_PITCH, ORBIT_MAX_PITCH);
	}
	if (GetMouseWheelMove() != 0.0f)
	{
		orbit->lerping = false;
		input_seen = true;
	}
	if (IsMouseButtonDown(MOUSE_BUTTON_LEFT))
		input_seen = true;
	orbit->distance = clampf(orbit->distance - GetMouseWheelMove(),
			ORBIT_MIN_DISTANCE, ORBIT_MAX_DISTANCE);
	if (orbit->lerping)
		advance_lerp(orbit, dt);
	else if (input_seen)
		orbit->idle_sec = 0.0f;
	else
	{
		orbit->idle_sec += dt;
		if (orbit->idle_sec > ORBIT_IDLE_SECONDS)
			orbit->yaw_deg += ORBIT_AUTO_ORBIT_DEG_PER_SEC * dt;
	}
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

/// @brief plane_right/plane_up for a clicked face whose outward normal is
///        `normal` (one of the 6 unit axis directions): two more unit
///        axis vectors spanning that face's own plane, picked by crossing
///        `normal` with a reference "world up" — swapped to world
///        "right" for the Up/Down faces, where normal and world up are
///        parallel and the cross product would degenerate to zero.
///        Reproduces this doc's own worked example exactly: normal=+X
///        gives plane_right=+Z, plane_up=+Y (section 8.3 step 4).
static void	face_plane_vectors(Vector3 normal, Vector3 *right, Vector3 *up)
{
	Vector3	reference;

	if (fabsf(normal.y) > 0.5f)
		reference = (Vector3){1.0f, 0.0f, 0.0f};
	else
		reference = (Vector3){0.0f, 1.0f, 0.0f};
	*right = Vector3Normalize(Vector3CrossProduct(normal, reference));
	*up = Vector3CrossProduct(*right, normal);
}

/// @brief Which world axis a (near-)axis-aligned vector points along —
///        used to turn plane_right/plane_up (always one of the 6 unit
///        axis directions here) into a t_axis for the eventual move.
static t_axis	vector_to_axis(Vector3 v)
{
	if (fabsf(v.x) >= fabsf(v.y) && fabsf(v.x) >= fabsf(v.z))
		return (AXIS_X);
	if (fabsf(v.y) >= fabsf(v.z))
		return (AXIS_Y);
	return (AXIS_Z);
}

bool	input_pick_start(t_drag_state *drag, const t_render_scene *scene,
	Camera3D camera)
{
	Ray				ray;
	RayCollision	hit;
	RayCollision	best;
	int				best_index;
	int				i;
	Vector3			half;
	Vector3			center;
	BoundingBox		box;
	Vector2			origin_2d;

	drag->active = false;
	ray = GetScreenToWorldRay(GetMousePosition(), camera);
	half = (Vector3){CUBIE_BODY_SIZE / 2.0f, CUBIE_BODY_SIZE / 2.0f,
		CUBIE_BODY_SIZE / 2.0f};
	best_index = -1;
	i = 0;
	while (i < RENDER_CUBIE_COUNT)
	{
		center = (Vector3){scene->cubies[i].x * CUBIE_SPACING,
			scene->cubies[i].y * CUBIE_SPACING,
			scene->cubies[i].z * CUBIE_SPACING};
		box.min = Vector3Subtract(center, half);
		box.max = Vector3Add(center, half);
		hit = GetRayCollisionBox(ray, box);
		if (hit.hit && (best_index == -1 || hit.distance < best.distance))
		{
			best = hit;
			best_index = i;
		}
		i++;
	}
	if (best_index == -1)
		return (false);
	center = (Vector3){scene->cubies[best_index].x * CUBIE_SPACING,
		scene->cubies[best_index].y * CUBIE_SPACING,
		scene->cubies[best_index].z * CUBIE_SPACING};
	drag->cubie_pos[0] = scene->cubies[best_index].x;
	drag->cubie_pos[1] = scene->cubies[best_index].y;
	drag->cubie_pos[2] = scene->cubies[best_index].z;
	face_plane_vectors(best.normal, &drag->plane_right, &drag->plane_up);
	origin_2d = GetWorldToScreen(center, camera);
	drag->screen_right = Vector2Normalize(Vector2Subtract(
			GetWorldToScreen(Vector3Add(center, drag->plane_right), camera),
			origin_2d));
	drag->screen_up = Vector2Normalize(Vector2Subtract(
			GetWorldToScreen(Vector3Add(center, drag->plane_up), camera),
			origin_2d));
	drag->accum_deg = 0.0f;
	drag->axis = AXIS_X;
	drag->layer = 0;
	drag->active = true;
	return (true);
}

void	input_pick_drag(t_drag_state *drag, Vector2 mouse_delta)
{
	float	along_right;
	float	along_up;
	Vector3	chosen;

	if (!drag->active)
		return ;
	along_right = mouse_delta.x * drag->screen_right.x
		+ mouse_delta.y * drag->screen_right.y;
	along_up = mouse_delta.x * drag->screen_up.x
		+ mouse_delta.y * drag->screen_up.y;
	if (fabsf(along_up) > fabsf(along_right))
	{
		drag->accum_deg += along_up * DRAG_SENSITIVITY;
		chosen = drag->plane_right;
	}
	else
	{
		drag->accum_deg += along_right * DRAG_SENSITIVITY;
		chosen = drag->plane_up;
	}
	drag->axis = vector_to_axis(chosen);
	drag->layer = drag->cubie_pos[drag->axis];
}

t_move	input_pick_release(t_drag_state *drag)
{
	static const float	snap_targets[5] = {-180.0f, -90.0f, 0.0f, 90.0f,
		180.0f};
	float				snapped;
	float				best_diff;
	float				diff;
	int					i;

	drag->active = false;
	snapped = snap_targets[0];
	best_diff = fabsf(drag->accum_deg - snap_targets[0]);
	i = 1;
	while (i < 5)
	{
		diff = fabsf(drag->accum_deg - snap_targets[i]);
		if (diff < best_diff)
		{
			best_diff = diff;
			snapped = snap_targets[i];
		}
		i++;
	}
	if (snapped == 0.0f)
		return (MOVE_COUNT);
	return (anim_move_for_turn(drag->axis, drag->layer, snapped));
}

t_active_turn	input_drag_get_active_turn(const t_drag_state *drag)
{
	t_active_turn	turn;

	turn.active = drag->active;
	turn.axis = drag->axis;
	turn.layer = drag->layer;
	turn.angle_deg = drag->accum_deg;
	return (turn);
}
