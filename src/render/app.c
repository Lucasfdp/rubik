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
# define AUTO_LOOP_WAIT_SEC 2.0f
# define SCRUB_STEP 10

/// @brief Solves app->cube for whichever puzzle app->puzzle currently
///        shows: the 2x2x2 corner-only IDA* solver (twobytwo.h) for
///        PUZZLE_2X2X2 -- app->algo is not even consulted then, there is
///        only the one solver -- otherwise whichever of the three
///        PUZZLE_3X3X3 algorithms app->algo selects, same as before.
///        Writes straight into app->solution_moves (shared by all four:
///        MAX_MOVES == 256 comfortably covers Kociemba's ~23,
///        Thistlethwaite's THISTLE_MAX_MOVES == 45, the beginner
///        method's LAYER_MAX_MOVES == 200, and the 2x2x2's own
///        TWOBYTWO_MAX_MOVES == 11 alike). Each branch builds and frees
///        its own tables around the call, exactly like main.c's four
///        solve_and_print* functions.
/// @return Move count (0 if already solved or the solver could not run).
static int	solve_with_algo(t_app *app)
{
	t_solver			kociemba;
	t_move_tables		tables;
	t_thistle_solver	thistle;
	t_two_solver		two;
	int					count;

	if (app->puzzle == PUZZLE_2X2X2)
	{
		if (!two_solver_init(&two))
			return (0);
		count = two_solve(&two, &app->cube, app->solution_moves);
		two_solver_free(&two);
		return (count);
	}
	if (app->algo == ALGO_THISTLETHWAITE)
	{
		if (!move_tables_build(&tables))
			return (0);
		if (!thistle_init(&thistle, &tables))
		{
			move_tables_free(&tables);
			return (0);
		}
		count = thistle_solve(&thistle, &app->cube, app->solution_moves);
		thistle_free(&thistle);
		move_tables_free(&tables);
		return (count);
	}
	if (app->algo == ALGO_LAYER)
		return (layer_solve(&app->cube, app->solution_moves));
	if (!solver_init(&kociemba))
		return (0);
	count = solve(&kociemba, &app->cube, app->solution_moves);
	solver_free(&kociemba);
	return (count);
}

/// @brief Runs the existing anti-cheat solver pipeline (same shape as
///        main.c's solve_and_print* functions, dispatched by app->algo
///        via solve_with_algo() above), queues every move of the result
///        for autoplay, and records it into app->solution_moves/
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
	int	count;
	int	i;

	count = solve_with_algo(app);
	if (count <= 0)
		return (0);
	app->solution_start_cube = app->cube;
	app->solution_move_count = count;
	app->solution_scrubbable = true;
	app->scrambling = false;
	i = 0;
	while (i < count && anim_push(&app->anim, app->solution_moves[i]))
		i++;
	return (count);
}

/// @brief app->puzzle-aware "is app->cube solved": a 2x2x2 only ever
///        looks at the 8 corners (two_cube_is_solved(), twobytwo.h) --
///        its edges can sit scrambled and it still reads solved, exactly
///        as the CLI's own "-p 2x2x2" already treats them (docs/en/14-
///        other-puzzles.md). Centralises what would otherwise be the
///        same `app->puzzle == PUZZLE_2X2X2 ? ... : ...` at every one of
///        render_run()/tick_autoplay()/solve_for_me()/tick_manual()'s
///        "is it solved yet" checks.
static bool	app_is_solved(const t_app *app)
{
	if (app->puzzle == PUZZLE_2X2X2)
		return (two_cube_is_solved(&app->cube));
	return (cube_is_solved(&app->cube));
}

static void	init_app(t_app *app, const t_cube *start_cube, bool has_scramble,
	t_puzzle initial_puzzle)
{
	app->cube = *start_cube;
	app->puzzle = initial_puzzle;
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
	app->algo = ALGO_KOCIEMBA;
	app->solution_count = 0;
	app->solution_move_count = 0;
	app->solution_scrubbable = false;
	app->rounded_corners = false;
	app->auto_loop = false;
	app->auto_loop_wait_sec = 0.0f;
	app->scrambling = false;
	app->was_solved = app_is_solved(app);
	if (has_scramble)
	{
		app->solution_count = queue_solution(app);
		if (app->solution_count > 0)
			app->mode = MODE_AUTOPLAY;
	}
}

