#include "rubik.h"
#include "testlib.h"

/// Tests for the pruning tables (coord/prune.c). Links the coord files +
/// cubie.c + moves.c: real cubes, turned with the real apply_move(), are
/// the referee, never the tables' own move tables.

typedef uint16_t	(*t_encode_fn_ptr)(const t_cube *);

static t_move_tables	g_moves;
static t_prune_tables	g_prune;

/// How many cells of a table hold `value`.
static int	count_cells(const t_prune_table *table, int value)
{
	int	cell;
	int	found;

	found = 0;
	cell = 0;
	while (cell < table->size)
	{
		if (table->dist[cell] == value)
			found++;
		cell++;
	}
	return (found);
}

/// Sizes match docs/en/02-algorithms.md, and only the goal (0, 0) is 0.
static void	test_sizes_and_goal(void)
{
	CHECK_EQ(g_prune.twist_slice.size, 1082565);
	CHECK_EQ(g_prune.flip_slice.size, 1013760);
	CHECK_EQ(g_prune.cperm_sperm.size, 967680);
	CHECK_EQ(g_prune.eperm_sperm.size, 967680);
	CHECK_EQ(prune_dist(&g_prune.twist_slice, 0, 0), 0);
	CHECK_EQ(count_cells(&g_prune.twist_slice, 0), 1);
	CHECK_EQ(count_cells(&g_prune.flip_slice, 0), 1);
	CHECK_EQ(count_cells(&g_prune.cperm_sperm, 0), 1);
	CHECK_EQ(count_cells(&g_prune.eperm_sperm, 0), 1);
}

/// Every pair can be reached, so no cell may still hold the sentinel.
static void	test_no_unvisited_cell(void)
{
	CHECK_EQ(count_cells(&g_prune.twist_slice, PRUNE_UNVISITED), 0);
	CHECK_EQ(count_cells(&g_prune.flip_slice, PRUNE_UNVISITED), 0);
	CHECK_EQ(count_cells(&g_prune.cperm_sperm, PRUNE_UNVISITED), 0);
	CHECK_EQ(count_cells(&g_prune.eperm_sperm, PRUNE_UNVISITED), 0);
}

/// One move changes the distance by at most 1 (a move and its undo are both
/// in the set). Counts the (cell, move) pairs that break that.
static int	count_jumps(const t_prune_table *table, const uint16_t *move_a,
	const uint16_t *move_b, t_move_mask moves)
{
	int	cell;
	int	move;
	int	next;
	int	diff;
	int	bad;

	bad = 0;
	cell = 0;
	while (cell < table->size)
	{
		move = -1;
		while (++move < MOVE_COUNT)
		{
			if (!move_in_mask(moves, (t_move)move))
				continue ;
			next = move_a[(cell / table->count_b) * MOVE_COUNT + move]
				* table->count_b
				+ move_b[(cell % table->count_b) * MOVE_COUNT + move];
			diff = table->dist[next] - table->dist[cell];
			bad += (diff > 1 || diff < -1);
		}
		cell++;
	}
	return (bad);
}

static void	test_neighbours_differ_by_at_most_one(void)
{
	const t_move_tables	*m;

	m = &g_moves;
	CHECK_EQ(count_jumps(&g_prune.twist_slice, m->twist, m->slice,
			MOVES_ALL), 0);
	CHECK_EQ(count_jumps(&g_prune.flip_slice, m->flip, m->slice,
			MOVES_ALL), 0);
	CHECK_EQ(count_jumps(&g_prune.cperm_sperm, m->cperm, m->sperm,
			MOVES_PHASE2), 0);
	CHECK_EQ(count_jumps(&g_prune.eperm_sperm, m->eperm, m->sperm,
			MOVES_PHASE2), 0);
}

/// State of the exact-distance walk (globals: test code, one walk at a time).
static const t_prune_table	*g_table;
static t_encode_fn_ptr		g_enc_a;
static t_encode_fn_ptr		g_enc_b;
static t_move_mask			g_mask;
static int					g_limit;
static uint8_t				*g_best;

/// Tries EVERY move sequence up to g_limit moves on a real cube and keeps,
/// for each pair, the fewest moves that reached it.
static void	walk(const t_cube *cube, int depth)
{
	t_cube	next;
	int		cell;
	int		move;

	cell = prune_index(g_table, g_enc_a(cube), g_enc_b(cube));
	if (depth < g_best[cell])
		g_best[cell] = (uint8_t)depth;
	if (depth == g_limit)
		return ;
	move = -1;
	while (++move < MOVE_COUNT)
	{
		if (!move_in_mask(g_mask, (t_move)move))
			continue ;
		next = *cube;
		apply_move(&next, (t_move)move);
		walk(&next, depth + 1);
	}
}

