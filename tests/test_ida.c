#include "rubik.h"
#include "ida.h"
#include "testlib.h"

/// Tests for the IDA* driver (solve/ida.c). Real cubes, turned with the
/// real apply_move(), are the referee: a returned path is applied to a
/// cube and the cube must end up where the phase says, and the length is
/// compared with a brute-force search that knows nothing about the tables.

static t_move_tables	g_moves;
static t_prune_tables	g_prune;
static t_ida_phase		g_phase1;
static t_ida_phase		g_phase2;

/// Random cube made with `steps` moves out of `allowed`.
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

/// Phase 1 goal on a real cube: twist, flip and slice all 0.
static bool	phase1_done(const t_cube *cube)
{
	return (encode_twist(cube) == 0 && encode_flip(cube) == 0
		&& encode_slice(cube) == 0);
}

/// True if no two neighbouring moves of the path are redundant.
static bool	path_is_clean(const t_move *path, int length)
{
	int	i;

	i = 1;
	while (i < length)
	{
		if (ida_move_is_redundant(path[i - 1], path[i]))
			return (false);
		i++;
	}
	return (true);
}

/// The rule, checked for every pair of moves against its plain-words
/// version: a move is a waste if it turns the same face as the previous
/// move, or if the previous face is D, L or B and this face is its
/// opposite (U, R or F), because "U D" already covers "D U".
static void	test_redundancy_rule(void)
{
	int	last;
	int	move;
	int	same;
	int	back;

	CHECK(!ida_move_is_redundant(-1, MOVE_U1));
	CHECK(ida_move_is_redundant(MOVE_R1, MOVE_R3));
	CHECK(!ida_move_is_redundant(MOVE_U1, MOVE_D1));
	CHECK(ida_move_is_redundant(MOVE_D1, MOVE_U1));
	CHECK(!ida_move_is_redundant(MOVE_U1, MOVE_R1));
	last = 0;
	while (last < MOVE_COUNT)
	{
		move = 0;
		while (move < MOVE_COUNT)
		{
			same = last / 3 == move / 3;
			back = last / 3 >= 3 && move / 3 == last / 3 - 3;
			CHECK_EQ(ida_move_is_redundant(last, (t_move)move), same || back);
			move++;
		}
		last++;
	}
}

/// Already at the goal: 0 moves, both phases.
static void	test_goal_needs_no_moves(void)
{
	uint16_t	start[3];
	t_move		path[IDA_MAX_DEPTH];

	ida_phase1_start(&SOLVED_CUBE, start);
	CHECK_EQ(ida_estimate(&g_phase1, start), 0);
	CHECK_EQ(ida_search(&g_phase1, start, path), 0);
	ida_phase2_start(&SOLVED_CUBE, start);
	CHECK_EQ(ida_estimate(&g_phase2, start), 0);
	CHECK_EQ(ida_search(&g_phase2, start, path), 0);
}

/// Phase 1 on random cubes: the path takes the cube to twist 0, flip 0,
/// slice 0, has no redundant neighbours, and is within the cap.
static int	count_bad_phase1(int runs, uint32_t seed)
{
	t_cube		cube;
	uint16_t	start[3];
	t_move		path[IDA_MAX_DEPTH];
	int			bad;
	int			length;

	bad = 0;
	while (runs-- > 0)
	{
		scramble(&cube, MOVES_ALL, 30, &seed);
		ida_phase1_start(&cube, start);
		length = ida_search(&g_phase1, start, path);
		bad += length < 0 || length > IDA_PHASE1_CAP
			|| length < ida_estimate(&g_phase1, start)
			|| !path_is_clean(path, length);
		if (length >= 0)
			cube_apply_moves(&cube, path, (size_t)length);
		bad += !phase1_done(&cube);
	}
	return (bad);
}

/// Phase 2 on random cubes that phase 1 has already solved: the path
/// takes the cube all the way to solved.
static int	count_bad_phase2(int runs, uint32_t seed)
{
	t_cube		cube;
	uint16_t	start[3];
	t_move		path[IDA_MAX_DEPTH];
	int			bad;
	int			length;

	bad = 0;
	while (runs-- > 0)
	{
		scramble(&cube, MOVES_PHASE2, 40, &seed);
		ida_phase2_start(&cube, start);
		length = ida_search(&g_phase2, start, path);
		bad += length < 0 || length > IDA_PHASE2_CAP
			|| length < ida_estimate(&g_phase2, start)
			|| !path_is_clean(path, length);
		if (length >= 0)
			cube_apply_moves(&cube, path, (size_t)length);
		bad += !cube_is_solved(&cube);
	}
	return (bad);
}

static void	test_each_phase_reaches_its_goal(void)
{
	CHECK_EQ(count_bad_phase1(100, 11), 0);
	CHECK_EQ(count_bad_phase2(100, 12), 0);
}

