#include <stdlib.h>
#include "scramble.h"

/// @brief One candidate move per iteration; re-rolled whenever it would
///        turn the same face as the previous move (candidate / 3 is the
///        face index, per cube.h's t_move layout: face * 3 + turn).
void	scramble_generate(t_move *out, size_t count, unsigned int *seed)
{
	size_t	i;
	t_move	candidate;

	i = 0;
	while (i < count)
	{
		candidate = (t_move)(rand_r(seed) % MOVE_COUNT);
		if (i == 0 || candidate / 3 != out[i - 1] / 3)
		{
			out[i] = candidate;
			i++;
		}
	}
}
