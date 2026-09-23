#include "rubik.h"

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
