#ifndef IDA_H
# define IDA_H

# include <stdbool.h>
# include <stdint.h>
# include "cube.h"
# include "coord.h"
# include "prune.h"

/// IDA* ("iterative deepening A*") finds the SHORTEST list of moves that
/// takes a state to the goal, without remembering the states it has seen
/// (docs/en/02-algorithms.md, item 7).
///
/// Set a budget of B moves, starting from the pruning-table estimate for
/// the start state. Try every move sequence depth-first. Abandon a branch
/// the moment "moves used so far + estimate of moves still needed" is
/// bigger than B: the estimate is a lower bound, so that branch cannot
/// finish within B. If no sequence reaches the goal, raise B by 1 and start
/// again. The first goal found is a shortest solution, because every
/// shorter budget was already tried and failed.
///
/// The search never touches a real cube. A state is three coordinates, a
/// move looks up the next value of each in its move table, and the
/// estimate comes from two pruning tables (movetable.h, prune.h). The same
/// driver runs both phases: only the tables, the allowed moves and the
/// depth cap change (t_ida_phase).

/// Size of the path buffer the caller must give ida_search(): the most
/// moves any search may return.
# define IDA_MAX_DEPTH 20

/// Known worst cases of the two-phase method: phase 1 never needs more
/// than 12 moves, phase 2 never more than 18.
# define IDA_PHASE1_CAP 12
# define IDA_PHASE2_CAP 18

/// Everything that makes phase 1 different from phase 2.
///
/// A state is three coordinates x, y, z (in that order in every array).
/// The two pruning tables are always the pairs (x, z) and (y, z): phase 1
/// is (twist, flip, slice) with twist-slice and flip-slice, phase 2 is
/// (cperm, eperm, sperm) with cperm-sperm and eperm-sperm.
///
/// - move_table[i]: move table of coordinate i (movetable.h).
/// - prune_xz, prune_yz: the two pruning tables.
/// - moves: moves the search may use.
/// - max_depth: gives up (returns -1) if no solution within this many
///   moves. Values above IDA_MAX_DEPTH count as IDA_MAX_DEPTH.
typedef struct s_ida_phase
{
	const uint16_t		*move_table[3];
	const t_prune_table	*prune_xz;
	const t_prune_table	*prune_yz;
	t_move_mask			moves;
	int					max_depth;
}	t_ida_phase;

/// Internal to ida.c: what one search remembers while it recurses.
///
/// - phase: the phase being searched.
/// - path: where the moves of the current branch are written.
/// - bound: the current budget B.
/// - length: set to the solution length when a solution is found.
typedef struct s_ida_ctx
{
	const t_ida_phase	*phase;
	t_move				*path;
	int					bound;
	int					length;
}	t_ida_ctx;

/// @brief Sets up phase 1: twist, flip, slice -> 0, 0, 0, all 18 moves.
void	ida_phase1_setup(t_ida_phase *phase, const t_move_tables *moves,
			const t_prune_tables *prune);

/// @brief Sets up phase 2: cperm, eperm, sperm -> 0, 0, 0, the 10 moves
///        of MOVES_PHASE2.
void	ida_phase2_setup(t_ida_phase *phase, const t_move_tables *moves,
			const t_prune_tables *prune);

/// @brief The phase 1 start state of a cube: twist, flip, slice.
void	ida_phase1_start(const t_cube *cube, uint16_t start[3]);

/// @brief The phase 2 start state of a cube: cperm, eperm, sperm.
///
/// Only meaningful once phase 1 is done (slice is 0), see coord.h.
void	ida_phase2_start(const t_cube *cube, uint16_t start[3]);

/// @brief Lower bound on the moves still needed from a state: the bigger of
///        the two pruning values. 0 exactly when the state IS the goal
///        (the goal is (0, 0, 0) and a pair reads 0 only at (0, 0)).
int		ida_estimate(const t_ida_phase *phase, const uint16_t coord[3]);

/// @brief True if `move` is a waste right after `last`, so the search
///        skips it.
///
/// Turning the same face twice in a row is one move written badly (R R is
/// R2, R R' is nothing). Opposite faces (U/D, R/L, F/B) do not affect each
/// other, so "U D" and "D U" reach the same state; only the order with the
/// smaller face number first (faces are U R F D L B) is allowed. Removes
/// most of the duplicate branches for free.
///
/// @param last The previous move, or -1 for the first move.
bool	ida_move_is_redundant(int last, t_move move);

/// @brief Shortest move list from `start` to the goal of this phase.
///
/// @param phase Tables, moves and depth cap of the phase.
/// @param start Start state, three coordinates.
/// @param path  Buffer of IDA_MAX_DEPTH moves; the solution is written to
///              path[0 .. return - 1], in order.
/// @return The number of moves (0 if `start` is already the goal), or -1
///         if there is no solution within phase->max_depth.
int		ida_search(const t_ida_phase *phase, const uint16_t start[3],
			t_move *path);

#endif
