#include "rubik.h"
#include "testlib.h"

/// Tests for Thistlethwaite's four-phase solver (solve/thistlethwaite.c).
/// Two tiers: a synthetic, fully hand-checkable tier for the genuinely NEW
/// algorithmic pieces (reachable_set(), prune_build_coset()) that doesn't
/// need real cubes at all, and a real-cube tier (move tables, real
/// scrambles, real apply_move()) for everything built on top of them,
/// matching the rest of this project's testing style.

static t_move_tables		g_moves;
static t_thistle_solver	g_thistle;

/* ---------------------------------------------------------------------- */
/* Tier 1: synthetic tables, hand-checkable by construction               */
/* ---------------------------------------------------------------------- */

/// A 4-state coordinate: MOVE_U1 cycles 0 -> 1 -> 2 -> 3 -> 0. Every other
/// move is the identity (never read by these tests, since the masks below
/// never include them, but filled in for a well-formed table).
static uint16_t	g_fake_a[4 * MOVE_COUNT];

/// A 1-state coordinate, exactly like the real DUMMY_MOVES: always 0.
static uint16_t	g_fake_b[1 * MOVE_COUNT];

static void	build_fake_tables(void)
{
	int	state;
	int	move;

	state = 0;
	while (state < 4)
	{
		move = 0;
		while (move < MOVE_COUNT)
		{
			g_fake_a[state * MOVE_COUNT + move] = (uint16_t)state;
			move++;
		}
		state++;
	}
	g_fake_a[0 * MOVE_COUNT + MOVE_U1] = 1;
	g_fake_a[1 * MOVE_COUNT + MOVE_U1] = 2;
	g_fake_a[2 * MOVE_COUNT + MOVE_U1] = 3;
	g_fake_a[3 * MOVE_COUNT + MOVE_U1] = 0;
	move = 0;
	while (move < MOVE_COUNT)
	{
		g_fake_b[move] = 0;
		move++;
	}
}

/// With every move allowed, all 4 states are reachable from 0. With no
/// move allowed, only 0 is.
static void	test_reachable_set_synthetic(void)
{
	bool	*full;
	bool	*none;

	full = reachable_set(g_fake_a, 4, MOVE_BIT(MOVE_U1));
	CHECK(full != NULL);
	if (full)
	{
		CHECK(full[0] && full[1] && full[2] && full[3]);
		free(full);
	}
	none = reachable_set(g_fake_a, 4, 0);
	CHECK(none != NULL);
	if (none)
	{
		CHECK(none[0] && !none[1] && !none[2] && !none[3]);
		free(none);
	}
}

/// One goal (state 0): distances walking the cycle forward are 0,1,2,3.
static void	test_prune_build_coset_single_seed(void)
{
	t_prune_table	table;
	bool			goal_a[4];
	bool			goal_b[1];

	goal_a[0] = true;
	goal_a[1] = false;
	goal_a[2] = false;
	goal_a[3] = false;
	goal_b[0] = true;
	CHECK(prune_build_coset(&table, g_fake_a, 4, goal_a, g_fake_b, 1,
			goal_b, MOVE_BIT(MOVE_U1)));
	CHECK_EQ(table.size, 4);
	CHECK_EQ(prune_dist(&table, 0, 0), 0);
	CHECK_EQ(prune_dist(&table, 1, 0), 1);
	CHECK_EQ(prune_dist(&table, 2, 0), 2);
	CHECK_EQ(prune_dist(&table, 3, 0), 3);
	CHECK_EQ(table.depth, 3);
	prune_free(&table);
}

/// Two goals (states 0 and 2): every other state is now only 1 step from
/// its nearest goal, proving multi-source seeding actually shortens
/// distances instead of just picking one seed and ignoring the rest.
static void	test_prune_build_coset_multi_seed(void)
{
	t_prune_table	table;
	bool			goal_a[4];
	bool			goal_b[1];

	goal_a[0] = true;
	goal_a[1] = false;
	goal_a[2] = true;
	goal_a[3] = false;
	goal_b[0] = true;
	CHECK(prune_build_coset(&table, g_fake_a, 4, goal_a, g_fake_b, 1,
			goal_b, MOVE_BIT(MOVE_U1)));
	CHECK_EQ(prune_dist(&table, 0, 0), 0);
	CHECK_EQ(prune_dist(&table, 1, 0), 1);
	CHECK_EQ(prune_dist(&table, 2, 0), 0);
	CHECK_EQ(prune_dist(&table, 3, 0), 1);
	CHECK_EQ(table.depth, 1);
	prune_free(&table);
}

