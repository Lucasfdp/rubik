#include "rubik.h"
#include "testlib.h"

/// Tests for the notation parser (parse/notation.c), the status messages
/// and validate_cube (parse/validate.c), plus parse -> apply -> validate.

# define LEN(a) (sizeof(a) / sizeof((a)[0]))

/// Parses input and returns the status. Fills moves/count.
static t_parse_status	parse(const char *input, t_move *moves, size_t *count)
{
	return (parse_notation(input, moves, count));
}

/// Shorthand: parse input, return only the status.
static t_parse_status	status_of(const char *input)
{
	t_move	moves[MAX_MOVES];
	size_t	count;

	return (parse(input, moves, &count));
}

static void	test_all_18_tokens(void)
{
	static const char	*tokens[MOVE_COUNT] = {"U", "U2", "U'", "R", "R2", "R'",
		"F", "F2", "F'", "D", "D2", "D'", "L", "L2", "L'", "B", "B2", "B'"};
	t_move				moves[MAX_MOVES];
	size_t				count;
	int					i;

	i = 0;
	while (i < MOVE_COUNT)
	{
		count = 99;
		CHECK_EQ(parse(tokens[i], moves, &count), PARSE_OK);
		CHECK_EQ(count, 1);
		CHECK_EQ(moves[0], i);
		i++;
	}
}

static void	test_sequence(void)
{
	static const t_move	want[] = {MOVE_R2, MOVE_D3, MOVE_B3};
	t_move				moves[MAX_MOVES];
	size_t				count;
	size_t				i;

	CHECK_EQ(parse("R2 D' B'", moves, &count), PARSE_OK);
	CHECK_EQ(count, LEN(want));
	i = 0;
	while (i < LEN(want))
	{
		CHECK_EQ(moves[i], want[i]);
		i++;
	}
	CHECK_EQ(parse("R2 D' B' D F2 R F2 R2 U L' F2 U' B' L2 R D B' R' B2 L2 "
			"F2 L2 R2 U2 D2", moves, &count), PARSE_OK);
	CHECK_EQ(count, 25);
}

/// Any run of whitespace between moves, and around the whole string.
static void	test_whitespace(void)
{
	static const char	*ok[] = {"R  U", "R   U", "R\tU", "\tR\tU\t", "  R U  ",
		"R U\n", "R U\r\n", "R\nU", "R\r\nU", "\nR U", " \r\n\t R \v U \f",
		"R U \n\n\n"};
	t_move				moves[MAX_MOVES];
	size_t				count;
	size_t				i;

	i = 0;
	while (i < LEN(ok))
	{
		count = 0;
		CHECK_EQ(parse(ok[i], moves, &count), PARSE_OK);
		CHECK_EQ(count, 2);
		i++;
	}
}

static void	test_empty_inputs(void)
{
	static const char	*empty[] = {"", " ", "   ", "\t", "\n", "\r\n", " \t\r\n\v\f"};
	t_move				moves[MAX_MOVES];
	size_t				count;
	size_t				i;

	count = 77;
	CHECK_EQ(parse(NULL, moves, &count), PARSE_EMPTY);
	CHECK_EQ(count, 0);
	i = 0;
	while (i < LEN(empty))
	{
		count = 77;
		CHECK_EQ(parse(empty[i], moves, &count), PARSE_EMPTY);
		CHECK_EQ(count, 0);
		i++;
	}
}

static void	test_bad_faces(void)
{
	static const char	*bad[] = {"X", "r", "u", "M", "E", "S", "x", "y", "z",
		"W", "1", "0", "'", "2", "-", "R X", "R2 X", "U R f", "R w", "*",
		"R U m", "@", "\"", "\x01"};
	size_t				i;

	i = 0;
	while (i < LEN(bad))
	{
		CHECK_EQ(status_of(bad[i]), PARSE_UNKNOWN_FACE);
		i++;
	}
}

static void	test_bad_modifiers(void)
{
	static const char	*bad[] = {"R3", "R1", "R0", "RR", "RU", "R\"", "R`",
		"R-", "R+", "Rw", "U R3", "U R2 D9", "R,", "R ' U", "U\x01"};
	size_t				i;

	i = 0;
	while (i < LEN(bad))
	{
		CHECK_EQ(status_of(bad[i]), i == 13 ? PARSE_UNKNOWN_FACE
			: PARSE_BAD_MODIFIER);
		i++;
	}
}

static void	test_tokens_too_long(void)
{
	static const char	*bad[] = {"R''", "R2'", "R'2", "R22", "RRR", "R2X",
		"R U2X", "R\xE2\x80\x99", "UUUUUUUUUU", "R2 D'' B"};
	size_t				i;

	i = 0;
	while (i < LEN(bad))
	{
		CHECK_EQ(status_of(bad[i]), PARSE_TOKEN_TOO_LONG);
		i++;
	}
}

