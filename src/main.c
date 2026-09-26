#include "rubik.h"

/// @brief Entry point: reads a scramble from argv[1] and builds its cube.
///
/// Usage: rubik "<scramble>", for example: rubik "R2 D' B'"
///
/// Pipeline: parse the string into moves, apply them to a copy of
/// SOLVED_CUBE, validate the resulting state, then (soon) hand ONLY that
/// state to the solver. The move list and the string never reach the
/// solver: that is the anti-cheat rule (docs/en/01-requirements.md), so the
/// solver physically cannot return the inverse of the scramble.
///
/// The solver does not exist yet. Until it does, an already-solved cube
/// prints an empty line (the correct answer: zero moves) and anything else
/// reports "solver not implemented yet" on stderr and exits with 2.
///
/// @return 0 on success, 1 on wrong argument count or a parse/validation
///         error, 2 when a solution is needed but the solver is missing.
int	main(int ac, char const *av[])
{
	t_move			moves[MAX_MOVES];
	size_t			count;
	t_cube			cube;
	t_parse_status	status;

	if (ac != 2)
	{
		fprintf(stderr, "usage: %s \"<scramble>\"\n", av[0]);
		return (1);
	}
	status = parse_notation(av[1], moves, &count);
	if (status != PARSE_OK)
	{
		fprintf(stderr, "rubik: %s\n", parse_status_message(status));
		return (1);
	}
	cube = SOLVED_CUBE;
	cube_apply_moves(&cube, moves, count);
	status = validate_cube(&cube);
	if (status != PARSE_OK)
	{
		fprintf(stderr, "rubik: %s\n", parse_status_message(status));
		return (1);
	}
	if (cube_is_solved(&cube))
	{
		printf("\n");
		return (0);
	}
	fprintf(stderr, "rubik: solver not implemented yet\n");
	return (2);
}