/// @brief K's keyboard puzzle switch: flips app->puzzle between the two
///        t_puzzle views. Never touches app->cube/app->scene at all --
///        both views are the exact same full cube and the exact same 26
///        synced slots, geometry_visible_count()/geometry_body_size()
///        (geometry.h) are what actually decide, every frame, which of
///        those slots draw_scene() and input_pick_start() see, so there
///        is nothing here to resync. Resets was_solved against the
///        NEWLY-selected view so switching itself never fires a false
///        solve celebration (a scrambled-edges cube that reads solved
///        the instant it becomes a 2x2x2, or the reverse). render_run()
///        only calls this while anim_is_idle() and no drag is active, so
///        a turn can never be caught mid-spin by the switch.
static void	switch_puzzle(t_app *app)
{
	if (app->puzzle == PUZZLE_3X3X3)
		app->puzzle = PUZZLE_2X2X2;
	else
		app->puzzle = PUZZLE_3X3X3;
	app->mode = MODE_MANUAL;
	app->auto_loop = false;
	app->was_solved = app_is_solved(app);
}

/// @brief Generates a fresh scramble, queues it for autoplay, records it
///        into app->solution_moves/solution_start_cube (Phase 7 §9.6),
///        and resets the practice session (undo/redo history and stats)
///        — a new scramble makes the previous cube state's history
///        meaningless.
static void	do_scramble(t_app *app)
{
	int	i;

	scramble_generate(app->solution_moves, SCRAMBLE_DEFAULT_LEN, &app->rng_seed);
	app->solution_start_cube = app->cube;
	app->solution_move_count = SCRAMBLE_DEFAULT_LEN;
	app->solution_scrubbable = true;
	i = 0;
	while (i < SCRAMBLE_DEFAULT_LEN)
	{
		anim_push(&app->anim, app->solution_moves[i]);
		i++;
	}
	app->solution_count = SCRAMBLE_DEFAULT_LEN;
	app->scrambling = true;
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
	if (!app_is_solved(app))
	{
		app->solution_count = queue_solution(app);
		return ;
	}
	app->auto_loop_wait_sec += dt;
	if (app->auto_loop_wait_sec >= AUTO_LOOP_WAIT_SEC)
	{
		app->auto_loop_wait_sec = 0.0f;
		do_scramble(app);
	}
}

/// @brief Shared bookkeeping for any manual move that enters the anim
///        pipeline, whichever door it came through (queued or a direct
///        drag hand-off, docs/en/11-drag-review.md §5.4): retires Phase
///        7 §9.6's scrub tracking (any manual move means the live cube
///        has diverged from whatever solution/scramble was tracked) and
///        starts the practice timer on the first manual move after a
///        scramble. `record` is true only for a genuinely new turn: an
///        undo/redo replay must NOT re-record itself, or it would defeat
///        its own redo/undo stack.
static void	record_manual_move(t_app *app, t_move move, bool record)
{
	app->solution_scrubbable = false;
	if (record)
		history_record(&app->history, move);
	if (!app->stats.timing)
	{
		app->stats.timing = true;
		app->stats.timer_start_sec = GetTime();
	}
	app->stats.move_count++;
}

/// @brief The queued path into the anim pipeline: keyboard turns and
///        undo/redo replays. Starts the move at angle 0, eased in-out —
///        anim_update()'s own dequeue does the rest, exactly as before.
static void	push_manual_move(t_app *app, t_move move, bool record)
{
	if (!anim_push(&app->anim, move))
		return ;
	record_manual_move(app, move, record);
}

/// @brief The mouse-drag counterpart of push_manual_move(): starts `move`
///        animating immediately via anim_begin_from(), continuing on
///        from the drag's own angle instead of rewinding to 0 first
///        (docs/en/11-drag-review.md §2.1/B5). Always records — a drag
///        release is always a new manual turn, never a replay.
static void	push_manual_move_from(t_app *app, t_move move, float from,
	float to)
{
	anim_begin_from(&app->anim, move, from, to);
	record_manual_move(app, move, true);
}

