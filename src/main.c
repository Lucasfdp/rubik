#include "rubik.h"

/// @brief Entry point: reads a scramble from argv[1] and parses it.
///
/// Usage: rubik "<scramble>", for example: rubik "R2 D' B'"
/// Currently it only parses and prints how many moves it found; errors go
/// to stderr with a message from parse_status_message().
///
/// @return 0 on success, 1 on wrong argument count or a parse error.
int	main(int ac, char const *av[])
{
	t_move			moves[MAX_MOVES];
	size_t			count;
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
	printf("parsed %zu move(s)\n", count);
	return (0);
}