/// A goal that only exists in the SECOND coordinate's slot (goal_b false
/// everywhere) seeds nothing: every cell stays unreached until real moves
/// (none given here) would connect them, so building must still succeed
/// and every reachable-from-nothing cell keeps the sentinel.
static void	test_prune_build_coset_empty_goal(void)
{
	t_prune_table	table;
	bool			goal_a[4];
	bool			goal_b[1];
	int				i;

	i = 0;
	while (i < 4)
		goal_a[i++] = false;
	goal_b[0] = true;
	CHECK(prune_build_coset(&table, g_fake_a, 4, goal_a, g_fake_b, 1,
			goal_b, MOVE_BIT(MOVE_U1)));
	i = 0;
	while (i < 4)
	{
		CHECK_EQ(table.dist[i], PRUNE_UNVISITED);
		i++;
	}
	prune_free(&table);
}

/* ---------------------------------------------------------------------- */
/* Tier 2: real move tables and real cubes                                */
/* ---------------------------------------------------------------------- */

static int	mask_size(t_move_mask mask)
{
	int	move;
	int	size;

	size = 0;
	move = 0;
	while (move < MOVE_COUNT)
	{
		size += move_in_mask(mask, (t_move)move);
		move++;
	}
	return (size);
}

/// The three masks are the right sizes, and nested as the group chain
/// requires: G3 moves are a subset of G2's (== MOVES_PHASE2), which are a
/// subset of G1's, which are a subset of every move there is.
static void	test_masks(void)
{
	int	move;

	CHECK_EQ(mask_size(MOVES_G1), 14);
	CHECK_EQ(mask_size(MOVES_G2), 10);
	CHECK_EQ(mask_size(MOVES_G3), 6);
	move = 0;
	while (move < MOVE_COUNT)
	{
		if (move_in_mask(MOVES_G3, (t_move)move))
			CHECK(move_in_mask(MOVES_G2, (t_move)move));
		if (move_in_mask(MOVES_G2, (t_move)move))
			CHECK(move_in_mask(MOVES_G1, (t_move)move));
		if (move_in_mask(MOVES_G1, (t_move)move))
			CHECK(move_in_mask(MOVES_ALL, (t_move)move));
		move++;
	}
}

/// G3's own three goal sets: 0 is always a member (it's solved), and each
/// set is CLOSED under MOVES_G3 (leaving a member with a G3 move can only
/// land on another member) — the structural property that makes each set
/// an honest subgroup, and that reachable_set()'s fixed-point loop must
/// produce for its answer to be trustworthy at all.
static int	count_not_closed(const bool *set, const uint16_t *table,
	int count)
{
	int	state;
	int	move;
	int	bad;

	bad = 0;
	state = 0;
	while (state < count)
	{
		if (set[state])
		{
			move = 0;
			while (move < MOVE_COUNT)
			{
				if (move_in_mask(MOVES_G3, (t_move)move))
					bad += !set[table[state * MOVE_COUNT + move]];
				move++;
			}
		}
		state++;
	}
	return (bad);
}

static void	test_goal_sets_are_closed_subgroups(void)
{
	CHECK(g_thistle.prune.cp_g3[0]);
	CHECK(g_thistle.prune.ep_g3[0]);
	CHECK(g_thistle.prune.sp_g3[0]);
	CHECK_EQ(count_not_closed(g_thistle.prune.cp_g3, g_moves.cperm,
			CPERM_COUNT), 0);
	CHECK_EQ(count_not_closed(g_thistle.prune.ep_g3, g_moves.eperm,
			EPERM_COUNT), 0);
	CHECK_EQ(count_not_closed(g_thistle.prune.sp_g3, g_moves.sperm,
			SPERM_COUNT), 0);
}

