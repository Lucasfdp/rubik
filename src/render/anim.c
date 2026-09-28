#include <math.h>
#include "render/anim.h"

# define ANIM_DEFAULT_SPEED 360.0f
# define ANIM_MIN_SPEED 60.0f
# define ANIM_MAX_SPEED 720.0f

/// One entry per t_move (18). axis/layer/quarter_deg describe ONE
/// application of that move as a rotation of the matching world axis
/// (see render/geometry.h's t_axis comment for the +X=R/+Y=U/+Z=F
/// convention this is built on).
///
/// These signs are not a guess: a throwaway script built cube.h's own
/// FACE_TABLES permutation as real 3D rotation matrices and solved, per
/// face, which signed world-axis rotation reproduces that exact
/// corner/edge cycle — then replayed 5,000 random moves to confirm no
/// mismatch. U/R/F clockwise all come out as -90 deg about their world
/// axis; D/L/B clockwise are +90 — the two are NOT symmetric the naive
/// way, which is exactly the trap this table's comment in the design doc
/// warned about.
typedef struct s_move_axis
{
	t_axis	axis;
	int8_t	layer;
	float	quarter_deg;
}	t_move_axis;

static const t_move_axis	MOVE_AXIS[MOVE_COUNT] = {
	[MOVE_U1] = {AXIS_Y, 1, -90.0f},
	[MOVE_U2] = {AXIS_Y, 1, -180.0f},
	[MOVE_U3] = {AXIS_Y, 1, 90.0f},
	[MOVE_R1] = {AXIS_X, 1, -90.0f},
	[MOVE_R2] = {AXIS_X, 1, -180.0f},
	[MOVE_R3] = {AXIS_X, 1, 90.0f},
	[MOVE_F1] = {AXIS_Z, 1, -90.0f},
	[MOVE_F2] = {AXIS_Z, 1, -180.0f},
	[MOVE_F3] = {AXIS_Z, 1, 90.0f},
	[MOVE_D1] = {AXIS_Y, -1, 90.0f},
	[MOVE_D2] = {AXIS_Y, -1, 180.0f},
	[MOVE_D3] = {AXIS_Y, -1, -90.0f},
	[MOVE_L1] = {AXIS_X, -1, 90.0f},
	[MOVE_L2] = {AXIS_X, -1, 180.0f},
	[MOVE_L3] = {AXIS_X, -1, -90.0f},
	[MOVE_B1] = {AXIS_Z, -1, 90.0f},
	[MOVE_B2] = {AXIS_Z, -1, 180.0f},
	[MOVE_B3] = {AXIS_Z, -1, -90.0f},
};

static float	ease_in_out_cubic(float t)
{
	if (t < 0.5f)
		return (4.0f * t * t * t);
	return (1.0f - powf(-2.0f * t + 2.0f, 3.0f) / 2.0f);
}

static float	clampf(float value, float lo, float hi)
{
	if (value < lo)
		return (lo);
	if (value > hi)
		return (hi);
	return (value);
}

void	anim_init(t_anim_state *state)
{
	state->head = 0;
	state->tail = 0;
	state->active = false;
	state->elapsed_sec = 0.0f;
	state->duration_sec = 0.0f;
	state->angle_deg = 0.0f;
	state->target_deg = 0.0f;
	state->speed_deg_per_sec = ANIM_DEFAULT_SPEED;
	state->paused = false;
}

bool	anim_push(t_anim_state *state, t_move move)
{
	size_t	next_tail;

	next_tail = (state->tail + 1) % ANIM_QUEUE_CAP;
	if (next_tail == state->head)
		return (false);
	state->queue[state->tail] = move;
	state->tail = next_tail;
	return (true);
}

void	anim_update(t_anim_state *state, t_render_scene *scene,
	t_cube *cube, float dt)
{
	float	frac;

	if (!state->active)
	{
		if (state->head == state->tail)
			return ;
		state->current = state->queue[state->head];
		state->head = (state->head + 1) % ANIM_QUEUE_CAP;
		state->elapsed_sec = 0.0f;
		state->target_deg = MOVE_AXIS[state->current].quarter_deg;
		state->duration_sec = fabsf(state->target_deg)
			/ state->speed_deg_per_sec;
		state->active = true;
	}
	if (state->paused)
		return ;
	state->elapsed_sec += dt;
	frac = state->elapsed_sec / state->duration_sec;
	if (frac >= 1.0f)
	{
		state->angle_deg = 0.0f;
		state->active = false;
		apply_move(cube, state->current);
		geometry_sync(scene, cube);
	}
	else
		state->angle_deg = state->target_deg * ease_in_out_cubic(frac);
}

bool	anim_is_idle(const t_anim_state *state)
{
	return (!state->active && state->head == state->tail);
}

void	anim_toggle_pause(t_anim_state *state)
{
	state->paused = !state->paused;
}

void	anim_step_one(t_anim_state *state, t_render_scene *scene,
	t_cube *cube)
{
	if (!state->active && state->head == state->tail)
		return ;
	state->paused = false;
	if (!state->active)
		anim_update(state, scene, cube, 0.0f);
	anim_update(state, scene, cube,
		state->duration_sec - state->elapsed_sec + 0.001f);
	state->paused = true;
}

void	anim_set_speed(t_anim_state *state, float deg_per_sec)
{
	state->speed_deg_per_sec = clampf(deg_per_sec, ANIM_MIN_SPEED,
			ANIM_MAX_SPEED);
}

void	anim_flush(t_anim_state *state)
{
	state->head = 0;
	state->tail = 0;
}

t_active_turn	anim_get_active_turn(const t_anim_state *state)
{
	t_active_turn	turn;

	turn.active = state->active;
	turn.axis = AXIS_X;
	turn.layer = 0;
	turn.angle_deg = 0.0f;
	if (state->active)
	{
		turn.axis = MOVE_AXIS[state->current].axis;
		turn.layer = MOVE_AXIS[state->current].layer;
		turn.angle_deg = state->angle_deg;
	}
	return (turn);
}

size_t	anim_pending_count(const t_anim_state *state)
{
	size_t	count;

	count = (state->tail + ANIM_QUEUE_CAP - state->head) % ANIM_QUEUE_CAP;
	if (state->active)
		count++;
	return (count);
}
