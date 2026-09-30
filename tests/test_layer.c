#include "rubik.h"
#include "testlib.h"

/// Tests for the beginner (layer-by-layer) solver (solve/layer.c). Same
/// shape as tests/test_thistlethwaite.c's real-cube tier: solved-cube and
/// random-scramble checks against the real move tables and a real
/// apply_move(), since layer_solve() has no synthetic table-building
/// logic of its own to test in isolation the way Thistlethwaite's
/// reachable_set()/prune_build_coset() did.

/// Random cube built from `steps` random moves.
static void	scramble(t_cube *cube, int steps, uint32_t *rng)
{
	*cube = SOLVED_CUBE;
	while (steps > 0)
	{
		apply_move(cube, (t_move)(test_rand(rng) % MOVE_COUNT));
		steps--;
	}
}

static void	test_already_solved(void)
{
	t_move	out[LAYER_MAX_MOVES];

	CHECK_EQ(layer_solve(&SOLVED_CUBE, out), 0);
}

static void	test_invalid_cube_is_refused(void)
{
	t_cube	cube;
	t_move	out[LAYER_MAX_MOVES];

	cube = SOLVED_CUBE;
	cube.corner_orient[0] = 1;
	CHECK_EQ(layer_solve(&cube, out), -1);
}

/// End to end: every stage chained by layer_solve(), applied to a real
/// scrambled cube, must reach solved -- the same shape as tests/
/// test_solve.c's / test_thistlethwaite.c's random-scramble checks.
/// No move-count upper bound is asserted here: unlike Kociemba's ~23 or
/// Thistlethwaite's ~40-45, the beginner method's count is not tightly
/// bounded by design -- LAYER_MAX_MOVES itself is the only ceiling that
/// matters, and count <= that is already checked below.
static void	test_random_scrambles_solve(void)
{
	t_cube		cube;
	t_cube		after;
	t_move		out[LAYER_MAX_MOVES];
	uint32_t	seed;
	int			bad;
	int			count;
	int			longest;
	int			run;

	seed = 71;
	bad = 0;
	longest = 0;
	run = -1;
	while (++run < 100)
	{
		scramble(&cube, 25, &seed);
		count = layer_solve(&cube, out);
		bad += count < 0 || count > LAYER_MAX_MOVES;
		after = cube;
		if (count > 0)
			cube_apply_moves(&after, out, (size_t)count);
		bad += !cube_is_solved(&after);
		if (count > longest)
			longest = count;
	}
	CHECK_EQ(bad, 0);
	CHECK(longest <= LAYER_MAX_MOVES);
}

/// Every stage (cross, first-layer corners, second-layer edges) must
/// leave the previous ones intact even when a piece it needs is already
/// sitting in an awkward spot: its own target slot but flipped/twisted
/// the wrong way, a different D-slot, or (for the last two stages) a
/// slot two stages could confuse. Fixed scrambles, not random, so a
/// regression in one specific case fails the same way every run.
static void	test_layer_by_layer_fixed_scrambles(void)
{
	static const char	*cases[] = {
		"R U R' U' R' F R2 U' R' U' R U R' F'",
		"F2 U' L F2 L' R' U2 R U' F2",
		"R' L' D2 L U L D2 L' U R'",
		"D2 R' D2 R L' D2 L U2",
		"R U2 R2 F R F' U2 R' F R F'",
	};
	size_t	i;
	t_move	scrambled[MAX_MOVES];
	size_t	count;
	t_cube	cube;
	t_move	out[LAYER_MAX_MOVES];
	int		len;
	t_cube	after;

	i = 0;
	while (i < sizeof(cases) / sizeof(cases[0]))
	{
		parse_notation(cases[i], scrambled, &count);
		cube = SOLVED_CUBE;
		cube_apply_moves(&cube, scrambled, count);
		len = layer_solve(&cube, out);
		CHECK(len >= 0 && len <= LAYER_MAX_MOVES);
		after = cube;
		if (len > 0)
			cube_apply_moves(&after, out, (size_t)len);
		CHECK(cube_is_solved(&after));
		i++;
	}
}

int	main(void)
{
	test_already_solved();
	test_invalid_cube_is_refused();
	test_layer_by_layer_fixed_scrambles();
	test_random_scrambles_solve();
	return (test_report("layer"));
}
