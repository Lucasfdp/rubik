#include <time.h>
#include "raylib.h"
#include "rubik.h"
#include "scramble.h"
#include "render.h"
#include "render/app.h"
#include "render/draw.h"
#include "render/hud.h"

# define WINDOW_WIDTH 1280
# define WINDOW_HEIGHT 720
# define SCRAMBLE_LEN 20
# define AUTO_LOOP_WAIT_SEC 2.0f
# define SCRUB_STEP 10

/// @brief Runs the existing anti-cheat solver pipeline (build tables,
///        solve, free tables — same shape as main.c's own
///        solve_and_print()), queues every move of the result for
///        autoplay, and records it into app->solution_moves/
///        solution_start_cube so Phase 7's scrub/reverse/auto-loop
///        (section 9.6) can replay it later. Shared by Mode A's startup
///        autoplay, Mode C's "solve for me", and auto-loop's own re-
///        solve (they are all the exact same pipeline, just triggered at
///        different times).
///
/// @return The number of moves queued (0 if already solved or the
///         solver could not run).
static int	queue_solution(t_app *app)
{
	t_solver	solver;
	int			count;
	int			i;

	if (!solver_init(&solver))
		return (0);
	count = solve(&solver, &app->cube, app->solution_moves);
	solver_free(&solver);
	if (count <= 0)
		return (0);
	app->solution_start_cube = app->cube;
	app->solution_move_count = count;
	app->solution_scrubbable = true;
	i = 0;
	while (i < count && anim_push(&app->anim, app->solution_moves[i]))
		i++;
	return (count);
}

static void	init_app(t_app *app, const t_cube *start_cube, bool has_scramble)
{
	app->cube = *start_cube;
	geometry_init(&app->scene);
	geometry_sync(&app->scene, &app->cube);
	anim_init(&app->anim);
	history_init(&app->history);
	app->stats = (t_session_stats){0};
	app->rng_seed = (unsigned int)time(NULL);
	app->lighting = draw_lighting_load();
	app->fx = fx_load();
	app->drag = (t_drag_state){0};
	app->orbit = (t_orbit_camera){.yaw_deg = 40.0f, .pitch_deg = 25.0f,
		.distance = 11.0f};
	app->camera = (Camera3D){.position = {6.0f, 6.0f, 8.0f},
		.target = {0.0f, 0.0f, 0.0f}, .up = {0.0f, 1.0f, 0.0f},
		.fovy = 45.0f, .projection = CAMERA_PERSPECTIVE};
	app->mode = MODE_MANUAL;
	app->solution_count = 0;
	app->solution_move_count = 0;
	app->solution_scrubbable = false;
	app->rounded_corners = false;
	app->auto_loop = false;
	app->auto_loop_wait_sec = 0.0f;
	app->was_solved = cube_is_solved(&app->cube);
	if (has_scramble)
	{
		app->solution_count = queue_solution(app);
		if (app->solution_count > 0)
			app->mode = MODE_AUTOPLAY;
	}
}

/// @brief Generates a fresh scramble, queues it for autoplay, records it
///        into app->solution_moves/solution_start_cube (Phase 7 §9.6),
///        and resets the practice session (undo/redo history and stats)
///        — a new scramble makes the previous cube state's history
///        meaningless.
static void	do_scramble(t_app *app)
{
	int	i;

	scramble_generate(app->solution_moves, SCRAMBLE_LEN, &app->rng_seed);
	app->solution_start_cube = app->cube;
	app->solution_move_count = SCRAMBLE_LEN;
	app->solution_scrubbable = true;
	i = 0;
	while (i < SCRAMBLE_LEN)
	{
		anim_push(&app->anim, app->solution_moves[i]);
		i++;
	}
	app->solution_count = SCRAMBLE_LEN;
	app->mode = MODE_AUTOPLAY;
	history_init(&app->history);
	app->stats = (t_session_stats){0};
}

/// @brief One frame of Mode A (autoplay): advances playback, handles the
///        transport keys (pause, step, speed, escape-to-manual), and —
///        when auto_loop is on (Phase 7 §9.6) — chains scramble -> solve
///        -> (2s wait) -> scramble again once the queue drains, instead
///        of dropping back to manual.
static void	tick_autoplay(t_app *app, float dt)
{
	anim_update(&app->anim, &app->scene, &app->cube, &app->fx, dt);
	if (IsKeyPressed(KEY_SPACE))
		anim_toggle_pause(&app->anim);
	if (IsKeyPressed(KEY_RIGHT))
		anim_step_one(&app->anim, &app->scene, &app->cube, &app->fx);
	if (IsKeyPressed(KEY_UP))
		anim_set_speed(&app->anim, app->anim.speed_deg_per_sec * 1.25f);
	if (IsKeyPressed(KEY_DOWN))
		anim_set_speed(&app->anim, app->anim.speed_deg_per_sec * 0.8f);
	if (IsKeyPressed(KEY_ESCAPE) && !anim_is_idle(&app->anim))
	{
		anim_flush(&app->anim);
		app->auto_loop = false;
	}
	if (!anim_is_idle(&app->anim))
		return ;
	if (!app->auto_loop)
	{
		app->mode = MODE_MANUAL;
		return ;
	}
	if (!cube_is_solved(&app->cube))
	{
		queue_solution(app);
		return ;
	}
	app->auto_loop_wait_sec += dt;
	if (app->auto_loop_wait_sec >= AUTO_LOOP_WAIT_SEC)
	{
		app->auto_loop_wait_sec = 0.0f;
		do_scramble(app);
	}
}

