#ifndef SOLVE_H
# define SOLVE_H

# include <stdbool.h>
# include "cube.h"
# include "movetable.h"
# include "prune.h"
# include "ida.h"

/// The most moves solve() can return: the phase 1 cap plus the phase 2 cap
/// (12 + 18 = 30). Size of the buffer the caller gives it.
# define SOLVE_MAX_MOVES (IDA_PHASE1_CAP + IDA_PHASE2_CAP)

/// Everything the solver needs, built once at startup: the six move
/// tables, the four pruning tables, and the setup of both phases.
///
/// phase1 and phase2 point INTO this struct (at moves and prune), so build
/// it in place with solver_init() and never copy it.
typedef struct s_solver
{
	t_move_tables	moves;
	t_prune_tables	prune;
	t_ida_phase		phase1;
	t_ida_phase		phase2;
}	t_solver;

/// @brief Builds all the tables (about 0.4 s). Call once.
/// @return true on success. On false (malloc failed) nothing is allocated.
bool	solver_init(t_solver *solver);

/// @brief Frees the tables. Safe to call twice.
void	solver_free(t_solver *solver);

/// @brief Shortens a move list in place by merging moves that cancel or
///        combine, and returns the new length.
///
/// The two phases are searched separately, so the seam can hold waste: a
/// phase 1 solution ending in F followed by a phase 2 solution starting
/// with F2 is really one move (F'). Two moves on the same face merge into
/// one (R R = R2, R R2 = R', R R' = nothing). Moves on OPPOSITE faces
/// commute (U D = D U), so they merge across each other too (U D U' = D).
/// Repeats until nothing changes, so the result never shrinks further, and
/// it always has the same effect on a cube as the original list.
///
/// @param moves Modified in place.
/// @param count Number of moves in the list.
/// @return New number of moves, at most `count`.
int		simplify_moves(t_move *moves, int count);

/// @brief Solves a cube in two phases (Kociemba's method).
///
/// Takes ONLY the cube state, never the scramble text or moves: the
/// anti-cheat rule (docs/en/01-requirements.md), so the answer cannot be
/// "the scramble backwards". Phase 1 finds the shortest moves that put the
/// cube in the group where phase 2 can finish it, phase 2 finds the
/// shortest way from there to solved, and the two lists are joined and
/// simplified (simplify_moves()). Each phase is shortest, the total is a
/// good solution (about 23 moves), not always the shortest possible.
///
/// @param solver An initialised solver.
/// @param cube   The cube to solve. Must be a real cube (cube_is_valid()).
/// @param out    Buffer of SOLVE_MAX_MOVES moves; the solution is written
///               to out[0 .. return - 1], in order.
/// @return The number of moves (0 if already solved), or -1 if the cube is
///         not valid or no solution was found within the depth caps.
int		solve(const t_solver *solver, const t_cube *cube, t_move *out);

#endif
