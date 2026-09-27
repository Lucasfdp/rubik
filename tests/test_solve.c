#include "rubik.h"
#include "testlib.h"

/// Tests for solve() and format_moves(). Real cubes, turned with the real
/// apply_move(), are the referee: a solution is applied to the scramble
/// and the cube must end up solved.

static t_solver	g_solver;

/// Random cube made with `steps` random moves. Only the cube is returned:
/// solve() never sees the moves.
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
	t_move	out[SOLVE_MAX_MOVES];

	CHECK_EQ(solve(&g_solver, &SOLVED_CUBE, out), 0);
}

/// Random scrambles: the solution has 1..30 moves, and applying it to the
/// scramble gives a solved cube.
static void	test_random_scrambles(void)
{
	t_cube		cube;
	t_cube		after;
	t_move		out[SOLVE_MAX_MOVES];
	uint32_t	seed;
	int			bad;
	int			count;
	int			run;

	seed = 41;
	bad = 0;
	run = -1;
	while (++run < 200)
	{
		scramble(&cube, 30, &seed);
		count = solve(&g_solver, &cube, out);
		bad += count < 0 || count > SOLVE_MAX_MOVES;
		after = cube;
		if (count > 0)
			cube_apply_moves(&after, out, (size_t)count);
		bad += !cube_is_solved(&after) || (count == 0 && !cube_is_solved(&cube));
	}
	CHECK_EQ(bad, 0);
}

/// Known hard cases: the superflip (every edge flipped in place, 20 moves
/// at best) and a checkerboard-style pattern of half turns.
static void	test_hard_cases(void)
{
	static const char *const	cases[] = {
		"U R2 F B R B2 R U2 L B2 R U' D' R2 F R' L B2 U2 F2",
		"U2 D2 R2 L2 F2 B2",
		"R L U2 R' L' F2 R L U2 R' L' F2 R L"};
	t_move						moves[MAX_MOVES];
	t_move						out[SOLVE_MAX_MOVES];
	size_t						count;
	t_cube						cube;
	int							i;
	int							length;

	i = 0;
	while (i < 3)
	{
		CHECK_EQ(parse_notation(cases[i], moves, &count), PARSE_OK);
		cube = SOLVED_CUBE;
		cube_apply_moves(&cube, moves, count);
		length = solve(&g_solver, &cube, out);
		CHECK(length >= 0);
		if (length >= 0)
			cube_apply_moves(&cube, out, (size_t)length);
		CHECK(cube_is_solved(&cube));
		i++;
	}
}

/// A cube that cannot exist is refused with -1 instead of searched.
static void	test_invalid_cube_is_refused(void)
{
	t_cube	cube;
	t_move	out[SOLVE_MAX_MOVES];

	cube = SOLVED_CUBE;
	cube.corner_orient[0] = 1;
	CHECK_EQ(solve(&g_solver, &cube, out), -1);
	cube = SOLVED_CUBE;
	cube.edge_orient[0] = 1;
	CHECK_EQ(solve(&g_solver, &cube, out), -1);
	cube = SOLVED_CUBE;
	cube.edge_perm[0] = EDGE_UF;
	cube.edge_perm[1] = EDGE_UR;
	CHECK_EQ(solve(&g_solver, &cube, out), -1);
}

/// The answer depends only on the cube: two identical cubes built from
/// different scrambles give the same solution.
static void	test_same_cube_same_answer(void)
{
	t_move	a[SOLVE_MAX_MOVES];
	t_move	b[SOLVE_MAX_MOVES];
	t_move	first[3];
	t_move	second[5];
	t_cube	one;
	t_cube	two;
	int		length;

	first[0] = MOVE_R1;
	first[1] = MOVE_U1;
	first[2] = MOVE_R3;
	second[0] = MOVE_R1;
	second[1] = MOVE_U1;
	second[2] = MOVE_F1;
	second[3] = MOVE_F3;
	second[4] = MOVE_R3;
	one = SOLVED_CUBE;
	cube_apply_moves(&one, first, 3);
	two = SOLVED_CUBE;
	cube_apply_moves(&two, second, 5);
	CHECK(cube_equal(&one, &two));
	length = solve(&g_solver, &one, a);
	CHECK_EQ(solve(&g_solver, &two, b), length);
	CHECK(memcmp(a, b, (size_t)length * sizeof(t_move)) == 0);
}

