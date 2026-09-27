#include <string.h>
#include "solve.h"

bool	solver_init(t_solver *solver)
{
	if (!move_tables_build(&solver->moves))
		return (false);
	if (!prune_tables_build(&solver->prune, &solver->moves))
	{
		move_tables_free(&solver->moves);
		return (false);
	}
	ida_phase1_setup(&solver->phase1, &solver->moves, &solver->prune);
	ida_phase2_setup(&solver->phase2, &solver->moves, &solver->prune);
	return (true);
}

void	solver_free(t_solver *solver)
{
	prune_tables_free(&solver->prune);
	move_tables_free(&solver->moves);
}

/// @brief Adds the turns of two moves on the same face: 1 + 1 = 2, 1 + 3 =
///        0 (they undo each other). A move is face * 3 + (quarter turns -
///        1), so quarter turns are move % 3 + 1 and add up mod 4.
///
/// @return The combined move, or -1 if they cancel out.
static int	combine_moves(t_move a, t_move b)
{
	int	quarters;

	quarters = ((int)a % 3 + 1 + (int)b % 3 + 1) % 4;
	if (quarters == 0)
		return (-1);
	return ((int)a / 3 * 3 + quarters - 1);
}

/// @brief Puts `move` on the end of the list, merging it with the move
///        before it if they are on the same face (or the one before that,
///        if the last move is on the opposite face and so commutes).
///
/// A cancelled pair is removed from the list, later moves shift down.
static void	push_move(t_move *list, int *size, t_move move)
{
	int	at;
	int	merged;

	at = *size - 1;
	if (at >= 1 && (int)list[at] / 3 == ((int)move / 3 + 3) % 6
		&& (int)list[at - 1] / 3 == (int)move / 3)
		at--;
	if (at < 0 || (int)list[at] / 3 != (int)move / 3)
	{
		list[(*size)++] = move;
		return ;
	}
	merged = combine_moves(list[at], move);
	if (merged >= 0)
	{
		list[at] = (t_move)merged;
		return ;
	}
	while (at + 1 < *size)
	{
		list[at] = list[at + 1];
		at++;
	}
	(*size)--;
}

/// One pass keeps a list of the moves so far and pushes each move on it.
/// Removing a pair can make its neighbours mergeable, and a pass only
/// looks back, so passes repeat until the length stops changing.
int	simplify_moves(t_move *moves, int count)
{
	int	before;
	int	size;
	int	i;

	before = -1;
	while (before != count)
	{
		before = count;
		size = 0;
		i = 0;
		while (i < count)
		{
			push_move(moves, &size, moves[i]);
			i++;
		}
		count = size;
	}
	return (count);
}

/// Each phase searches into its own IDA_MAX_DEPTH buffer (what ida_search()
/// asks for) and the moves are copied into `out` one after the other.
/// Phase 2 starts from the cube AFTER the phase 1 moves, because its
/// coordinates only mean something once phase 1 is done (coord.h).
int	solve(const t_solver *solver, const t_cube *cube, t_move *out)
{
	t_move		path[IDA_MAX_DEPTH];
	t_cube		work;
	uint16_t	start[3];
	int			first;
	int			second;

	if (!cube_is_valid(cube))
		return (-1);
	work = *cube;
	ida_phase1_start(&work, start);
	first = ida_search(&solver->phase1, start, path);
	if (first < 0)
		return (-1);
	memcpy(out, path, (size_t)first * sizeof(t_move));
	cube_apply_moves(&work, path, (size_t)first);
	ida_phase2_start(&work, start);
	second = ida_search(&solver->phase2, start, path);
	if (second < 0)
		return (-1);
	memcpy(out + first, path, (size_t)second * sizeof(t_move));
	return (simplify_moves(out, first + second));
}