/// @brief The one place a manual (Mode C) move enters the animation
///        queue — keyboard turns, mouse-drag turns, AND undo/redo
///        replays, so stats/timing bookkeeping never has a second path
///        to drift out of sync with. `record` is true only for a
///        genuinely new turn (keyboard or mouse): an undo/redo replay
///        must NOT re-record itself, or it would defeat its own
///        redo/undo stack. Any call here — new or replayed — means the
///        live cube has diverged from whatever solution/scramble
///        app->solution_moves last tracked, so Phase 7 §9.6's scrub
///        controls are retired until the next fresh one.
static void	push_manual_move(t_app *app, t_move move, bool record)
{
	app->solution_scrubbable = false;
	if (!anim_push(&app->anim, move))
		return ;
	if (record)
		history_record(&app->history, move);
	if (!app->stats.timing)
	{
		app->stats.timing = true;
		app->stats.timer_start_sec = GetTime();
	}
	app->stats.move_count++;
}

/// @brief Runs the solver on the CURRENT live cube (not a separately
///        tracked "what the user thinks the state is" copy) and queues
///        its output for autoplay. No-op if already solved.
static void	solve_for_me(t_app *app)
{
	int	count;

	if (cube_is_solved(&app->cube))
		return ;
	count = queue_solution(app);
	if (count > 0)
	{
		app->solution_count = count;
		app->mode = MODE_AUTOPLAY;
	}
}

/// @brief Phase 7 §9.6's "jump to move N" (the +-SCRUB_STEP button
///        version): resets the cube to the tracked solution/scramble's
///        start, replays moves [0, target) instantly, then re-queues
///        [target, count) so animated playback resumes from there. The
///        current position is read back from anim's own pending count
///        rather than kept as separate state — valid exactly as long as
///        app->solution_scrubbable holds (see push_manual_move()).
static void	solution_jump(t_app *app, int delta)
{
	int	current;
	int	target;
	int	i;

	current = app->solution_move_count - (int)anim_pending_count(&app->anim);
	target = current + delta;
	if (target < 0)
		target = 0;
	if (target > app->solution_move_count)
		target = app->solution_move_count;
	app->cube = app->solution_start_cube;
	i = 0;
	while (i < target)
	{
		apply_move(&app->cube, app->solution_moves[i]);
		i++;
	}
	geometry_sync(&app->scene, &app->cube);
	anim_flush(&app->anim);
	while (i < app->solution_move_count)
	{
		anim_push(&app->anim, app->solution_moves[i]);
		i++;
	}
	if (target < app->solution_move_count)
		app->mode = MODE_AUTOPLAY;
}

/// @brief Phase 7 §9.6's reverse playback: queues the tracked solution's
///        moves inverted and back-to-front, so autoplay un-does it from
///        wherever the cube currently sits (the end state — cube is left
///        untouched here; each inverse commits normally as it plays).
static void	solution_reverse(t_app *app)
{
	int	i;

	anim_flush(&app->anim);
	i = app->solution_move_count - 1;
	while (i >= 0)
	{
		anim_push(&app->anim, move_inverse(app->solution_moves[i]));
		i--;
	}
	app->mode = MODE_AUTOPLAY;
}

