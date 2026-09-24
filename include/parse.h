#ifndef PARSE_H
# define PARSE_H

# include <stddef.h>
# include "cube.h"

/// Maximum number of moves one scramble can contain.
///
/// Sized so parse_notation() can tokenize into a fixed stack buffer of
/// MAX_MOVES * 3 chars (up to 2 chars per move + 1 separator) with no
/// malloc. That same buffer size doubles as an input-length guard: an
/// oversized string is rejected before any parsing happens.
# define MAX_MOVES 256

/// Result code shared by parse_notation() and validate_cube().
///
/// One vocabulary for both "bad notation" and "impossible cube", so main.c
/// only needs a single error path. Turn a code into text with
/// parse_status_message().
typedef enum e_parse_status
{
	/// Success.
	PARSE_OK,
	/// Input was NULL, empty, or contained no moves (only spaces/tabs).
	PARSE_EMPTY,
	/// A token starts with something other than U R F D L B (lowercase,
	/// M, E, S, x, y, z are all rejected).
	PARSE_UNKNOWN_FACE,
	/// A token's second character is neither ' nor 2.
	PARSE_BAD_MODIFIER,
	/// A token is longer than 2 characters.
	PARSE_TOKEN_TOO_LONG,
	/// More than MAX_MOVES moves, or the input is too long for the buffer.
	PARSE_TOO_MANY_MOVES,
	/// validate_cube() rejected the cube: no real cube can look like this.
	PARSE_INVALID_CUBE
}	t_parse_status;

/// @brief Parses a scramble string like "R2 D' B'" into a move list.
t_parse_status	parse_notation(const char *input, t_move *moves, size_t *count);

/// @brief Returns a human-readable message for a status code.
const char		*parse_status_message(t_parse_status status);

/// @brief Checks that a cube is physically reachable.
t_parse_status	validate_cube(const t_cube *cube);

#endif