/// Both phases in a row from a fully scrambled cube: solved at the end,
/// and never more than the two caps added together.
static void	test_both_phases_solve_the_cube(void)
{
	t_cube		cube;
	uint16_t	start[3];
	t_move		path[IDA_MAX_DEPTH];
	uint32_t	seed;
	int			bad;
	int			run;
	int			length;

	seed = 13;
	bad = 0;
	run = -1;
	while (++run < 100)
	{
		scramble(&cube, MOVES_ALL, 30, &seed);
		ida_phase1_start(&cube, start);
		length = ida_search(&g_phase1, start, path);
		bad += length < 0;
		if (length >= 0)
			cube_apply_moves(&cube, path, (size_t)length);
		ida_phase2_start(&cube, start);
		length = ida_search(&g_phase2, start, path);
		bad += length < 0;
		if (length >= 0)
			cube_apply_moves(&cube, path, (size_t)length);
		bad += !cube_is_solved(&cube);
	}
	CHECK_EQ(bad, 0);
}

/// Brute force on real cubes, with no tables: is there a sequence of
/// exactly `left` moves out of g_mask that ends where g_goal says?
static bool				(*g_goal)(const t_cube *);
static t_move_mask		g_mask;

static bool	exists_in(const t_cube *cube, int left)
{
	t_cube	next;
	int		move;

	if (left == 0)
		return (g_goal(cube));
	move = -1;
	while (++move < MOVE_COUNT)
	{
		if (!move_in_mask(g_mask, (t_move)move))
			continue ;
		next = *cube;
		apply_move(&next, (t_move)move);
		if (exists_in(&next, left - 1))
			return (true);
	}
	return (false);
}

/// The fewest moves (0 .. limit) that reach g_goal, or -1 if none does.
static int	brute_force_length(const t_cube *cube, int limit)
{
	int	length;

	length = 0;
	while (length <= limit)
	{
		if (exists_in(cube, length))
			return (length);
		length++;
	}
	return (-1);
}

/// The driver's length must equal the brute-force minimum: not just a
/// solution, the SHORTEST one. Scrambles of `steps` moves have a minimum
/// of at most `steps`.
///
/// @return How many scrambles disagree.
static int	count_not_shortest(const t_ida_phase *phase, bool phase1,
	int steps, uint32_t seed)
{
	t_cube		cube;
	uint16_t	start[3];
	t_move		path[IDA_MAX_DEPTH];
	int			bad;
	int			run;

	g_mask = phase->moves;
	g_goal = cube_is_solved;
	if (phase1)
		g_goal = phase1_done;
	bad = 0;
	run = -1;
	while (++run < 30)
	{
		scramble(&cube, phase->moves, steps, &seed);
		if (phase1)
			ida_phase1_start(&cube, start);
		else
			ida_phase2_start(&cube, start);
		bad += ida_search(phase, start, path) != brute_force_length(&cube,
				steps);
	}
	return (bad);
}

static void	test_solutions_are_shortest(void)
{
	CHECK_EQ(count_not_shortest(&g_phase1, true, 4, 21), 0);
	CHECK_EQ(count_not_shortest(&g_phase2, false, 5, 22), 0);
}

/// A cap that is too small gives -1 instead of a wrong answer.
static void	test_cap_too_small(void)
{
	t_ida_phase	small;
	t_cube		cube;
	uint16_t	start[3];
	t_move		path[IDA_MAX_DEPTH];
	uint32_t	seed;

	seed = 31;
	small = g_phase1;
	scramble(&cube, MOVES_ALL, 30, &seed);
	ida_phase1_start(&cube, start);
	while (ida_estimate(&small, start) < 4)
	{
		scramble(&cube, MOVES_ALL, 30, &seed);
		ida_phase1_start(&cube, start);
	}
	small.max_depth = 2;
	CHECK_EQ(ida_search(&small, start, path), -1);
	small.max_depth = 100;
	CHECK(ida_search(&small, start, path) >= 4);
}

int	main(void)
{
	if (!move_tables_build(&g_moves) || !prune_tables_build(&g_prune, &g_moves))
	{
		fprintf(stderr, "  could not build the tables\n");
		return (1);
	}
	ida_phase1_setup(&g_phase1, &g_moves, &g_prune);
	ida_phase2_setup(&g_phase2, &g_moves, &g_prune);
	test_redundancy_rule();
	test_goal_needs_no_moves();
	test_each_phase_reaches_its_goal();
	test_both_phases_solve_the_cube();
	test_solutions_are_shortest();
	test_cap_too_small();
	prune_tables_free(&g_prune);
	move_tables_free(&g_moves);
	return (test_report("ida"));
}
