#ifndef RENDER_APP_H
# define RENDER_APP_H

# include "raylib.h"
# include "cube.h"
# include "algo.h"
# include "parse.h"
# include "render/geometry.h"
# include "render/anim.h"
# include "render/input.h"
# include "render/history.h"
# include "render/draw.h"
# include "render/fx.h"

/// Practice-mode stats (docs/en/03b-3d-implementation-plan.md section
/// 6.3): session bookkeeping, not animation state, so it lives here
/// rather than in t_anim_state. Timing starts on the first manual
/// (keyboard-driven) move after a scramble and stops the instant the
/// cube reads solved.
typedef struct s_session_stats
{
	double	timer_start_sec;
	double	elapsed_sec;
	int		move_count;
	bool	timing;
}	t_session_stats;

/// Everything one frame of render_run()'s loop needs, gathered here so
/// growing the feature later never means growing a function's argument
/// list — every render/*.c module that needs state gets one pointer.
///
/// Phase 6/7 additions: drag (mouse click-and-drag turning) and fx (turn
/// sound + solve celebration) follow the same "thread it through
/// explicitly" rule as lighting/orbit/anim already did. was_solved lets
/// render_run() edge-detect "just became solved" once, for the
/// celebration, regardless of which path (manual, autoplay, solve-for-
/// me, undo/redo) got it there. solution_moves holds whatever the
/// solver (or the scrambler) last produced, purely so queue_solution()/
/// do_scramble() can feed it into anim_push() one move at a time. algo
/// is the keyboard algorithm switch (see render/app.c's queue_solution()
/// and the T handler in render_run()): which solver -- of the same three
/// choices "-a" offers on the command line -- last got picked, either by
/// T or by whatever main.c started the window with; defaults to
/// ALGO_KOCIEMBA. puzzle is K's keyboard puzzle switch (render/app.c's
/// switch_puzzle() and the K handler in render_run()): which of the two
/// t_puzzle views (3x3x3 or 2x2x2) is currently drawn/solved, either
/// toggled live by K or picked by whatever main.c's "-p 2x2x2" started
/// the window with; defaults to PUZZLE_3X3X3. Switching never touches
/// app->cube itself -- both views read the exact same t_cube, a 2x2x2
/// just never looks at or draws its edge/centre pieces (see algo.h's
/// t_puzzle comment).
typedef struct s_app
{
	t_cube				cube;
	t_render_scene		scene;
	t_anim_state		anim;
	t_orbit_camera		orbit;
	Camera3D			camera;
	t_render_mode		mode;
	int					solution_count;
	t_history			history;
	t_session_stats		stats;
	unsigned int		rng_seed;
	t_render_lighting	lighting;
	t_drag_state		drag;
	t_fx_state			fx;
	bool				was_solved;
	bool				rounded_corners;
	bool				auto_loop;
	float				auto_loop_wait_sec;
	bool				scrambling;
	t_move				solution_moves[MAX_MOVES];
	t_algo				algo;
	t_puzzle			puzzle;
}	t_app;

#endif