/// Exact text, and the round trip through parse_notation().
static void	test_format_moves(void)
{
	static const t_move	list[] = {MOVE_R1, MOVE_U2, MOVE_F3, MOVE_D1, MOVE_L2,
		MOVE_B3};
	t_move				back[MAX_MOVES];
	char				text[64];
	size_t				count;
	size_t				len;

	len = format_moves(list, 6, text);
	CHECK(strcmp(text, "R U2 F' D L2 B'") == 0);
	CHECK_EQ(len, strlen(text));
	CHECK_EQ(parse_notation(text, back, &count), PARSE_OK);
	CHECK_EQ(count, 6);
	CHECK(memcmp(back, list, sizeof(list)) == 0);
	CHECK_EQ(format_moves(list, 0, text), 0);
	CHECK_EQ(text[0], '\0');
	CHECK_EQ(format_moves(list, 1, text), 1);
	CHECK(strcmp(text, "R") == 0);
}

/// Every one of the 18 moves survives format -> parse.
static void	test_format_every_move(void)
{
	t_move	all[MOVE_COUNT];
	t_move	back[MAX_MOVES];
	char	text[MOVE_COUNT * 3 + 1];
	size_t	count;
	int		i;

	i = 0;
	while (i < MOVE_COUNT)
	{
		all[i] = (t_move)i;
		i++;
	}
	format_moves(all, MOVE_COUNT, text);
	CHECK_EQ(parse_notation(text, back, &count), PARSE_OK);
	CHECK_EQ(count, MOVE_COUNT);
	CHECK(memcmp(back, all, sizeof(all)) == 0);
}

/// Exact merges: same face combines, opposite faces commute, and a merge
/// that removes a pair can let its neighbours merge too.
static void	test_simplify_known(void)
{
	t_move	a[] = {MOVE_R1, MOVE_R2};
	t_move	b[] = {MOVE_R1, MOVE_R3};
	t_move	c[] = {MOVE_U1, MOVE_D1, MOVE_U3};
	t_move	d[] = {MOVE_R1, MOVE_U1, MOVE_U3, MOVE_R1};
	t_move	e[] = {MOVE_F1, MOVE_B2, MOVE_L1, MOVE_F1};

	CHECK_EQ(simplify_moves(a, 2), 1);
	CHECK_EQ(a[0], MOVE_R3);
	CHECK_EQ(simplify_moves(b, 2), 0);
	CHECK_EQ(simplify_moves(c, 3), 1);
	CHECK_EQ(c[0], MOVE_D1);
	CHECK_EQ(simplify_moves(d, 4), 1);
	CHECK_EQ(d[0], MOVE_R2);
	CHECK_EQ(simplify_moves(e, 4), 4);
	CHECK_EQ(simplify_moves(e, 0), 0);
}

/// Random move lists (short, so merges are common): the simplified list has
/// the same effect on a cube, is never longer, has no two moves on the same
/// face side by side, and simplifying again changes nothing.
static void	test_simplify_random(void)
{
	t_move		list[24];
	t_move		copy[24];
	t_cube		one;
	t_cube		two;
	uint32_t	seed;
	int			bad;
	int			count;
	int			size;
	int			i;

	seed = 77;
	bad = 0;
	count = -1;
	while (++count < 5000)
	{
		size = (int)(test_rand(&seed) % 24);
		i = -1;
		while (++i < size)
			list[i] = (t_move)(test_rand(&seed) % 6 * 3 + test_rand(&seed) % 3);
		memcpy(copy, list, sizeof(list));
		one = SOLVED_CUBE;
		cube_apply_moves(&one, list, (size_t)size);
		i = simplify_moves(list, size);
		two = SOLVED_CUBE;
		cube_apply_moves(&two, list, (size_t)i);
		bad += !cube_equal(&one, &two) || i > size
			|| simplify_moves(list, i) != i;
		while (--i > 0)
			bad += list[i] / 3 == list[i - 1] / 3;
	}
	CHECK_EQ(bad, 0);
}

/// Building the solver again works, and free is safe to repeat.
static void	test_init_free_cycle(void)
{
	t_solver	second;
	t_move		out[SOLVE_MAX_MOVES];
	t_cube		cube;
	uint32_t	seed;

	seed = 5;
	CHECK(solver_init(&second));
	scramble(&cube, 30, &seed);
	CHECK(solve(&second, &cube, out) >= 0);
	solver_free(&second);
	solver_free(&second);
}

int	main(void)
{
	if (!solver_init(&g_solver))
	{
		fprintf(stderr, "  could not build the solver\n");
		return (1);
	}
	test_already_solved();
	test_random_scrambles();
	test_hard_cases();
	test_invalid_cube_is_refused();
	test_same_cube_same_answer();
	test_format_moves();
	test_format_every_move();
	test_simplify_known();
	test_simplify_random();
	test_init_free_cycle();
	solver_free(&g_solver);
	return (test_report("solve"));
}
