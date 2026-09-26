#include "rubik.h"
#include "testlib.h"

/// Tests for the cube model (cube/cubie.c): SOLVED_CUBE, cube_equal,
/// cube_is_solved, the twist/flip sums and cube_is_valid.

static void	test_solved_cube(void)
{
	int	i;

	i = 0;
	while (i < CORNER_COUNT)
	{
		CHECK_EQ(SOLVED_CUBE.corner_perm[i], i);
		CHECK_EQ(SOLVED_CUBE.corner_orient[i], 0);
		i++;
	}
	i = 0;
	while (i < EDGE_COUNT)
	{
		CHECK_EQ(SOLVED_CUBE.edge_perm[i], i);
		CHECK_EQ(SOLVED_CUBE.edge_orient[i], 0);
		i++;
	}
	CHECK(cube_is_solved(&SOLVED_CUBE));
	CHECK(cube_is_valid(&SOLVED_CUBE));
	CHECK_EQ(cube_corner_twist_sum(&SOLVED_CUBE), 0);
	CHECK_EQ(cube_edge_flip_sum(&SOLVED_CUBE), 0);
}

/// Changing any single field must be noticed by cube_equal / is_solved.
static void	test_equal_notices_every_field(void)
{
	t_cube	cube;
	t_cube	copy;
	int		i;

	cube = SOLVED_CUBE;
	CHECK(cube_equal(&cube, &SOLVED_CUBE));
	i = 0;
	while (i < CORNER_COUNT)
	{
		copy = SOLVED_CUBE;
		copy.corner_orient[i] = 1;
		CHECK(!cube_equal(&copy, &SOLVED_CUBE));
		CHECK(!cube_is_solved(&copy));
		copy = SOLVED_CUBE;
		copy.corner_perm[i] = (t_corner)((i + 1) % CORNER_COUNT);
		CHECK(!cube_equal(&copy, &SOLVED_CUBE));
		i++;
	}
	i = 0;
	while (i < EDGE_COUNT)
	{
		copy = SOLVED_CUBE;
		copy.edge_orient[i] = 1;
		CHECK(!cube_equal(&copy, &SOLVED_CUBE));
		copy = SOLVED_CUBE;
		copy.edge_perm[i] = (t_edge)((i + 1) % EDGE_COUNT);
		CHECK(!cube_equal(&copy, &SOLVED_CUBE));
		i++;
	}
}

static void	test_twist_rule(void)
{
	t_cube	cube;
	int		i;
	int		j;

	i = 0;
	while (i < CORNER_COUNT)
	{
		cube = SOLVED_CUBE;
		cube.corner_orient[i] = 1;
		CHECK_EQ(cube_corner_twist_sum(&cube), 1);
		CHECK(!cube_is_valid(&cube));
		cube.corner_orient[i] = 2;
		CHECK_EQ(cube_corner_twist_sum(&cube), 2);
		CHECK(!cube_is_valid(&cube));
		j = (i + 1) % CORNER_COUNT;
		cube = SOLVED_CUBE;
		cube.corner_orient[i] = 1;
		cube.corner_orient[j] = 2;
		CHECK(cube_is_valid(&cube));
		i++;
	}
	cube = SOLVED_CUBE;
	cube.corner_orient[0] = 1;
	cube.corner_orient[1] = 1;
	cube.corner_orient[2] = 1;
	CHECK_EQ(cube_corner_twist_sum(&cube), 0);
	CHECK(cube_is_valid(&cube));
}

static void	test_flip_rule(void)
{
	t_cube	cube;
	int		i;

	i = 0;
	while (i < EDGE_COUNT)
	{
		cube = SOLVED_CUBE;
		cube.edge_orient[i] = 1;
		CHECK_EQ(cube_edge_flip_sum(&cube), 1);
		CHECK(!cube_is_valid(&cube));
		cube.edge_orient[(i + 1) % EDGE_COUNT] = 1;
		CHECK_EQ(cube_edge_flip_sum(&cube), 0);
		CHECK(cube_is_valid(&cube));
		i++;
	}
}

static void	swap_corners(t_cube *cube, int a, int b)
{
	t_corner	tmp;

	tmp = cube->corner_perm[a];
	cube->corner_perm[a] = cube->corner_perm[b];
	cube->corner_perm[b] = tmp;
}

static void	swap_edges(t_cube *cube, int a, int b)
{
	t_edge	tmp;

	tmp = cube->edge_perm[a];
	cube->edge_perm[a] = cube->edge_perm[b];
	cube->edge_perm[b] = tmp;
}

/// Permutation parity: swapping two corners OR two edges alone is
/// impossible on a real cube, swapping one pair of each is fine.
static void	test_parity_rule(void)
{
	t_cube	cube;
	int		a;
	int		b;
	int		c;
	int		d;

	a = 0;
	while (a < CORNER_COUNT)
	{
		b = a + 1;
		while (b < CORNER_COUNT)
		{
			cube = SOLVED_CUBE;
			swap_corners(&cube, a, b);
			CHECK(!cube_is_valid(&cube));
			b++;
		}
		a++;
	}
	a = 0;
	while (a < EDGE_COUNT)
	{
		b = a + 1;
		while (b < EDGE_COUNT)
		{
			cube = SOLVED_CUBE;
			swap_edges(&cube, a, b);
			CHECK(!cube_is_valid(&cube));
			b++;
		}
		a++;
	}
	c = 0;
	while (c < CORNER_COUNT - 1)
	{
		d = 0;
		while (d < EDGE_COUNT - 1)
		{
			cube = SOLVED_CUBE;
			swap_corners(&cube, c, c + 1);
			swap_edges(&cube, d, d + 1);
			CHECK(cube_is_valid(&cube));
			d++;
		}
		c++;
	}
}

/// A 3-cycle is an even permutation, so it is a legal cube by itself.
static void	test_three_cycle_is_valid(void)
{
	t_cube	cube;

	cube = SOLVED_CUBE;
	swap_corners(&cube, 0, 1);
	swap_corners(&cube, 1, 2);
	CHECK(cube_is_valid(&cube));
	cube = SOLVED_CUBE;
	swap_edges(&cube, 0, 1);
	swap_edges(&cube, 1, 2);
	CHECK(cube_is_valid(&cube));
}

int	main(void)
{
	test_solved_cube();
	test_equal_notices_every_field();
	test_twist_rule();
	test_flip_rule();
	test_parity_rule();
	test_three_cycle_is_valid();
	return (test_report("cubie"));
}