/// Compares a table with real cubes up to `limit` moves: a cell must hold
/// distance d (d <= limit) exactly when real cubes reach that pair in d
/// moves, and must hold more than `limit` when they do not.
///
/// @return How many cells disagree, or -1 if malloc failed.
static int	count_wrong_cells(const t_prune_table *table, t_encode_fn_ptr a,
	t_encode_fn_ptr b, t_move_mask moves, int limit)
{
	int	cell;
	int	bad;

	g_best = malloc((size_t)table->size);
	if (!g_best)
		return (-1);
	memset(g_best, 255, (size_t)table->size);
	g_table = table;
	g_enc_a = a;
	g_enc_b = b;
	g_mask = moves;
	g_limit = limit;
	walk(&SOLVED_CUBE, 0);
	bad = 0;
	cell = -1;
	while (++cell < table->size)
	{
		if (g_best[cell] <= limit)
			bad += (table->dist[cell] != g_best[cell]);
		else
			bad += (table->dist[cell] <= limit);
	}
	free(g_best);
	return (bad);
}

static void	test_exact_for_short_distances(void)
{
	CHECK_EQ(count_wrong_cells(&g_prune.twist_slice, encode_twist,
			encode_slice, MOVES_ALL, 4), 0);
	CHECK_EQ(count_wrong_cells(&g_prune.flip_slice, encode_flip,
			encode_slice, MOVES_ALL, 4), 0);
	CHECK_EQ(count_wrong_cells(&g_prune.cperm_sperm, encode_cperm,
			encode_sperm, MOVES_PHASE2, 5), 0);
	CHECK_EQ(count_wrong_cells(&g_prune.eperm_sperm, encode_eperm,
			encode_sperm, MOVES_PHASE2, 5), 0);
}

/// Random cube made with `steps` moves out of `allowed`; the estimate must
/// never be more than `steps` (it is a LOWER bound on the moves needed,
/// and `steps` moves are enough). Returns how many walks broke that.
static int	count_overestimates(t_move_mask allowed, bool phase1,
	uint32_t seed)
{
	t_cube	cube;
	int		bad;
	int		walks;
	int		steps;
	t_move	move;

	bad = 0;
	walks = -1;
	while (++walks < 3000)
	{
		cube = SOLVED_CUBE;
		steps = 0;
		while (steps < 40)
		{
			move = (t_move)(test_rand(&seed) % MOVE_COUNT);
			if (!move_in_mask(allowed, move))
				continue ;
			apply_move(&cube, move);
			steps++;
			if (phase1)
				bad += prune_phase1(&g_prune, encode_twist(&cube),
						encode_flip(&cube), encode_slice(&cube)) > steps;
			else
				bad += prune_phase2(&g_prune, encode_cperm(&cube),
						encode_eperm(&cube), encode_sperm(&cube)) > steps;
		}
	}
	return (bad);
}

static void	test_never_overestimates(void)
{
	CHECK_EQ(count_overestimates(MOVES_ALL, true, 7), 0);
	CHECK_EQ(count_overestimates(MOVES_PHASE2, false, 8), 0);
}

/// The tables have several layers (a table that is all 0s and 1s would be
/// useless as a heuristic) and stay within the known worst cases.
static void	test_depths(void)
{
	CHECK(g_prune.twist_slice.depth >= 5 && g_prune.twist_slice.depth <= 12);
	CHECK(g_prune.flip_slice.depth >= 5 && g_prune.flip_slice.depth <= 12);
	CHECK(g_prune.cperm_sperm.depth >= 5 && g_prune.cperm_sperm.depth <= 18);
	CHECK(g_prune.eperm_sperm.depth >= 5 && g_prune.eperm_sperm.depth <= 18);
}

/// Building again works, and free is safe to repeat.
static void	test_build_free_cycle(void)
{
	t_prune_tables	second;

	CHECK(prune_tables_build(&second, &g_moves));
	CHECK_EQ(second.twist_slice.depth, g_prune.twist_slice.depth);
	CHECK_EQ(second.eperm_sperm.depth, g_prune.eperm_sperm.depth);
	prune_tables_free(&second);
	CHECK(second.twist_slice.dist == NULL && second.eperm_sperm.dist == NULL);
	prune_tables_free(&second);
}

int	main(void)
{
	if (!move_tables_build(&g_moves) || !prune_tables_build(&g_prune, &g_moves))
	{
		fprintf(stderr, "  could not build the tables\n");
		return (1);
	}
	test_sizes_and_goal();
	test_no_unvisited_cell();
	test_neighbours_differ_by_at_most_one();
	test_exact_for_short_distances();
	test_never_overestimates();
	test_depths();
	test_build_free_cycle();
	prune_tables_free(&g_prune);
	move_tables_free(&g_moves);
	return (test_report("prune"));
}
