/* ========================================================================
 * parse.h — argv string -> t_move[]. See src/parse/notation.c.
 * ======================================================================== */

#ifndef PARSE_H
# define PARSE_H

# include <stddef.h>
# include "cube.h"

/* Generous fixed cap on how many moves one scramble can hold. Picked so
 * notation.c can tokenize into a stack buffer with no malloc/free to
 * track — see that file for how it doubles as an input-length guard. */
# define MAX_MOVES 256

typedef enum e_parse_status
{
	PARSE_OK,
	PARSE_EMPTY,
	PARSE_UNKNOWN_FACE,
	PARSE_BAD_MODIFIER,
	PARSE_TOKEN_TOO_LONG,
	PARSE_TOO_MANY_MOVES
}	t_parse_status;

t_parse_status	parse_notation(const char *input, t_move *moves, size_t *count);
const char		*parse_status_message(t_parse_status status);

#endif
