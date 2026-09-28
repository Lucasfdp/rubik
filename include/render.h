#ifndef RENDER_H
# define RENDER_H

# include <stdbool.h>
# include "cube.h"

/// @brief Opens the 3D window and runs the render loop until the user
///        closes it. Owns everything under src/render/ internally; this
///        is the entire public surface main.c ever touches. Deliberately
///        raylib-agnostic: main.c's BONUS_ALGO translation unit is built
///        without raylib's include path (see the Makefile's pattern-
///        specific CPPFLAGS rule for src/render/*.o), so nothing pulled
///        in here may mention a raylib type.
///
/// @param start_cube   The cube to open on. When has_scramble is false,
///                     this is SOLVED_CUBE and the window opens straight
///                     into manual/practice turning.
/// @param has_scramble True to auto-solve start_cube and play the
///                     solution back on open; false to open ready for
///                     manual turning.
/// @return true on a clean exit, false on an internal render error.
bool	render_run(const t_cube *start_cube, bool has_scramble);

#endif
