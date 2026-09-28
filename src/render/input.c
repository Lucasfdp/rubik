#include <math.h>
#include "raylib.h"
#include "raymath.h"
#include "render/input.h"
#include "render/anim.h"

# define ORBIT_MIN_DISTANCE 4.0f
# define ORBIT_MAX_DISTANCE 20.0f
# define ORBIT_SENSITIVITY 0.3f
# define ORBIT_KEY_SENSITIVITY 90.0f
# define ORBIT_IDLE_SECONDS 6.0f
# define ORBIT_AUTO_ORBIT_DEG_PER_SEC 6.0f
# define ORBIT_PRESET_LERP_SEC 0.3f
# define CAMERA_PRESET_COUNT 4
/// docs/en/12-visual-polish-review.md camera-pitch bug: raylib's
/// LookAt uses a fixed world-up (0,1,0) (see app.c's Camera3D init),
/// so a pitch of exactly +/-90 degrees puts the view direction
/// parallel to up -- a gimbal-lock singularity where the camera's
/// right vector degenerates and the orbit visually reverses instead
/// of continuing over the pole. Clamping just short of the poles
/// keeps the camera in the well-defined range on both sides.
# define ORBIT_PITCH_LIMIT_DEG 89.0f

/// Half-extent of a single box covering the whole puzzle (26 cubies on
/// a +/-1 lattice, CUBIE_SPACING apart, each CUBIE_BODY_SIZE wide) —
/// docs/en/11-drag-review.md §1.4/B4's replacement for 26 separate,
/// gapped per-cubie boxes.
# define CUBE_HALF_EXTENT (CUBIE_SPACING + CUBIE_BODY_SIZE / 2.0f)
/// Pixels of total displacement from the press point before a drag
/// commits to an axis/layer at all (§1.2/B2).
# define DRAG_DEADZONE_PX 8.0f
/// World-unit "radius" the drag angle is measured as an arc over
/// (§2.6/S4): world_distance / DRAG_RADIUS, in radians.
# define DRAG_RADIUS (1.5f * CUBIE_SPACING)
/// Floor on the screen-space scale (px per world unit) used to convert
/// a drag into world distance, so a face seen nearly edge-on (screen_dir
/// close to zero length) can't blow up the sensitivity (§2.6/S4).
# define DRAG_MIN_PX_PER_UNIT 20.0f
/// Look-ahead (seconds) applied to the smoothed angular velocity when
/// snapping on release, so a fast flick commits even short of 45°
/// (§2.7/S5).
# define DRAG_FLICK_LOOKAHEAD 0.08f
/// Rubber-band cap (degrees) for a drag locked onto a middle slice —
/// no move exists for one, so it never actually turns, only resists
/// and always settles back to 0 (§2.3/S1 option A).
# define DRAG_BLOCK_DEG 12.0f

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

/// @brief left/right = yaw, up/down = pitch, ORBIT_KEY_SENSITIVITY
///        degrees/sec while held. Signed the same way a right-mouse-
///        drag orbit is (see orbit_camera_update()): "left" turns the
///        same direction dragging the mouse left would.
void	orbit_camera_key_nudge(t_orbit_camera *orbit, float dt)
{
	bool	pressed;

	pressed = false;
	if (IsKeyDown(KEY_LEFT))
	{
		orbit->yaw_deg += ORBIT_KEY_SENSITIVITY * dt;
		pressed = true;
	}
	if (IsKeyDown(KEY_RIGHT))
	{
		orbit->yaw_deg -= ORBIT_KEY_SENSITIVITY * dt;
		pressed = true;
	}
	if (IsKeyDown(KEY_UP))
	{
		orbit->pitch_deg += ORBIT_KEY_SENSITIVITY * dt;
		pressed = true;
	}
	if (IsKeyDown(KEY_DOWN))
	{
		orbit->pitch_deg -= ORBIT_KEY_SENSITIVITY * dt;
		pressed = true;
	}
	orbit->pitch_deg = clampf(orbit->pitch_deg, -ORBIT_PITCH_LIMIT_DEG,
			ORBIT_PITCH_LIMIT_DEG);
	if (pressed)
	{
		orbit->lerping = false;
		orbit->idle_sec = 0.0f;
	}
}

