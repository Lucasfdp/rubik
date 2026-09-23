/* ========================================================================
 * src/parse/notation.c — argv string -> t_move[]
 * ======================================================================== */

#include "rubik.h"

/* Face letters in the exact order t_move's enum uses them: U R F D L B.
 * That means (strchr(FACES, token[0]) - FACES) gives the face's index
 * 0-5 directly, and index * 3 lands on that face's first move (the CW
 * one, since each face's three moves are laid out CW/180/CCW in a row). */
static const char	FACES[] = "URFDLB";

/* Turns one already-length-checked token ("R", "R2", "R'") into a t_move.
 * Only the modifier is decided here -- the face letter is resolved via
 * FACES above. Anything other than no-modifier / "2" / "'" is rejected. */
static t_parse_status	token_to_move(const char *token, t_move *out)
{
	const char	*face_ptr;
	int			face_index;
	int			offset;

	face_ptr = strchr(FACES, token[0]);
	if (!face_ptr)
		return (PARSE_UNKNOWN_FACE);
	face_index = (int)(face_ptr - FACES);
	if (token[1] == '\0')
		offset = 0;
	else if (token[1] == '2')
		offset = 1;
	else if (token[1] == '\'')
		offset = 2;
	else
		return (PARSE_BAD_MODIFIER);
	*out = (t_move)(face_index * 3 + offset);
	return (PARSE_OK);
}

/* input: the raw argv string, e.g. "R2 D' B'" -- never modified.
 * moves: caller-owned array, at least MAX_MOVES t_move slots.
 * count: set to how many moves were parsed, only when PARSE_OK is returned.
 *
 * input is copied into a local stack buffer before tokenizing, because
 * strtok_r writes '\0' into the string it walks. Leaving input untouched
 * matters: main.c will want to echo the original scramble in an error
 * message, and mutating argv out from under the caller is the kind of
 * thing that comes back to bite you later in the project. */
t_parse_status	parse_notation(const char *input, t_move *moves, size_t *count)
{
	char			buf[MAX_MOVES * 3];
	char			*token;
	char			*saveptr;
	t_parse_status	status;

	if (!input || !input[0])
		return (PARSE_EMPTY);
	if (strlen(input) >= sizeof(buf))
		return (PARSE_TOO_MANY_MOVES);
	snprintf(buf, sizeof(buf), "%s", input);
	*count = 0;
	token = strtok_r(buf, " \t", &saveptr);
	while (token)
	{
		if (*count >= MAX_MOVES)
			return (PARSE_TOO_MANY_MOVES);
		if (strlen(token) > 2)
			return (PARSE_TOKEN_TOO_LONG);
		status = token_to_move(token, &moves[*count]);
		if (status != PARSE_OK)
			return (status);
		(*count)++;
		token = strtok_r(NULL, " \t", &saveptr);
	}
	if (*count == 0)
		return (PARSE_EMPTY);
	return (PARSE_OK);
}

/* One message per t_parse_status, indexed by the enum value itself --
 * see the explanation of why this is safe and stays in sync by
 * construction, alongside the rest of this function's write-up. */
const char	*parse_status_message(t_parse_status status)
{
	static const char	*const messages[] = {
		[PARSE_OK] = "ok",
		[PARSE_EMPTY] = "empty scramble",
		[PARSE_UNKNOWN_FACE] = "unknown face letter (expected one of U R F D L B)",
		[PARSE_BAD_MODIFIER] = "bad move modifier (expected ' or 2)",
		[PARSE_TOKEN_TOO_LONG] = "move token too long",
		[PARSE_TOO_MANY_MOVES] = "too many moves"
	};
	return (messages[status]);
}
