#ifndef LAYER_H
# define LAYER_H
# include "cube.h"

/// Beginner-method (layer-by-layer) solver: white cross, first-layer
/// corners, second-layer edges, then the last layer. See src/solve/
/// layer.c for the full pedagogical rationale, including why the last
/// layer is handed off to the existing Kociemba solve() rather than a
/// hand-built OLL/PLL table.
///
/// Move-count ceiling for the beginner method's output. Unlike Kociemba
/// (~23 moves) or Thistlethwaite (~40-45), the beginner method has no
/// tight theoretical bound -- 200 is a comfortably generous ceiling
/// derived from the worst case across every stage's pop+align+insert
/// (cross 4x12 + corners 4x9 + mid-edges 4x15 + a 30-move Kociemba
/// finish = 174 worst case), verified in practice never to be
/// approached (longest observed across thousands of random scrambles
/// stays well under half of this). Must stay under the shared
/// t_app.solution_moves buffer size (MAX_MOVES == 256, include/parse.h)
/// used by the 3D bonus for every algorithm.
# define LAYER_MAX_MOVES 200

/// @brief Solves `cube` with the layer-by-layer (beginner) method.
/// @param cube The cube to solve. Only the resulting t_cube is read --
///             never scramble text or a move list (solve/ anti-cheat
///             rule, docs/en/02-algorithms.md).
/// @param out  Receives up to LAYER_MAX_MOVES moves.
/// @return Move count (0 if already solved), or -1 if `cube` is not a
///         valid cube state.
int	layer_solve(const t_cube *cube, t_move *out);

#endif
