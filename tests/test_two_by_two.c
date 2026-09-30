#include "rubik.h"
#include "testlib.h"

/// Tests for two_solve() (include/twobytwo.h): 2x2x2 support is
/// corner-only Kociemba, so these mirror test_solve.c's shape but check
/// corners alone (two_cube_is_solved()), never the whole cube -- and
/// prove edges are read by nothing here.

static t_two_solver	g_solver;

/// Random cube made with `steps` random moves. Only the cube is returned:
/// two_solve() never sees the moves.
static void	scramble(t_cube *cube, int steps, uint32_t *rng)
{
	t_move	move;

	*cube = SOLVED_CUBE;
	while (steps-- > 0)
	{
		move = (t_move)(test_rand(rng) % MOVE_COUNT);
		apply_move(cube, move);
	}
}

static void	test_already_solved(void)
{
	t_move	out[TWOBYTWO_MAX_MOVES];

	CHECK_EQ(two_solve(&g_solver, &SOLVED_CUBE, out), 0);
}

/// Random scrambles: the solution is never longer than TWOBYTWO_MAX_MOVES
/// (the proven bound, docs/en/14-other-puzzles.md), and applying it to
/// the scramble solves the corners -- not necessarily the edges, which
/// is the whole point of a 2x2x2.
static void	test_random_scrambles(void)
{
	t_cube		cube;
	t_cube		after;
	t_move		out[TWOBYTWO_MAX_MOVES];
	uint32_t	seed;
	int			bad;
	int			count;
	int			run;

	seed = 13;
	bad = 0;
	run = -1;
	while (++run < 500)
	{
		scramble(&cube, 20, &seed);
		count = two_solve(&g_solver, &cube, out);
		bad += count < 0 || count > TWOBYTWO_MAX_MOVES;
		after = cube;
		if (count > 0)
			cube_apply_moves(&after, out, (size_t)count);
		bad += !two_cube_is_solved(&after);
	}
	CHECK_EQ(bad, 0);
}

/// A cube whose corners are already solved but whose edges are not is
/// still solved as far as two_solve() is concerned -- a 2x2x2 has no
/// edges to be out of place.
static void	test_solved_corners_scrambled_edges(void)
{
	t_cube	cube;
	t_move	out[TWOBYTWO_MAX_MOVES];

	cube = SOLVED_CUBE;
	cube.edge_perm[0] = EDGE_UF;
	cube.edge_perm[1] = EDGE_UR;
	CHECK_EQ(two_solve(&g_solver, &cube, out), 0);
}

/// Two cubes with the SAME corners but different edges get the exact
/// same solution: edge_perm and edge_orient are never read.
static void	test_edges_are_ignored(void)
{
	t_cube	a;
	t_cube	b;
	t_move	out_a[TWOBYTWO_MAX_MOVES];
	t_move	out_b[TWOBYTWO_MAX_MOVES];
	int		len;

	a = SOLVED_CUBE;
	apply_move(&a, MOVE_R1);
	apply_move(&a, MOVE_U1);
	apply_move(&a, MOVE_F2);
	b = a;
	b.edge_perm[0] = EDGE_UF;
	b.edge_perm[1] = EDGE_UR;
	b.edge_orient[3] = 1;
	len = two_solve(&g_solver, &a, out_a);
	CHECK(len > 0);
	CHECK_EQ(two_solve(&g_solver, &b, out_b), len);
	CHECK(memcmp(out_a, out_b, (size_t)len * sizeof(t_move)) == 0);
}

/// A corner-orientation invariant violation is refused with -1: a 2x2x2
/// has no edges to check parity against, but the twist-sum invariant
/// (cube.h) is still physical -- it is what makes a state reachable at
/// all.
static void	test_invalid_corners_refused(void)
{
	t_cube	cube;
	t_move	out[TWOBYTWO_MAX_MOVES];

	cube = SOLVED_CUBE;
	cube.corner_orient[0] = 1;
	CHECK_EQ(two_solve(&g_solver, &cube, out), -1);
}

/// Building the solver again works, and free is safe to repeat.
static void	test_init_free_cycle(void)
{
	t_two_solver	second;
	t_move			out[TWOBYTWO_MAX_MOVES];
	t_cube			cube;
	uint32_t		seed;

	seed = 9;
	CHECK(two_solver_init(&second));
	scramble(&cube, 15, &seed);
	CHECK(two_solve(&second, &cube, out) >= 0);
	two_solver_free(&second);
	two_solver_free(&second);
}

int	main(void)
{
	if (!two_solver_init(&g_solver))
	{
		fprintf(stderr, "  could not build the 2x2x2 solver\n");
		return (1);
	}
	test_already_solved();
	test_random_scrambles();
	test_solved_corners_scrambled_edges();
	test_edges_are_ignored();
	test_invalid_corners_refused();
	test_init_free_cycle();
	two_solver_free(&g_solver);
	return (test_report("two_by_two"));
}
