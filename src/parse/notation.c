#include "rubik.h"

/// Face letters in the same order as t_move: U R F D L B.
///
/// strchr(FACES, letter) - FACES gives the face index 0-5, and
/// index * 3 is that face's first move (the clockwise one), because each
/// face's three moves sit together as clockwise / 180 / counter-clockwise.
static const char	FACES[] = "URFDLB";

/// @brief Converts one token ("R", "R2" or "R'") into a t_move.
///
/// The face letter is looked up in FACES. The modifier then picks the
/// offset inside that face's group of three: none = 0 (clockwise),
/// "2" = 1 (180), "'" = 2 (counter-clockwise).
///
/// Precondition: token is non-empty and at most 2 chars long. The caller
/// (parse_notation) checks the length before calling.
///
/// @param token NUL-terminated token.
/// @param out   Written only when PARSE_OK is returned.
/// @return PARSE_OK, PARSE_UNKNOWN_FACE or PARSE_BAD_MODIFIER.
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

/// @brief Parses a scramble string like "R2 D' B'" into a move list.
///
/// Tokens are separated by spaces or tabs. The input is copied into a
/// local buffer first, because strtok_r writes '\0' into the string it
/// walks; this way the caller's string (usually argv) is never modified.
/// The buffer size also acts as an input-length guard (see MAX_MOVES).
///
/// Checks, in order: NULL/empty input, input too long, then for each
/// token: too many moves, token longer than 2 chars, valid face and
/// modifier. Input with only whitespace gives PARSE_EMPTY.
///
/// @param input Raw scramble string. Never modified.
/// @param moves Caller-owned array with room for MAX_MOVES moves. May be
///              partly filled when an error is returned.
/// @param count Number of moves parsed. Only meaningful on PARSE_OK.
/// @return PARSE_OK, or the first error found.
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

/// @brief Returns a human-readable message for a status code.
///
/// The messages[] table uses designated initializers ([PARSE_OK] = "ok"),
/// so each string lands at the index of its enum value no matter what
/// order it is written in.
///
/// Note: when you add a value to t_parse_status you must add its message
/// here too. A missing entry is NOT caught by the compiler, and looking
/// it up would read past the end of the table.
///
/// @param status A valid t_parse_status value.
/// @return Static string, never NULL, never to be freed.
const char	*parse_status_message(t_parse_status status)
{
	/// Indexed by t_parse_status value.
	static const char	*const messages[] = {
		[PARSE_OK] = "ok",
		[PARSE_EMPTY] = "empty scramble",
		[PARSE_UNKNOWN_FACE] = "unknown face letter (expected one of U R F D L B)",
		[PARSE_BAD_MODIFIER] = "bad move modifier (expected ' or 2)",
		[PARSE_TOKEN_TOO_LONG] = "move token too long",
		[PARSE_TOO_MANY_MOVES] = "too many moves",
		[PARSE_INVALID_CUBE] = "physically impossible cube (bad parity/orientation)"
	};
	return (messages[status]);
}