/// Random cube built entirely from `steps` moves out of `allowed`.
static void	scramble(t_cube *cube, t_move_mask allowed, int steps,
	uint32_t *rng)
{
	t_move	move;

	*cube = SOLVED_CUBE;
	while (steps > 0)
	{
		move = (t_move)(test_rand(rng) % MOVE_COUNT);
		if (move_in_mask(allowed, move))
		{
			apply_move(cube, move);
			steps--;
		}
	}
}

/// A cube built from ONLY G3 moves is, by definition, a member of G3: its
/// cperm / eperm / sperm must all show up as true in the goal sets. This
/// is the real-cube counterpart of test_goal_sets_are_closed_subgroups().
static void	test_g3_scrambles_are_recognised(void)
{
	t_cube		cube;
	uint32_t	seed;
	int			bad;
	int			run;

	seed = 501;
	bad = 0;
	run = -1;
	while (++run < 300)
	{
		scramble(&cube, MOVES_G3, 20, &seed);
		bad += !g_thistle.prune.cp_g3[encode_cperm(&cube)];
		bad += !g_thistle.prune.ep_g3[encode_eperm(&cube)];
		bad += !g_thistle.prune.sp_g3[encode_sperm(&cube)];
	}
	CHECK_EQ(bad, 0);
}

/// Direct estimate helper mirroring ida_estimate() for phase 3 / phase 4's
/// shape (max of the two paired tables), without needing a whole
/// t_ida_phase for a one-off check.
static int	pair_estimate(const t_prune_table *xz, const t_prune_table *yz,
	int x, int y, int z)
{
	int	a;
	int	b;

	a = prune_dist(xz, x, z);
	b = prune_dist(yz, y, z);
	if (a > b)
		return (a);
	return (b);
}

static void	test_phase3_never_overestimates(void)
{
	t_cube		cube;
	uint32_t	seed;
	int			bad;
	int			steps;
	int			run;
	t_move		move;

	seed = 502;
	bad = 0;
	run = -1;
	while (++run < 500)
	{
		cube = SOLVED_CUBE;
		steps = 0;
		while (steps < 13)
		{
			move = (t_move)(test_rand(&seed) % MOVE_COUNT);
			if (!move_in_mask(MOVES_G2, move))
				continue ;
			apply_move(&cube, move);
			steps++;
			bad += pair_estimate(&g_thistle.prune.cperm_sperm3,
					&g_thistle.prune.eperm_sperm3, encode_cperm(&cube),
					encode_eperm(&cube), encode_sperm(&cube)) > steps;
		}
	}
	CHECK_EQ(bad, 0);
}

/// Phase 4's tables are ordinary single-goal tables (built with
/// prune_build(), tight MOVES_G3), so the usual never-overestimate check
/// applies directly: from a G3 member scrambled by k random G3 moves, at
/// most k moves are needed back to solved.
static void	test_phase4_never_overestimates(void)
{
	t_cube		cube;
	uint32_t	seed;
	int			bad;
	int			steps;
	int			run;
	t_move		move;

	seed = 503;
	bad = 0;
	run = -1;
	while (++run < 500)
	{
		cube = SOLVED_CUBE;
		steps = 0;
		while (steps < 15)
		{
			move = (t_move)(test_rand(&seed) % MOVE_COUNT);
			if (!move_in_mask(MOVES_G3, move))
				continue ;
			apply_move(&cube, move);
			steps++;
			bad += pair_estimate(&g_thistle.prune.cperm_sperm4,
					&g_thistle.prune.eperm_sperm4, encode_cperm(&cube),
					encode_eperm(&cube), encode_sperm(&cube)) > steps;
		}
	}
	CHECK_EQ(bad, 0);
}

