#ifndef ALGO_H
# define ALGO_H

/// Which solver "-a" (main.c) or T (the 3D bonus, render/app.c) picked.
/// Kociemba is the default everywhere -- no flag, no keypress needed.
///
/// Shared between main.c and render/app.c (hence its own header, pulled
/// in unconditionally by rubik.h just like thistlethwaite.h already is):
/// the bonus 3D view needs the same three-way choice main.c's "-a" flag
/// offers, so main.c's t_algo enum moved here rather than being
/// duplicated.
typedef enum e_algo
{
	ALGO_KOCIEMBA,
	ALGO_THISTLETHWAITE,
	ALGO_LAYER,
}	t_algo;

/// Which puzzle the 3D bonus renders and solves -- K (render/app.c)
/// toggles it live, "-p 2x2x2" (main.c) picks the one the window opens
/// on. Both share the exact same t_cube: a 2x2x2 is just "only look at
/// and solve the 8 corners of the same full cube", never a different
/// struct or a different scramble/render pipeline (see docs/en/14-
/// other-puzzles.md for why that trick stops working past 2x2x2).
typedef enum e_puzzle
{
	PUZZLE_3X3X3,
	PUZZLE_2X2X2,
}	t_puzzle;

#endif
