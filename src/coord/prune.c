#include <stdlib.h>
#include <string.h>
#include "prune.h"

/// @brief Marks every pair one move away from cell `cell` as depth + 1,
///        unless it already has a depth.
///
/// A pair index is a * count_b + b, so `cell / count_b` is a and
/// `cell % count_b` is b. Each move turns BOTH coordinates through their
/// own move table, then the two answers are put back together.
///
/// @return How many cells were newly reached.
static int	expand_cell(t_prune_table *table, const uint16_t *move_a,
	const uint16_t *move_b, t_move_mask moves, int cell, int depth)
{
	int	found;
	int	move;
	int	next;

	found = 0;
	move = 0;
	while (move < MOVE_COUNT)
	{
		if (move_in_mask(moves, (t_move)move))
		{
			next = move_a[(cell / table->count_b) * MOVE_COUNT + move]
				* table->count_b
				+ move_b[(cell % table->count_b) * MOVE_COUNT + move];
			if (table->dist[next] == PRUNE_UNVISITED)
			{
				table->dist[next] = (uint8_t)(depth + 1);
				found++;
			}
		}
		move++;
	}
	return (found);
}

/// @brief Runs the layer-by-layer BFS to fill in every cell's distance,
///        once `table->dist` has already been allocated and seeded (goal
///        cells at 0, everything else PRUNE_UNVISITED).
///
/// To make layer d + 1, walk the whole table and expand every cell that
/// holds d. Simple and needs no extra memory; about a dozen walks of a
/// million bytes is nothing. Stops the first time a layer finds nothing
/// new. Shared by prune_build() (one goal cell) and prune_build_coset()
/// (many goal cells) — everything past the seeding is identical.
static void	bfs_fill(t_prune_table *table, const uint16_t *move_a,
	const uint16_t *move_b, t_move_mask moves)
{
	int	depth;
	int	cell;
	int	found;

	depth = 0;
	found = 1;
	while (found > 0)
	{
		found = 0;
		cell = 0;
		while (cell < table->size)
		{
			if (table->dist[cell] == depth)
				found += expand_cell(table, move_a, move_b, moves, cell, depth);
			cell++;
		}
		if (found > 0)
			table->depth = ++depth;
	}
}

/// @brief Builds one pruning table by breadth-first search from (0, 0).
bool	prune_build(t_prune_table *table, const uint16_t *move_a, int count_a,
	const uint16_t *move_b, int count_b, t_move_mask moves)
{
	table->count_b = count_b;
	table->size = count_a * count_b;
	table->depth = 0;
	table->dist = malloc((size_t)table->size);
	if (!table->dist)
		return (false);
	memset(table->dist, PRUNE_UNVISITED, (size_t)table->size);
	table->dist[0] = 0;
	bfs_fill(table, move_a, move_b, moves);
	return (true);
}

/// @brief Forward BFS from 0 (docs/en/10-thistlethwaite-spec.md §3): start
/// with only 0 reached, then repeatedly walk every reached value and mark
/// its neighbours, until a whole pass finds nothing new. No queue needed,
/// same trick as bfs_fill().
bool	*reachable_set(const uint16_t *table, int count, t_move_mask moves)
{
	bool	*seen;
	bool	growing;
	int		state;
	int		move;
	uint16_t	next;

	seen = malloc((size_t)count * sizeof(bool));
	if (!seen)
		return (NULL);
	memset(seen, 0, (size_t)count * sizeof(bool));
	seen[0] = true;
	growing = true;
	while (growing)
	{
		growing = false;
		state = 0;
		while (state < count)
		{
			move = 0;
			while (seen[state] && move < MOVE_COUNT)
			{
				if (move_in_mask(moves, (t_move)move))
				{
					next = table[state * MOVE_COUNT + move];
					if (!seen[next])
					{
						seen[next] = true;
						growing = true;
					}
				}
				move++;
			}
			state++;
		}
	}
	return (seen);
}

/// @brief Builds a pruning table whose goal is every pair (a, b) with
///        goal_a[a] && goal_b[b], not just (0, 0).
bool	prune_build_coset(t_prune_table *table, const uint16_t *move_a,
	int count_a, const bool *goal_a, const uint16_t *move_b, int count_b,
	const bool *goal_b, t_move_mask moves)
{
	int	a;
	int	b;

	table->count_b = count_b;
	table->size = count_a * count_b;
	table->depth = 0;
	table->dist = malloc((size_t)table->size);
	if (!table->dist)
		return (false);
	memset(table->dist, PRUNE_UNVISITED, (size_t)table->size);
	a = 0;
	while (a < count_a)
	{
		b = 0;
		while (b < count_b)
		{
			if (goal_a[a] && goal_b[b])
				table->dist[a * count_b + b] = 0;
			b++;
		}
		a++;
	}
	bfs_fill(table, move_a, move_b, moves);
	return (true);
}

void	prune_free(t_prune_table *table)
{
	free(table->dist);
	memset(table, 0, sizeof(*table));
}

void	prune_tables_free(t_prune_tables *tables)
{
	prune_free(&tables->twist_slice);
	prune_free(&tables->flip_slice);
	prune_free(&tables->cperm_sperm);
	prune_free(&tables->eperm_sperm);
}

/// Phase 1 pairs use all 18 moves; phase 2 pairs use the 10 of
/// MOVES_PHASE2 (the only moves eperm and sperm have real answers for).
bool	prune_tables_build(t_prune_tables *tables, const t_move_tables *moves)
{
	memset(tables, 0, sizeof(*tables));
	if (!prune_build(&tables->twist_slice, moves->twist, TWIST_COUNT,
			moves->slice, SLICE_COUNT, MOVES_ALL)
		|| !prune_build(&tables->flip_slice, moves->flip, FLIP_COUNT,
			moves->slice, SLICE_COUNT, MOVES_ALL)
		|| !prune_build(&tables->cperm_sperm, moves->cperm, CPERM_COUNT,
			moves->sperm, SPERM_COUNT, MOVES_PHASE2)
		|| !prune_build(&tables->eperm_sperm, moves->eperm, EPERM_COUNT,
			moves->sperm, SPERM_COUNT, MOVES_PHASE2))
	{
		prune_tables_free(tables);
		return (false);
	}
	return (true);
}
