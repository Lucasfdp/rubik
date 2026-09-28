#ifndef RENDER_APP_H
# define RENDER_APP_H

# include "raylib.h"
# include "cube.h"
# include "render/geometry.h"
# include "render/anim.h"
# include "render/input.h"
# include "render/history.h"
# include "render/draw.h"

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
}	t_app;

#endif