/// @brief One frame of Mode C's mouse half of the button split: starts a
///        drag on left-mouse-down, updates it while held, and on release
///        resolves it into a real move through the exact same
///        anim_push()-via-push_manual_move() path keyboard turns use
///        (Phase 6, docs/en/03b-3d-implementation-plan.md section 8).
static void	tick_manual_mouse(t_app *app)
{
	t_move	move;

	if (!app->drag.active && IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
		input_pick_start(&app->drag, &app->scene, app->camera);
	else if (app->drag.active && IsMouseButtonReleased(MOUSE_BUTTON_LEFT))
	{
		move = input_pick_release(&app->drag);
		if (move != MOVE_COUNT)
			push_manual_move(app, move, true);
	}
	else if (app->drag.active)
		input_pick_drag(&app->drag, GetMouseDelta());
}

/// @brief One frame of Mode C (manual): reads at most one input source —
///        an in-progress mouse drag, a scramble/solve/undo/redo/scrub
///        key, or a keyboard face turn — and feeds any resulting move
///        into the SAME anim_push()/anim_update() pipeline autoplay
///        uses. Also tracks the practice-session timer: started by the
///        first manual move, stopped the instant the cube reads solved.
static void	tick_manual(t_app *app, float dt)
{
	t_move	move;
	t_move	inverse;

	if (anim_is_idle(&app->anim))
	{
		if (app->drag.active || IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
			tick_manual_mouse(app);
		else if (IsKeyPressed(KEY_S))
			do_scramble(app);
		else if (IsKeyPressed(KEY_ENTER))
			solve_for_me(app);
		else if (IsKeyPressed(KEY_Z) && history_undo(&app->history, &inverse))
			push_manual_move(app, inverse, false);
		else if (IsKeyPressed(KEY_Y) && history_redo(&app->history, &move))
			push_manual_move(app, move, false);
		else if (app->solution_scrubbable && IsKeyPressed(KEY_LEFT_BRACKET))
			solution_jump(app, -SCRUB_STEP);
		else if (app->solution_scrubbable && IsKeyPressed(KEY_RIGHT_BRACKET))
			solution_jump(app, SCRUB_STEP);
		else if (app->solution_scrubbable && IsKeyPressed(KEY_BACKSLASH))
			solution_reverse(app);
		else
		{
			move = input_poll_keyboard();
			if (move != MOVE_COUNT)
				push_manual_move(app, move, true);
		}
	}
	anim_update(&app->anim, &app->scene, &app->cube, &app->fx, dt);
	if (app->stats.timing)
	{
		app->stats.elapsed_sec = GetTime() - app->stats.timer_start_sec;
		if (cube_is_solved(&app->cube))
			app->stats.timing = false;
	}
}

bool	render_run(const t_cube *start_cube, bool has_scramble)
{
	t_app			app;
	float			dt;
	t_active_turn	turn;
	bool			now_solved;

	InitWindow(WINDOW_WIDTH, WINDOW_HEIGHT, "rubik -- 3D bonus");
	init_app(&app, start_cube, has_scramble);
	while (!WindowShouldClose())
	{
		dt = GetFrameTime();
		orbit_camera_update(&app.orbit, &app.camera, dt);
		if (IsKeyPressed(KEY_FIVE))
			orbit_camera_preset(&app.orbit, 0);
		if (IsKeyPressed(KEY_SIX))
			orbit_camera_preset(&app.orbit, 1);
		if (IsKeyPressed(KEY_SEVEN))
			orbit_camera_preset(&app.orbit, 2);
		if (IsKeyPressed(KEY_EIGHT))
			orbit_camera_preset(&app.orbit, 3);
		if (IsKeyPressed(KEY_P))
		{
			geometry_palette_cycle();
			geometry_sync(&app.scene, &app.cube);
		}
		if (IsKeyPressed(KEY_C))
			app.rounded_corners = !app.rounded_corners;
		if (IsKeyPressed(KEY_A))
		{
			app.auto_loop = !app.auto_loop;
			if (!app.auto_loop)
			{
				anim_flush(&app.anim);
				app.mode = MODE_MANUAL;
			}
			else if (app.mode == MODE_MANUAL)
				do_scramble(&app);
		}
		if (app.mode == MODE_AUTOPLAY)
			tick_autoplay(&app, dt);
		else
			tick_manual(&app, dt);
		now_solved = cube_is_solved(&app.cube);
		if (now_solved && !app.was_solved)
			fx_spawn_celebration(&app.fx);
		app.was_solved = now_solved;
		turn = anim_get_active_turn(&app.anim);
		if (!turn.active)
			turn = input_drag_get_active_turn(&app.drag);
		draw_lighting_update_camera(&app.lighting, app.camera);
		BeginDrawing();
		ClearBackground(RAYWHITE);
		DrawRectangleGradientV(0, 0, GetScreenWidth(), GetScreenHeight(),
			(Color){235, 244, 255, 255}, (Color){250, 250, 248, 255});
		BeginMode3D(app.camera);
		draw_scene(&app.scene, &turn, &app.lighting, app.rounded_corners);
		fx_update_and_draw(&app.fx, dt);
		EndMode3D();
		hud_draw(&app.anim, app.mode, app.solution_count,
			app.stats.elapsed_sec, app.stats.move_count, app.auto_loop);
		EndDrawing();
	}
	fx_unload(&app.fx);
	draw_lighting_unload(&app.lighting);
	CloseWindow();
	return (true);
}
