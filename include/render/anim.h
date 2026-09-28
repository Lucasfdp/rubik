#ifndef RENDER_ANIM_H
# define RENDER_ANIM_H

# include <stddef.h>
# include <stdbool.h>
# include "cube.h"
# include "parse.h"
# include "render/geometry.h"
# include "render/fx.h"

/// A queued solution can never be longer than the longest input this
/// program accepts anywhere else (MAX_MOVES, parse.h) — reuse that bound
/// instead of inventing a second one.
# define ANIM_QUEUE_CAP MAX_MOVES

/// Playback state for both autoplay (Mode A, the solver's move list) and
/// manual turning (Mode C, one keyboard or mouse-drag turn at a time):
/// both feed the exact same queue/update pipeline, so a manual scramble
/// is exactly as trustworthy as a solver-produced one.
///
/// queue/head/tail: a fixed-capacity circular buffer, empty when
/// head == tail.
typedef struct s_anim_state
{
	t_move	queue[ANIM_QUEUE_CAP];
	size_t	head;
	size_t	tail;
	bool	active;
	t_move	current;
	float	elapsed_sec;
	float	duration_sec;
	float	angle_deg;
	float	target_deg;
	float	speed_deg_per_sec;
	bool	paused;
}	t_anim_state;

/// @brief Empties the queue and resets to a sane default speed. Call once
///        before the first anim_push()/anim_update().
void	anim_init(t_anim_state *state);

/// @brief Queues one more move to play. false if the queue is full (never
///        happens for a solver solution or single manual turns — the
///        queue is sized to MAX_MOVES).
bool	anim_push(t_anim_state *state, t_move move);

/// @brief Advances the in-progress move (if any) by dt seconds of eased
///        rotation; on completion, commits it with apply_move() and a
///        full geometry_sync(), plays the turn sound via `fx` (Phase 7
///        §9.3 — pass NULL to skip), then starts the next queued move,
///        if any. No-op if idle and the queue is empty.
void	anim_update(t_anim_state *state, t_render_scene *scene,
			t_cube *cube, t_fx_state *fx, float dt);

/// @brief True when nothing is animating and the queue is empty.
bool	anim_is_idle(const t_anim_state *state);

/// @brief Pause/resume playback. Has no effect on which moves are queued.
void	anim_toggle_pause(t_anim_state *state);

/// @brief Advances exactly one move to completion, then re-pauses.
///        Reuses anim_update()'s own commit path (a single large dt)
///        instead of a second "apply instantly" code path. No-op if
///        already idle.
void	anim_step_one(t_anim_state *state, t_render_scene *scene,
			t_cube *cube, t_fx_state *fx);

/// @brief Sets the playback speed (degrees of rotation per second),
///        clamped to a sane range.
void	anim_set_speed(t_anim_state *state, float deg_per_sec);

/// @brief Drops every move still waiting in the queue. Any move already
///        in progress is left to finish committing on its own.
void	anim_flush(t_anim_state *state);

/// @brief Describes the layer currently mid-turn, for draw_scene()'s
///        transient rotation. ->active is false when nothing is
///        animating right now.
t_active_turn	anim_get_active_turn(const t_anim_state *state);

/// @brief How many moves are still queued or mid-flight (0 when idle).
///        Used by the HUD to show "N moves remaining of TOTAL", and by
///        Phase 7 §9.6's scrub commands to work out how far into a
///        tracked solution/scramble playback currently is.
size_t	anim_pending_count(const t_anim_state *state);

/// @brief The move whose single application equals rotating `axis` by
///        exactly `quarter_deg` around `layer` — the exact inverse of
///        MOVE_AXIS's own table (anim.c), exposed so input.c's mouse
///        drag (Phase 6 §8.5) can resolve a snapped drag into a real
///        move without duplicating or exporting that table.
///
/// @return MOVE_COUNT if no move matches.
t_move	anim_move_for_turn(t_axis axis, int8_t layer, float quarter_deg);

#endif
