#include "twobytwo.h"

/// A coordinate that never changes: its only value is 0, and every move
/// maps 0 to 0. This lets the generic prune_build()/t_ida_phase machinery
/// -- built for THREE coordinates and two pruning PAIRS -- treat twist
/// and cperm as two independent 1-D tables: prune_dist(table, x, 0) is
/// just "distance of x alone" once the table's second coordinate has
/// exactly one possible value. Zero new code in prune.c or ida.c for
/// that: this static array is the entire trick.
static const uint16_t	g_zero_table[MOVE_COUNT] = {0};

bool	two_cube_is_solved(const t_cube *cube)
{
	int	i;

	i = 0;
	while (i < CORNER_COUNT)
	{
		if (cube->corner_perm[i] != (t_corner)i
			|| cube->corner_orient[i] != 0)
			return (false);
		i++;
	}
	return (true);
}

bool	two_solver_init(t_two_solver *solver)
{
	if (!move_tables_build(&solver->moves))
		return (false);
	if (!prune_build(&solver->twist_prune, solver->moves.twist, TWIST_COUNT,
			g_zero_table, 1, MOVES_ALL))
	{
		move_tables_free(&solver->moves);
		return (false);
	}
	if (!prune_build(&solver->cperm_prune, solver->moves.cperm, CPERM_COUNT,
			g_zero_table, 1, MOVES_ALL))
	{
		prune_free(&solver->twist_prune);
		move_tables_free(&solver->moves);
		return (false);
	}
	solver->phase.move_table[0] = solver->moves.twist;
	solver->phase.move_table[1] = solver->moves.cperm;
	solver->phase.move_table[2] = g_zero_table;
	solver->phase.prune_xz = &solver->twist_prune;
	solver->phase.prune_yz = &solver->cperm_prune;
	solver->phase.moves = MOVES_ALL;
	solver->phase.max_depth = TWOBYTWO_MAX_MOVES;
	return (true);
}

void	two_solver_free(t_two_solver *solver)
{
	prune_free(&solver->twist_prune);
	prune_free(&solver->cperm_prune);
	move_tables_free(&solver->moves);
}

/// One IDA* search, not two phases: a 2x2x2 has only one goal to reach
/// (corners solved), so there is nothing for a phase boundary to split.
int	two_solve(const t_two_solver *solver, const t_cube *cube, t_move *out)
{
	uint16_t	start[3];

	if (cube_corner_twist_sum(cube) != 0)
		return (-1);
	if (two_cube_is_solved(cube))
		return (0);
	start[0] = encode_twist(cube);
	start[1] = encode_cperm(cube);
	start[2] = 0;
	return (ida_search(&solver->phase, start, out));
}
