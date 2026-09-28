#include <math.h>
#include "render/input.h"
#include "render/anim.h"
#include "testlib.h"

/// Tests for the mouse click-and-drag turning math (docs/en/11-drag-
/// review.md §6.1): input_pick_drag()'s axis lock + angle accumulation,
/// input_pick_release()'s snap, and anim_move_for_turn()'s half-turn
/// matching. Links only input.c + anim.c + geometry.c + fx.c + cubie.c +
/// moves.c — never main.c or the solver. Opt-in only (`make test_drag` /
/// `make test_bonus`), never part of `make test` (see the Makefile
/// comment by DR_SRCS): src/render/ needs real raylib, and the
/// mandatory build/test path must never depend on that being present.
///
/// None of these tests open a window or touch a real camera: every
/// t_drag_state is built by hand via setup_drag() below, reproducing
/// exactly what input_pick_start() itself would have written into
/// normal/plane_dir/cubie_pos for a given clicked face+slot — the "pull
/// the pure math out so it's testable" approach §6.1 asks for, without
/// exporting anything new from input.c. screen_dir is a trivial
/// synthetic (1,0)/(0,1) mapping rather than a real camera projection:
/// input_pick_drag()'s math only cares that the two are distinguishable
/// directions, not what a real screen projection of them looks like.

static Vector3	unit_axis(int i)
{
	if (i == 0)
		return ((Vector3){1.0f, 0.0f, 0.0f});
	if (i == 1)
		return ((Vector3){0.0f, 1.0f, 0.0f});
	return ((Vector3){0.0f, 0.0f, 1.0f});
}

/// @brief Reproduces input_pick_start()'s own field population for a
///        clicked face (`normal_axis`/`normal_sign`) on a cubie at fixed
///        slot `cubie_pos`, minus the actual raycast: plane_dir[0]/[1]
///        are the two OTHER world axes in ascending order (exactly
///        input_pick_start()'s loop), never dependent on sign — the
///        cross product with the signed `normal` is what carries B1's
///        fix, not the plane_dir choice.
static void	setup_drag(t_drag_state *drag, int normal_axis,
	float normal_sign, const int8_t cubie_pos[3])
{
	int	i;
	int	k;

	*drag = (t_drag_state){0};
	drag->normal = (Vector3){unit_axis(normal_axis).x * normal_sign,
		unit_axis(normal_axis).y * normal_sign,
		unit_axis(normal_axis).z * normal_sign};
	i = -1;
	k = 0;
	while (++i < 3)
	{
		drag->cubie_pos[i] = cubie_pos[i];
		if (i != normal_axis)
			drag->plane_dir[k++] = unit_axis(i);
	}
	drag->screen_dir[0] = (Vector2){1.0f, 0.0f};
	drag->screen_dir[1] = (Vector2){0.0f, 1.0f};
	drag->start_mouse = (Vector2){0.0f, 0.0f};
	drag->active = true;
}

