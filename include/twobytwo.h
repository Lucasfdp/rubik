#ifndef TWOBYTWO_H
# define TWOBYTWO_H

# include <stdbool.h>
# include "cube.h"
# include "coord.h"
# include "movetable.h"
# include "prune.h"
# include "ida.h"

/// 2x2x2 support (the subject's "ways to work with other puzzles" bonus
/// item; see docs/en/14-other-puzzles.md for why 4x4x4/Megaminx/Square-1
/// do NOT generalise the same way).
///
/// A 2x2x2 has corners and nothing else -- no edges, no centres. Solving
/// one from a t_cube is therefore corner-only Kociemba: a SINGLE search
/// (there is only one goal, so there is no phase 1 / phase 2 split) over
/// twist and cperm, with the full 18-move set, never reading edge_perm or
/// edge_orient. Every coordinate encoder, move table, prune builder and
/// the IDA* driver itself are the exact same code the 3x3x3 solver uses
/// (coord.h, movetable.h, prune.h, ida.h) -- nothing new was written for
/// those layers, only this module's setup of them.
///
/// A proven result (docs/en/14-other-puzzles.md) bounds any 2x2x2 at 11
/// moves in this metric, so TWOBYTWO_MAX_MOVES is a real cap, not a guess:
/// ida_search() can never need more.
# define TWOBYTWO_MAX_MOVES 11

/// Everything the 2x2x2 solver needs, built once at startup: the SAME
/// six-table t_move_tables the 3x3x3 solver builds (only ->twist and
/// ->cperm are ever read here) plus two small pruning tables of its own.
///
/// phase points INTO this struct (at moves.twist, moves.cperm and the two
/// prune tables below), so build it in place with two_solver_init() and
/// never copy it.
typedef struct s_two_solver
{
	t_move_tables	moves;
	t_prune_table	twist_prune;
	t_prune_table	cperm_prune;
	t_ida_phase		phase;
}	t_two_solver;

/// @brief Builds the tables (shares the six-table build with the 3x3x3
///        solver, plus two cheap 1-D prune tables on top). Call once.
/// @return true on success. On false (malloc failed) nothing is allocated.
bool	two_solver_init(t_two_solver *solver);

/// @brief Frees the tables. Safe to call twice.
void	two_solver_free(t_two_solver *solver);

/// @brief True if the cube's corners alone are solved. Edges are never
///        looked at: this, not cube_is_solved(), is "solved" for a
///        2x2x2, since it has no edges to be out of place.
bool	two_cube_is_solved(const t_cube *cube);

/// @brief Solves the CORNERS of a cube (a 2x2x2 has nothing else), taking
///        only the cube, never the scramble -- the same anti-cheat rule
///        as solve() (docs/en/01-requirements.md). edge_perm and
///        edge_orient are read by nothing in this call.
///
/// @param solver An initialised solver.
/// @param cube   The cube to solve. Only the corner-twist invariant is
///               checked (cube_corner_twist_sum(cube) == 0): a 2x2x2 has
///               no edges, so unlike solve() there is no permutation-
///               parity invariant linking corners to anything else.
/// @param out    Buffer of TWOBYTWO_MAX_MOVES moves; the solution is
///               written to out[0 .. return - 1], in order.
/// @return The number of moves (0 if the corners are already solved), or
///         -1 if the corner invariant fails or no solution was found.
int		two_solve(const t_two_solver *solver, const t_cube *cube,
			t_move *out);

#endif