void	orbit_camera_update(t_orbit_camera *orbit, Camera3D *camera,
	float dt, bool allow_input)
{
	Vector2	delta;
	bool	input_seen;

	input_seen = false;
	if (allow_input)
	{
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
			orbit->pitch_deg -= delta.y * ORBIT_SENSITIVITY;
			orbit->pitch_deg = clampf(orbit->pitch_deg,
					-ORBIT_PITCH_LIMIT_DEG, ORBIT_PITCH_LIMIT_DEG);
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
	}
	if (orbit->lerping)
		advance_lerp(orbit, dt);
	else if (input_seen)
		orbit->idle_sec = 0.0f;
	else if (allow_input)
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

/// @brief Which world axis a (near-)axis-aligned vector points along —
///        used both on a raw hit point (which of the puzzle's 6 outer
///        faces it landed on) and on a rotation-axis cross product
///        (§1.1/B1).
static t_axis	vector_to_axis(Vector3 v)
{
	if (fabsf(v.x) >= fabsf(v.y) && fabsf(v.x) >= fabsf(v.z))
		return (AXIS_X);
	if (fabsf(v.y) >= fabsf(v.z))
		return (AXIS_Y);
	return (AXIS_Z);
}

/// @brief `v`'s component along world axis `i` (0=x, 1=y, 2=z).
static float	vcomp(Vector3 v, int i)
{
	if (i == 0)
		return (v.x);
	if (i == 1)
		return (v.y);
	return (v.z);
}

/// @brief The unit vector along world axis `i`.
static Vector3	axis_unit(int i)
{
	return ((Vector3){i == 0, i == 1, i == 2});
}

/// @brief A world coordinate rounded to the nearest lattice slot
///        (-1, 0, or 1) — used to read off a hit point's non-picked-face
///        coordinates as a cubie slot.
static int8_t	slot_from_coord(float c)
{
	return ((int8_t)clampf(roundf(c / CUBIE_SPACING), -1.0f, 1.0f));
}

/// @brief One box for the whole puzzle: a seam click can no longer leak
///        through to an interior neighbour's face (§1.4/B4). The normal
///        and cubie slot both come from the hit point itself, never from
///        GetRayCollisionBox()'s own (truncated-to-int) normal.
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
	box.min = (Vector3){-CUBE_HALF_EXTENT, -CUBE_HALF_EXTENT,
		-CUBE_HALF_EXTENT};
	box.max = (Vector3){CUBE_HALF_EXTENT, CUBE_HALF_EXTENT,
		CUBE_HALF_EXTENT};
	hit = GetRayCollisionBox(GetScreenToWorldRay(GetMousePosition(),
			camera), box);
	if (!hit.hit)
		return (false);
	n = vector_to_axis(hit.point);
	drag->normal = Vector3Scale(axis_unit(n),
			vcomp(hit.point, n) > 0.0f ? 1.0f : -1.0f);
	i = -1;
	k = 0;
	while (++i < 3)
	{
		if (i == n)
			drag->cubie_pos[i] = (vcomp(hit.point, i) > 0.0f) ? 1 : -1;
		else
		{
			drag->cubie_pos[i] = slot_from_coord(vcomp(hit.point, i));
			drag->plane_dir[k++] = axis_unit(i);
		}
	}
	center = (Vector3){drag->cubie_pos[0] * CUBIE_SPACING,
		drag->cubie_pos[1] * CUBIE_SPACING, drag->cubie_pos[2]
		* CUBIE_SPACING};
	origin = GetWorldToScreen(center, camera);
	k = -1;
	while (++k < 2)
		drag->screen_dir[k] = Vector2Subtract(GetWorldToScreen(
				Vector3Add(center, drag->plane_dir[k]), camera), origin);
	drag->start_mouse = GetMousePosition();
	drag->active = true;
	return (true);
}

/// @brief Called once, the first frame a drag leaves the dead zone:
///        picks whichever of plane_dir[0]/[1] the total displacement
///        `d` best lines up with, then derives a SIGNED rotation axis
///        as normal x plane_dir[chosen] (§1.1/B1) — this is what the
///        old plane_right/plane_up split lost, inverting 9 of 12
///        face/direction cases. Never called again for this drag
///        (§1.2/B2): axis/layer/sign/chosen are fixed here for good.
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
			score[k] = fabsf(Vector2DotProduct(d, drag->screen_dir[k]))
				/ len;
	}
	drag->chosen = (score[1] > score[0]);
	rot = Vector3CrossProduct(drag->normal, drag->plane_dir[drag->chosen]);
	drag->axis = vector_to_axis(rot);
	drag->sign = (vcomp(rot, drag->axis) > 0.0f) ? 1.0f : -1.0f;
	drag->layer = drag->cubie_pos[drag->axis];
	drag->blocked = (drag->layer == 0);
	drag->locked = true;
}

/// @brief True for a face's own dead-centre cubie (exactly one non-zero
///        lattice coordinate) — the ONE pick where every straight-line
///        drag direction is a middle slice (§2.3/S1), so it gets the
///        circular-turn treatment (lock_center()) instead of ever being
///        `blocked`. An edge (two non-zero) or corner (three) always has
///        at least one real straight-line direction and keeps the
///        normal lock_axis() path.
static bool	is_center_pick(const int8_t cubie_pos[3])
{
	int	nonzero;

	nonzero = (cubie_pos[0] != 0) + (cubie_pos[1] != 0)
		+ (cubie_pos[2] != 0);
	return (nonzero == 1);
}

