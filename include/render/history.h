#ifndef RENDER_HISTORY_H
# define RENDER_HISTORY_H

# include <stddef.h>
# include <stdbool.h>
# include "parse.h"
# include "cube.h"

/// Undo/redo stacks for Mode C manual turns only (docs/en/03b-3d-
/// implementation-plan.md section 6.4). Autoplay and scramble moves are
/// never pushed here — undo is a manual-turning concept, so replaying an
/// autoplay move backwards would undo something the user never did.
///
/// Sized like anim.h's own queue: a practice session can never rack up
/// more manual turns than the longest input this program accepts
/// anywhere else (MAX_MOVES, parse.h).
# define HISTORY_CAP MAX_MOVES

typedef struct s_history
{
	t_move	undo_stack[HISTORY_CAP];
	size_t	undo_top;
	t_move	redo_stack[HISTORY_CAP];
	size_t	redo_top;
}	t_history;

/// @brief Empties both stacks. Call once before first use, and again
///        whenever a new practice session starts (e.g. a fresh scramble)
///        since old undo history refers to a cube state that no longer
///        exists.
void	history_init(t_history *history);

/// @brief Records one applied manual move: pushes it onto the undo
///        stack and clears the redo stack — a fresh move invalidates
///        whatever redo history existed. Silently drops the move once
///        the undo stack is full (HISTORY_CAP reached) rather than
///        overflowing.
void	history_record(t_history *history, t_move applied);

/// @brief Pops the most recent undo entry, pushes it (unchanged) onto
///        the redo stack, and writes its inverse to *out — the move to
///        feed into anim_push() to actually undo it on screen.
///
/// @return false if the undo stack is empty (*out untouched).
bool	history_undo(t_history *history, t_move *out);

/// @brief Pops the most recent redo entry, pushes it back onto the undo
///        stack, and writes the ORIGINAL move (not its inverse) to *out.
///
/// @return false if the redo stack is empty (*out untouched).
bool	history_redo(t_history *history, t_move *out);

/// @brief The exact inverse of one move — derived directly from t_move's
///        face * 3 + turn layout (turn 0 = cw, 1 = 180, 2 = ccw):
///        inverting flips cw <-> ccw and leaves a 180 unchanged.
t_move	move_inverse(t_move move);

#endif