/// @brief B1 + B6 oracle: three independently-derived face/direction
///        cases (a = normal x drag_dir, then MOVE_AXIS's own asymmetric
///        cw/ccw signs — U/R/F clockwise are -90 about their axis,
///        D/L/B clockwise are +90, the "trap" §2.2 warns about). The F
///        case reproduces the review doc's own stated repro verbatim:
///        "front sticker, right column, drag up -> R" (before the fix,
///        R').
static void	test_direction_oracle(void)
{
	t_drag_state	drag;
	const int8_t	front_right[3] = {1, 1, 1};
	const int8_t	right_front[3] = {1, 0, 1};
	const int8_t	down_left[3] = {-1, -1, 0};
	float			from;
	float			to;

	setup_drag(&drag, 2, 1.0f, front_right);
	input_pick_drag(&drag, (Vector2){0.0f, 10.0f}, 1.0f / 60.0f);
	CHECK(drag.locked && !drag.blocked);
	CHECK_EQ(drag.axis, AXIS_X);
	CHECK(drag.sign < 0.0f);
	drag.accum_deg = -90.0f;
	drag.vel_deg_per_sec = 0.0f;
	CHECK_EQ(input_pick_release(&drag, &from, &to), MOVE_R1);
	setup_drag(&drag, 2, 1.0f, front_right);
	input_pick_drag(&drag, (Vector2){0.0f, 10.0f}, 1.0f / 60.0f);
	drag.accum_deg = 90.0f;
	drag.vel_deg_per_sec = 0.0f;
	CHECK_EQ(input_pick_release(&drag, &from, &to), MOVE_R3);
	setup_drag(&drag, 0, 1.0f, right_front);
	input_pick_drag(&drag, (Vector2){10.0f, 0.0f}, 1.0f / 60.0f);
	CHECK(drag.locked && !drag.blocked);
	CHECK_EQ(drag.axis, AXIS_Z);
	CHECK(drag.sign > 0.0f);
	drag.accum_deg = -90.0f;
	drag.vel_deg_per_sec = 0.0f;
	CHECK_EQ(input_pick_release(&drag, &from, &to), MOVE_F1);
	setup_drag(&drag, 1, -1.0f, down_left);
	input_pick_drag(&drag, (Vector2){0.0f, 10.0f}, 1.0f / 60.0f);
	CHECK(drag.locked && !drag.blocked);
	CHECK_EQ(drag.axis, AXIS_X);
	CHECK(drag.sign < 0.0f);
	drag.accum_deg = 90.0f;
	drag.vel_deg_per_sec = 0.0f;
	CHECK_EQ(input_pick_release(&drag, &from, &to), MOVE_L1);
}

/// @brief B6 regression, isolated from the drag geometry entirely: a
///        half turn must match anim_move_for_turn() regardless of which
///        of the two signs of 180 the drag actually reached, since
///        MOVE_AXIS stores each half turn with one fixed sign.
static void	test_half_turn_either_sign(void)
{
	CHECK_EQ(anim_move_for_turn(AXIS_Y, 1, 180.0f), MOVE_U2);
	CHECK_EQ(anim_move_for_turn(AXIS_Y, 1, -180.0f), MOVE_U2);
	CHECK_EQ(anim_move_for_turn(AXIS_Y, -1, 180.0f), MOVE_D2);
	CHECK_EQ(anim_move_for_turn(AXIS_Y, -1, -180.0f), MOVE_D2);
}

/// @brief B2 regression: once locked, axis/sign/chosen never change for
///        the rest of the drag, even on a frame whose mouse position
///        would (under the old per-frame-delta bug) suggest a different
///        axis — a frame near the start point, the exact "still frame
///        right before release" trap §1.2 describes.
static void	test_axis_locked_once(void)
{
	t_drag_state	drag;
	const int8_t	pos[3] = {1, 1, 1};
	t_axis			locked_axis;
	float			locked_sign;
	int				locked_chosen;

	setup_drag(&drag, 2, 1.0f, pos);
	input_pick_drag(&drag, (Vector2){0.0f, 10.0f}, 1.0f / 60.0f);
	CHECK(drag.locked);
	locked_axis = drag.axis;
	locked_sign = drag.sign;
	locked_chosen = drag.chosen;
	input_pick_drag(&drag, (Vector2){0.0f, 0.0f}, 1.0f / 60.0f);
	CHECK_EQ(drag.axis, locked_axis);
	CHECK_EQ(drag.chosen, locked_chosen);
	CHECK(drag.sign == locked_sign);
	input_pick_drag(&drag, (Vector2){9.0f, -3.0f}, 1.0f / 60.0f);
	CHECK_EQ(drag.axis, locked_axis);
	CHECK_EQ(drag.chosen, locked_chosen);
}

/// @brief S1 regression: an EDGE sticker dragged along its slice
///        direction (the picked axis's cubie_pos coordinate is 0 — a
///        true face-CENTRE, covered separately below, gets the S1
///        circular-turn extra instead and is never `blocked`) rubber-
///        bands within +/-DRAG_BLOCK_DEG no matter how far the mouse
///        travels, and always releases to MOVE_COUNT.
static void	test_middle_slice_blocked(void)
{
	t_drag_state	drag;
	const int8_t	edge[3] = {1, 0, 1};
	float			from;
	float			to;

	setup_drag(&drag, 2, 1.0f, edge);
	input_pick_drag(&drag, (Vector2){10.0f, 0.0f}, 1.0f / 60.0f);
	CHECK(drag.locked);
	CHECK(drag.blocked);
	CHECK(!drag.center_turn);
	input_pick_drag(&drag, (Vector2){1000000.0f, 0.0f}, 1.0f / 60.0f);
	CHECK(fabsf(drag.accum_deg) <= 12.0f);
	CHECK_EQ(input_pick_release(&drag, &from, &to), MOVE_COUNT);
	CHECK(to == 0.0f);
}