/// Applying phase 1's / phase 2's own solution must land the cube exactly
/// on flip == 0 (phase 1) or twist == 0 && slice == 0 (phase 2) — the
/// single-goal phases, checked the same way tests/test_ida.c checks
/// Kociemba's phase 1.
static void	test_phase1_and_phase2_reach_their_goal(void)
{
	t_cube		cube;
	t_move		path[IDA_MAX_DEPTH];
	uint16_t	start[3];
	uint32_t	seed;
	int			bad;
	int			run;
	int			length;

	seed = 504;
	bad = 0;
	run = -1;
	while (++run < 200)
	{
		scramble(&cube, MOVES_ALL, 25, &seed);
		start[0] = encode_flip(&cube);
		start[1] = start[0];
		start[2] = 0;
		length = ida_search(&g_thistle.phase1, start, path);
		bad += length < 0 || length > THISTLE_PHASE1_CAP;
		if (length >= 0)
			cube_apply_moves(&cube, path, (size_t)length);
		bad += encode_flip(&cube) != 0;
		start[0] = encode_twist(&cube);
		start[1] = start[0];
		start[2] = encode_slice(&cube);
		length = ida_search(&g_thistle.phase2, start, path);
		bad += length < 0 || length > THISTLE_PHASE2_CAP;
		if (length >= 0)
			cube_apply_moves(&cube, path, (size_t)length);
		bad += encode_twist(&cube) != 0 || encode_slice(&cube) != 0;
	}
	CHECK_EQ(bad, 0);
}

/// End to end: every phase chained by thistle_solve(), applied to a real
/// cube, must reach solved — the same shape as tests/test_solve.c's
/// test_random_scrambles(), and the ultimate check that the coset logic,
/// the masks and the phase order all actually compose correctly.
static void	test_random_scrambles_solve(void)
{
	t_cube		cube;
	t_cube		after;
	t_move		out[THISTLE_MAX_MOVES];
	uint32_t	seed;
	int			bad;
	int			count;
	int			longest;
	int			total;
	int			run;

	seed = 91;
	bad = 0;
	longest = 0;
	total = 0;
	run = -1;
	while (++run < 200)
	{
		scramble(&cube, MOVES_ALL, 30, &seed);
		count = thistle_solve(&g_thistle, &cube, out);
		bad += count < 0 || count > THISTLE_MAX_MOVES;
		after = cube;
		if (count > 0)
			cube_apply_moves(&after, out, (size_t)count);
		bad += !cube_is_solved(&after);
		if (count > longest)
			longest = count;
		total += count;
	}
	CHECK_EQ(bad, 0);
	/// docs/en/02-algorithms.md §3.B: 40-45 moves average, well inside
	/// R4's 50-word ceiling. A generous ceiling here (60) catches a real
	/// regression without being flaky over the worst-case tail.
	CHECK(longest <= 60);
	CHECK((double)total / 200.0 < 50.0);
}

static void	test_already_solved(void)
{
	t_move	out[THISTLE_MAX_MOVES];

	CHECK_EQ(thistle_solve(&g_thistle, &SOLVED_CUBE, out), 0);
}

static void	test_invalid_cube_is_refused(void)
{
	t_cube	cube;
	t_move	out[THISTLE_MAX_MOVES];

	cube = SOLVED_CUBE;
	cube.corner_orient[0] = 1;
	CHECK_EQ(thistle_solve(&g_thistle, &cube, out), -1);
}

/// Building and freeing again works, and free is safe to repeat.
static void	test_init_free_cycle(void)
{
	t_thistle_solver	second;
	t_move				out[THISTLE_MAX_MOVES];
	t_cube				cube;
	uint32_t			seed;

	seed = 17;
	CHECK(thistle_init(&second, &g_moves));
	scramble(&cube, MOVES_ALL, 30, &seed);
	CHECK(thistle_solve(&second, &cube, out) >= 0);
	thistle_free(&second);
	thistle_free(&second);
}

int	main(void)
{
	build_fake_tables();
	test_reachable_set_synthetic();
	test_prune_build_coset_single_seed();
	test_prune_build_coset_multi_seed();
	test_prune_build_coset_empty_goal();
	if (!move_tables_build(&g_moves) || !thistle_init(&g_thistle, &g_moves))
	{
		fprintf(stderr, "  could not build the thistlethwaite solver\n");
		return (1);
	}
	test_masks();
	test_goal_sets_are_closed_subgroups();
	test_g3_scrambles_are_recognised();
	test_phase3_never_overestimates();
	test_phase4_never_overestimates();
	test_phase1_and_phase2_reach_their_goal();
	test_random_scrambles_solve();
	test_already_solved();
	test_invalid_cube_is_refused();
	test_init_free_cycle();
	thistle_free(&g_thistle);
	move_tables_free(&g_moves);
	return (test_report("thistlethwaite"));
}