/// Exactly MAX_MOVES moves is fine, one more is not, and the raw string
/// length guard (MAX_MOVES * 3 bytes) cannot be bypassed with padding.
static void	test_limits(void)
{
	static char	text[MAX_MOVES * 4 + 64];
	static char	huge[5001];
	t_move		moves[MAX_MOVES];
	size_t		count;
	size_t		i;

	text[0] = '\0';
	i = 0;
	while (i < MAX_MOVES)
	{
		strcat(text, i ? " R2" : "R2");
		i++;
	}
	CHECK_EQ(strlen(text), MAX_MOVES * 3 - 1);
	CHECK_EQ(parse(text, moves, &count), PARSE_OK);
	CHECK_EQ(count, MAX_MOVES);
	strcat(text, " R2");
	CHECK_EQ(parse(text, moves, &count), PARSE_TOO_MANY_MOVES);
	text[0] = '\0';
	i = 0;
	while (i < MAX_MOVES + 1)
	{
		strcat(text, i ? " R" : "R");
		i++;
	}
	CHECK(strlen(text) < MAX_MOVES * 3);
	CHECK_EQ(parse(text, moves, &count), PARSE_TOO_MANY_MOVES);
	memset(text, ' ', MAX_MOVES * 3);
	text[MAX_MOVES * 3] = 'R';
	text[MAX_MOVES * 3 + 1] = '\0';
	CHECK_EQ(parse(text, moves, &count), PARSE_TOO_MANY_MOVES);
	memset(huge, 'R', sizeof(huge) - 1);
	CHECK_EQ(parse(huge, moves, &count), PARSE_TOO_MANY_MOVES);
}

/// The caller's string must come back byte for byte identical.
static void	test_input_not_modified(void)
{
	char	input[] = "R2 D'\tB'\nU X";
	char	copy[sizeof(input)];
	t_move	moves[MAX_MOVES];
	size_t	count;

	memcpy(copy, input, sizeof(input));
	parse(input, moves, &count);
	CHECK(memcmp(input, copy, sizeof(input)) == 0);
	memcpy(input, "R U R'", 7);
	memcpy(copy, input, 7);
	parse(input, moves, &count);
	CHECK(memcmp(input, copy, 7) == 0);
}

/// Every status has its own non-empty message (a missing entry in the
/// designated-initializer table would be a NULL pointer here).
static void	test_status_messages(void)
{
	int	a;
	int	b;

	a = 0;
	while (a <= PARSE_INVALID_CUBE)
	{
		CHECK(parse_status_message((t_parse_status)a) != NULL);
		if (parse_status_message((t_parse_status)a))
		{
			CHECK(parse_status_message((t_parse_status)a)[0] != '\0');
			b = a + 1;
			while (b <= PARSE_INVALID_CUBE)
			{
				CHECK(parse_status_message((t_parse_status)b) != NULL
					&& strcmp(parse_status_message((t_parse_status)a),
						parse_status_message((t_parse_status)b)) != 0);
				b++;
			}
		}
		a++;
	}
}

static void	test_validate_cube(void)
{
	t_cube	cube;

	CHECK_EQ(validate_cube(&SOLVED_CUBE), PARSE_OK);
	cube = SOLVED_CUBE;
	cube.corner_orient[3] = 1;
	CHECK_EQ(validate_cube(&cube), PARSE_INVALID_CUBE);
	cube = SOLVED_CUBE;
	cube.edge_orient[3] = 1;
	CHECK_EQ(validate_cube(&cube), PARSE_INVALID_CUBE);
	cube = SOLVED_CUBE;
	cube.edge_perm[0] = EDGE_UF;
	cube.edge_perm[1] = EDGE_UR;
	CHECK_EQ(validate_cube(&cube), PARSE_INVALID_CUBE);
}

/// The full main() pipeline minus the solver: parse a scramble, apply it
/// to SOLVED_CUBE, validate. Any parsed scramble must give a valid cube.
static void	test_parse_apply_validate(void)
{
	static const char	*faces = "URFDLB";
	static const char	*mods[] = {"", "2", "'"};
	char				text[MAX_MOVES * 3 + 1];
	t_move				moves[MAX_MOVES];
	size_t				count;
	t_cube				cube;
	uint32_t			rng;
	size_t				pos;
	int					round;
	int					len;
	int					i;

	CHECK_EQ(parse("F R U2 B' L' D'", moves, &count), PARSE_OK);
	cube = SOLVED_CUBE;
	cube_apply_moves(&cube, moves, count);
	CHECK(!cube_is_solved(&cube));
	CHECK_EQ(validate_cube(&cube), PARSE_OK);
	CHECK_EQ(parse("R R'", moves, &count), PARSE_OK);
	cube = SOLVED_CUBE;
	cube_apply_moves(&cube, moves, count);
	CHECK(cube_is_solved(&cube));
	rng = 987654321;
	round = 0;
	while (round < 300)
	{
		len = (int)(test_rand(&rng) % 100) + 1;
		pos = 0;
		i = 0;
		while (i < len)
		{
			if (i)
				text[pos++] = ' ';
			text[pos++] = faces[test_rand(&rng) % 6];
			pos += (size_t)snprintf(text + pos, 3, "%s",
					mods[test_rand(&rng) % 3]);
			i++;
		}
		text[pos] = '\0';
		CHECK_EQ(parse(text, moves, &count), PARSE_OK);
		CHECK_EQ(count, len);
		cube = SOLVED_CUBE;
		cube_apply_moves(&cube, moves, count);
		CHECK_EQ(validate_cube(&cube), PARSE_OK);
		round++;
	}
}

int	main(void)
{
	test_all_18_tokens();
	test_sequence();
	test_whitespace();
	test_empty_inputs();
	test_bad_faces();
	test_bad_modifiers();
	test_tokens_too_long();
	test_limits();
	test_input_not_modified();
	test_status_messages();
	test_validate_cube();
	test_parse_apply_validate();
	return (test_report("parse"));
}