/// @brief The clicked centre's angle around its own face, measured in
///        the (screen_dir[0], screen_dir[1]) basis (each leg scaled down
///        to px-per-world-unit like the straight-drag path, §2.6/S4, so
///        a face seen at a steep angle doesn't warp the circle). Not a
///        true screen-space angle — an approximation good enough for an
///        optional gesture, not a precision measurement.
static float	center_angle_deg(const t_drag_state *drag, Vector2 d)
{
	float	len0;
	float	len1;
	float	u;
	float	v;

	len0 = fmaxf(Vector2Length(drag->screen_dir[0]), DRAG_MIN_PX_PER_UNIT);
	len1 = fmaxf(Vector2Length(drag->screen_dir[1]), DRAG_MIN_PX_PER_UNIT);
	u = Vector2DotProduct(d, drag->screen_dir[0]) / len0;
	v = Vector2DotProduct(d, drag->screen_dir[1]) / len1;
	return (atan2f(v, u) * RAD2DEG);
}

/// @brief Called once, the first frame a centre-pick drag leaves the
///        dead zone: `axis`/`layer` are the face's OWN axis/layer (not a
///        cross-product result — there is no "other" layer to resolve,
///        a centre pick only ever means turning this face), and
///        `accum_deg` starts at 0 sweep rather than a linear angle.
static void	lock_center(t_drag_state *drag, Vector2 d)
{
	drag->center_turn = true;
	drag->axis = vector_to_axis(drag->normal);
	drag->layer = drag->cubie_pos[drag->axis];
	drag->blocked = false;
	drag->center_prev_angle_deg = center_angle_deg(drag, d);
	drag->locked = true;
}

/// @brief Per-frame update for a centre-pick drag: measures the SIGNED
///        change in angle since last frame (never an absolute angle,
///        which would jump at +/-180) and accumulates it — everything
///        past this point (the +/-180 clamp, the flick-velocity EMA)
///        is identical to update_linear_angle() below, so
///        input_pick_release() needs no special case for a circular
///        drag at all.
static void	update_center_angle(t_drag_state *drag, Vector2 d, float dt)
{
	float	angle;
	float	delta;
	float	prev;

	angle = center_angle_deg(drag, d);
	delta = angle - drag->center_prev_angle_deg;
	if (delta > 180.0f)
		delta -= 360.0f;
	else if (delta < -180.0f)
		delta += 360.0f;
	drag->center_prev_angle_deg = angle;
	prev = drag->accum_deg;
	drag->accum_deg = clampf(drag->accum_deg + delta, -180.0f, 180.0f);
	if (dt > 0.0f)
		drag->vel_deg_per_sec = 0.7f * drag->vel_deg_per_sec
			+ 0.3f * (drag->accum_deg - prev) / dt;
}

/// @brief Per-frame update for a normal (non-centre) drag: total
///        displacement `d` projected onto the locked plane_dir, turned
///        into a world-space arc angle (§2.6/S4), rubber-banded if
///        `blocked` (§2.3/S1 option A).
static void	update_linear_angle(t_drag_state *drag, Vector2 d, float dt)
{
	Vector2	sd;
	float	len2;
	float	raw;
	float	prev;

	sd = drag->screen_dir[drag->chosen];
	len2 = fmaxf(Vector2DotProduct(sd, sd),
			DRAG_MIN_PX_PER_UNIT * DRAG_MIN_PX_PER_UNIT);
	raw = drag->sign * (Vector2DotProduct(d, sd) / len2) / DRAG_RADIUS
		* RAD2DEG;
	raw = clampf(raw, -180.0f, 180.0f);
	if (drag->blocked)
		raw = DRAG_BLOCK_DEG * tanhf(raw / DRAG_BLOCK_DEG);
	prev = drag->accum_deg;
	drag->accum_deg = raw;
	if (dt > 0.0f)
		drag->vel_deg_per_sec = 0.7f * drag->vel_deg_per_sec
			+ 0.3f * (raw - prev) / dt;
}

void	input_pick_drag(t_drag_state *drag, Vector2 mouse, float dt)
{
	Vector2	d;

	if (!drag->active)
		return ;
	d = Vector2Subtract(mouse, drag->start_mouse);
	if (!drag->locked)
	{
		if (Vector2Length(d) < DRAG_DEADZONE_PX)
			return ;
		if (is_center_pick(drag->cubie_pos))
			lock_center(drag, d);
		else
			lock_axis(drag, d);
	}
	if (drag->center_turn)
		update_center_angle(drag, d, dt);
	else
		update_linear_angle(drag, d, dt);
}

t_move	input_pick_release(t_drag_state *drag, float *from_deg,
	float *to_deg)
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
	drag->center_turn = false;
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