/// @brief S1 extra: a face's dead-centre sticker is never `blocked` —
///        instead it's read as a circular sweep around the face's own
///        centre, which resolves to a real turn of that face (its own
///        axis/layer) once the sweep passes the usual 45 deg snap
///        threshold, same as any other drag from here on.
static void	test_center_circular_turn(void)
{
	t_drag_state	drag;
	const int8_t	r_center[3] = {1, 0, 0};
	float			from;
	float			to;

	setup_drag(&drag, 0, 1.0f, r_center);
	input_pick_drag(&drag, (Vector2){10.0f, 0.0f}, 1.0f / 60.0f);
	CHECK(drag.locked);
	CHECK(drag.center_turn);
	CHECK(!drag.blocked);
	input_pick_drag(&drag, (Vector2){0.0f, 10.0f}, 1.0f / 60.0f);
	CHECK(drag.accum_deg > 45.0f);
	drag.vel_deg_per_sec = 0.0f;
	CHECK_EQ(input_pick_release(&drag, &from, &to), MOVE_R3);
	CHECK(to == 90.0f);
	setup_drag(&drag, 0, 1.0f, r_center);
	input_pick_drag(&drag, (Vector2){10.0f, 0.0f}, 1.0f / 60.0f);
	input_pick_drag(&drag, (Vector2){0.0f, -10.0f}, 1.0f / 60.0f);
	CHECK(drag.accum_deg < -45.0f);
	drag.vel_deg_per_sec = 0.0f;
	CHECK_EQ(input_pick_release(&drag, &from, &to), MOVE_R1);
	CHECK(to == -90.0f);
}

/// @brief S5 regression: a short drag (well under the 45 deg snap
///        threshold) that is moving FAST at release commits via the
///        velocity look-ahead, while the same short drag at rest
///        cancels — the exact "fast 30 deg flick" case §2.7 describes.
static void	test_flick_commits_short_drag(void)
{
	t_drag_state	drag;
	const int8_t	pos[3] = {1, 1, 1};
	float			from;
	float			to;

	setup_drag(&drag, 2, 1.0f, pos);
	input_pick_drag(&drag, (Vector2){0.0f, 10.0f}, 1.0f / 60.0f);
	drag.accum_deg = 20.0f;
	drag.vel_deg_per_sec = 0.0f;
	CHECK_EQ(input_pick_release(&drag, &from, &to), MOVE_COUNT);
	setup_drag(&drag, 2, 1.0f, pos);
	input_pick_drag(&drag, (Vector2){0.0f, 10.0f}, 1.0f / 60.0f);
	drag.accum_deg = 20.0f;
	drag.vel_deg_per_sec = 900.0f;
	CHECK(input_pick_release(&drag, &from, &to) != MOVE_COUNT);
	CHECK(to == 90.0f);
}

/// @brief B5 regression: a release that resolves to a real move reports
///        the drag's own angle as `from`, never 0 -- so the caller can
///        animate onward from there instead of rewinding.
static void	test_release_reports_from_angle(void)
{
	t_drag_state	drag;
	const int8_t	pos[3] = {1, 1, 1};
	float			from;
	float			to;

	setup_drag(&drag, 2, 1.0f, pos);
	input_pick_drag(&drag, (Vector2){0.0f, 10.0f}, 1.0f / 60.0f);
	drag.accum_deg = 70.0f;
	drag.vel_deg_per_sec = 0.0f;
	CHECK_EQ(input_pick_release(&drag, &from, &to), MOVE_R3);
	CHECK(from == 70.0f);
	CHECK(to == 90.0f);
}

int	main(void)
{
	test_direction_oracle();
	test_half_turn_either_sign();
	test_axis_locked_once();
	test_middle_slice_blocked();
	test_center_circular_turn();
	test_flick_commits_short_drag();
	test_release_reports_from_angle();
	return (test_report("test_drag"));
}