/// @brief Runs the solver on the CURRENT live cube (not a separately
///        tracked "what the user thinks the state is" copy) and queues
///        its output for autoplay. No-op if already solved.
static void	solve_for_me(t_app *app)
{
	int	count;

	if (app_is_solved(app))
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
///        app->solution_scrubbable holds (see record_manual_move()).
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

/// @brief One frame of Mode C's mouse half of the button split
///        (docs/en/11-drag-review.md §5.4): starts a drag on left-
///        mouse-down, updates it while held, and resolves it on
///        release — either into a real move (continuing to animate from
///        the drag's own angle, B5/B6) or, if it never left the dead
///        zone or was locked onto a middle slice, a settle back to 0.
///        Release is LEVEL-triggered (button no longer down, or the
///        window lost focus) rather than edge-triggered on
///        IsMouseButtonReleased(), so a drag can never get stuck
///        following the mouse with no button held (§2.4/S2) — pressing
///        `A` mid-drag additionally cancels it explicitly, see
///        render_run().
static void	tick_manual_mouse(t_app *app, float dt)
{
	t_move	move;
	float	from;
	float	to;

	if (!app->drag.active)
	{
		if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
			input_pick_start(&app->drag, app->camera,
				geometry_half_extent(app->puzzle));
		return ;
	}
	if (IsMouseButtonDown(MOUSE_BUTTON_LEFT) && IsWindowFocused())
	{
		input_pick_drag(&app->drag, GetMousePosition(), dt);
		return ;
	}
	move = input_pick_release(&app->drag, &from, &to);
	if (move != MOVE_COUNT)
		push_manual_move_from(app, move, from, to);
	else if (from != 0.0f)
		anim_begin_settle(&app->anim, app->drag.axis, app->drag.layer,
			from);
}

/// @brief One frame of Mode C (manual): reads at most one input source —
///        an in-progress mouse drag, a scramble/solve/undo/redo/scrub
///        key, or a keyboard face turn — and feeds any resulting move
///        into the SAME anim pipeline autoplay uses. Also tracks the
///        practice-session timer: started by the first manual move,
///        stopped the instant the cube reads solved.
static void	tick_manual(t_app *app, float dt)
{
	t_move	move;
	t_move	inverse;

	if (anim_is_idle(&app->anim))
	{
		if (app->drag.active || IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
			tick_manual_mouse(app, dt);
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
		if (app_is_solved(app))
			app->stats.timing = false;
	}
}

bool	render_run(const t_cube *start_cube, bool has_scramble,
	t_puzzle initial_puzzle)
{
	t_app			app;
	float			dt;
	t_active_turn	turn;
	bool			now_solved;

	SetConfigFlags(FLAG_VSYNC_HINT);
	InitWindow(WINDOW_WIDTH, WINDOW_HEIGHT, "rubik -- 3D bonus");
	SetTargetFPS(60);
	init_app(&app, start_cube, has_scramble, initial_puzzle);
	while (!WindowShouldClose())
	{
		dt = GetFrameTime();
		if (app.mode == MODE_MANUAL && !app.drag.active)
			orbit_camera_key_nudge(&app.orbit, dt);
		orbit_camera_update(&app.orbit, &app.camera, dt, !app.drag.active);
		if (!app.drag.active)
		{
			if (IsKeyPressed(KEY_FIVE))
				orbit_camera_preset(&app.orbit, 0);
			if (IsKeyPressed(KEY_SIX))
				orbit_camera_preset(&app.orbit, 1);
			if (IsKeyPressed(KEY_SEVEN))
				orbit_camera_preset(&app.orbit, 2);
			if (IsKeyPressed(KEY_EIGHT))
				orbit_camera_preset(&app.orbit, 3);
		}
		if (IsKeyPressed(KEY_P))
		{
			geometry_palette_cycle();
			geometry_sync(&app.scene, &app.cube);
		}
		if (IsKeyPressed(KEY_C))
			app.rounded_corners = !app.rounded_corners;
		if (IsKeyPressed(KEY_A))
		{
			input_pick_cancel(&app.drag);
			app.auto_loop = !app.auto_loop;
			if (!app.auto_loop)
			{
				anim_flush(&app.anim);
				app.mode = MODE_MANUAL;
			}
			else if (app.mode == MODE_MANUAL)
				do_scramble(&app);
		}
		if (IsKeyPressed(KEY_T))
		{
			if (app.algo == ALGO_KOCIEMBA)
				app.algo = ALGO_THISTLETHWAITE;
			else if (app.algo == ALGO_THISTLETHWAITE)
				app.algo = ALGO_LAYER;
			else
				app.algo = ALGO_KOCIEMBA;
		}
		if (IsKeyPressed(KEY_K) && !app.drag.active
			&& anim_is_idle(&app.anim))
			switch_puzzle(&app);
		if (app.mode == MODE_AUTOPLAY)
			tick_autoplay(&app, dt);
		else
			tick_manual(&app, dt);
		now_solved = app_is_solved(&app);
		if (now_solved && !app.was_solved)
			fx_spawn_celebration(&app.fx);
		app.was_solved = now_solved;
		turn = anim_get_active_turn(&app.anim);
		if (!turn.active)
			turn = input_drag_get_active_turn(&app.drag);
		draw_lighting_update_camera(&app.lighting, app.camera);
		BeginDrawing();
		ClearBackground(BLACK);
		DrawRectangleGradientV(0, 0, GetScreenWidth(), GetScreenHeight(),
			(Color){28, 30, 38, 255}, (Color){12, 12, 15, 255});
		BeginMode3D(app.camera);
		draw_scene(&app.scene, &turn, &app.lighting, app.rounded_corners,
			geometry_visible_count(app.puzzle),
			geometry_body_size(app.puzzle));
		fx_update_and_draw(&app.fx, dt);
		EndMode3D();
		hud_draw(&app.anim, app.mode, app.solution_count,
			app.stats.elapsed_sec, app.stats.move_count, app.auto_loop,
			app.scrambling, app.algo, app.puzzle);
		EndDrawing();
	}
	fx_unload(&app.fx);
	draw_lighting_unload(&app.lighting);
	CloseWindow();
	return (true);
}
