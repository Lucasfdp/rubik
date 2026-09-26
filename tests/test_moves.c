#include "rubik.h"
#include "testlib.h"

/// Tests for the 18 moves (cube/moves.c): apply_move(), cube_apply_moves().
/// Links only cubie.c + moves.c, never the parser or main.

# define LEN(a) (sizeof(a) / sizeof((a)[0]))

/// Face indices, same order as t_move / 3: U R F D L B.
enum {FACE_U, FACE_R, FACE_F, FACE_D, FACE_L, FACE_B, FACE_COUNT};

/// The inverse of a move: same face, turn 0 <-> 2, 180 stays 180.
static t_move	inverse_of(t_move move)
{
	return ((t_move)((move / 3) * 3 + (2 - move % 3)));
}

/// True if corner_perm and edge_perm each contain every piece exactly once.
static bool	is_real_permutation(const t_cube *cube)
{
	int	corners[CORNER_COUNT];
	int	edges[EDGE_COUNT];
	int	i;

	memset(corners, 0, sizeof(corners));
	memset(edges, 0, sizeof(edges));
	i = 0;
	while (i < CORNER_COUNT)
		corners[cube->corner_perm[i++]]++;
	i = 0;
	while (i < EDGE_COUNT)
		edges[cube->edge_perm[i++]]++;
	i = 0;
	while (i < CORNER_COUNT)
		if (corners[i++] != 1)
			return (false);
	i = 0;
	while (i < EDGE_COUNT)
		if (edges[i++] != 1)
			return (false);
	return (true);
}

/// How many times a sequence must be repeated to get back to solved
/// (its "order"), or 0 if it takes more than limit repeats.
static size_t	order_of(const t_move *seq, size_t len, size_t limit)
{
	t_cube	cube;
	size_t	n;

	cube = SOLVED_CUBE;
	n = 1;
	while (n <= limit)
	{
		cube_apply_moves(&cube, seq, len);
		if (cube_is_solved(&cube))
			return (n);
		n++;
	}
	return (0);
}

static void	test_every_move_alone(void)
{
	t_cube	cube;
	int		move;

	move = 0;
	while (move < MOVE_COUNT)
	{
		cube = SOLVED_CUBE;
		apply_move(&cube, (t_move)move);
		CHECK(!cube_is_solved(&cube));
		CHECK(is_real_permutation(&cube));
		CHECK(cube_is_valid(&cube));
		CHECK_EQ(cube_corner_twist_sum(&cube), 0);
		CHECK_EQ(cube_edge_flip_sum(&cube), 0);
		move++;
	}
}

static void	test_turn_identities(void)
{
	t_cube	cube;
	int		face;
	int		i;

	face = 0;
	while (face < FACE_COUNT)
	{
		cube = SOLVED_CUBE;
		i = 0;
		while (i++ < 4)
			apply_move(&cube, (t_move)(face * 3));
		CHECK(cube_is_solved(&cube));
		cube = SOLVED_CUBE;
		i = 0;
		while (i++ < 4)
			apply_move(&cube, (t_move)(face * 3 + 2));
		CHECK(cube_is_solved(&cube));
		cube = SOLVED_CUBE;
		apply_move(&cube, (t_move)(face * 3 + 1));
		apply_move(&cube, (t_move)(face * 3 + 1));
		CHECK(cube_is_solved(&cube));
		cube = SOLVED_CUBE;
		apply_move(&cube, (t_move)(face * 3));
		apply_move(&cube, (t_move)(face * 3));
		apply_move(&cube, (t_move)(face * 3 + 1));
		CHECK(cube_is_solved(&cube));
		apply_move(&cube, (t_move)(face * 3 + 1));
		CHECK(!cube_is_solved(&cube));
		face++;
	}
}

/// Every move followed by its inverse is a no-op, and R2 == R R == R' R'.
static void	test_inverses(void)
{
	t_cube	cube;
	t_cube	twice;
	int		move;

	move = 0;
	while (move < MOVE_COUNT)
	{
		cube = SOLVED_CUBE;
		apply_move(&cube, (t_move)move);
		apply_move(&cube, inverse_of((t_move)move));
		CHECK(cube_is_solved(&cube));
		move++;
	}
	move = 0;
	while (move < MOVE_COUNT)
	{
		cube = SOLVED_CUBE;
		twice = SOLVED_CUBE;
		apply_move(&cube, (t_move)(move + 1));
		apply_move(&twice, (t_move)move);
		apply_move(&twice, (t_move)move);
		CHECK(cube_equal(&cube, &twice));
		cube = SOLVED_CUBE;
		twice = SOLVED_CUBE;
		apply_move(&cube, (t_move)(move + 2));
		apply_move(&twice, (t_move)move);
		apply_move(&twice, (t_move)move);
		apply_move(&twice, (t_move)move);
		CHECK(cube_equal(&cube, &twice));
		move += 3;
	}
}

/// What a clockwise quarter turn changes in orientation, per face:
/// F and B flip the 4 edges they move, R L F B twist 4 corners, U and D
/// change no orientation at all (docs/en/02a-cube-notation.md).
static void	test_orientation_rules(void)
{
	t_cube	cube;
	int		face;
	int		i;
	int		flipped;
	int		twisted;

	face = 0;
	while (face < FACE_COUNT)
	{
		cube = SOLVED_CUBE;
		apply_move(&cube, (t_move)(face * 3));
		flipped = 0;
		twisted = 0;
		i = 0;
		while (i < EDGE_COUNT)
			flipped += cube.edge_orient[i++];
		i = 0;
		while (i < CORNER_COUNT)
			twisted += (cube.corner_orient[i++] != 0);
		CHECK_EQ(flipped, (face == FACE_F || face == FACE_B) ? 4 : 0);
		CHECK_EQ(twisted, (face == FACE_U || face == FACE_D) ? 0 : 4);
		face++;
	}
}

/// Hard-coded expectations for R and U, taken from Kociemba's reference
/// tables (docs/en/02a-cube-notation.md). Catches a wrong sign or a
/// wrong cycle direction that the identity tests could miss.
static void	test_known_r_and_u(void)
{
	static const uint8_t	r_twist[CORNER_COUNT] = {2, 0, 0, 1, 1, 0, 0, 2};
	t_cube					cube;
	int						i;

	cube = SOLVED_CUBE;
	apply_move(&cube, MOVE_R1);
	i = 0;
	while (i < CORNER_COUNT)
	{
		CHECK_EQ(cube.corner_orient[i], r_twist[i]);
		i++;
	}
	CHECK_EQ(cube.corner_perm[CORNER_UBR], CORNER_URF);
	CHECK_EQ(cube.corner_perm[CORNER_DRB], CORNER_UBR);
	CHECK_EQ(cube.corner_perm[CORNER_DFR], CORNER_DRB);
	CHECK_EQ(cube.corner_perm[CORNER_URF], CORNER_DFR);
	CHECK_EQ(cube.edge_perm[EDGE_BR], EDGE_UR);
	CHECK_EQ(cube.edge_perm[EDGE_DR], EDGE_BR);
	CHECK_EQ(cube.edge_perm[EDGE_FR], EDGE_DR);
	CHECK_EQ(cube.edge_perm[EDGE_UR], EDGE_FR);
	cube = SOLVED_CUBE;
	apply_move(&cube, MOVE_U1);
	CHECK_EQ(cube.corner_perm[CORNER_URF], CORNER_UBR);
	CHECK_EQ(cube.corner_perm[CORNER_UFL], CORNER_URF);
	CHECK_EQ(cube.corner_perm[CORNER_ULB], CORNER_UFL);
	CHECK_EQ(cube.corner_perm[CORNER_UBR], CORNER_ULB);
	CHECK_EQ(cube.edge_perm[EDGE_UR], EDGE_UB);
	CHECK_EQ(cube.edge_perm[EDGE_UF], EDGE_UR);
	CHECK_EQ(cube.edge_perm[EDGE_UL], EDGE_UF);
	CHECK_EQ(cube.edge_perm[EDGE_UB], EDGE_UL);
}

/// Opposite faces commute; neighbouring faces do not.
static void	test_commutation(void)
{
	static const t_move	ud[] = {MOVE_U1, MOVE_D1};
	static const t_move	du[] = {MOVE_D1, MOVE_U1};
	static const t_move	ru[] = {MOVE_R1, MOVE_U1};
	static const t_move	ur[] = {MOVE_U1, MOVE_R1};
	t_cube				a;
	t_cube				b;

	a = SOLVED_CUBE;
	b = SOLVED_CUBE;
	cube_apply_moves(&a, ud, LEN(ud));
	cube_apply_moves(&b, du, LEN(du));
	CHECK(cube_equal(&a, &b));
	a = SOLVED_CUBE;
	b = SOLVED_CUBE;
	cube_apply_moves(&a, ru, LEN(ru));
	cube_apply_moves(&b, ur, LEN(ur));
	CHECK(!cube_equal(&a, &b));
}

/// Well-known algorithm identities. If a table value is wrong, these
/// orders come out different.
static void	test_known_algorithms(void)
{
	static const t_move	sexy[] = {MOVE_R1, MOVE_U1, MOVE_R3, MOVE_U3};
	static const t_move	sune[] = {MOVE_R1, MOVE_U1, MOVE_R3, MOVE_U1,
		MOVE_R1, MOVE_U2, MOVE_R3};
	static const t_move	ru[] = {MOVE_R1, MOVE_U1};
	static const t_move	rup[] = {MOVE_R1, MOVE_U3};
	static const t_move	fr[] = {MOVE_F1, MOVE_R1};
	static const t_move	big[] = {MOVE_R1, MOVE_U2, MOVE_D3, MOVE_B1, MOVE_D3};

	CHECK_EQ(order_of(sexy, LEN(sexy), 100), 6);
	CHECK_EQ(order_of(sune, LEN(sune), 100), 6);
	CHECK_EQ(order_of(ru, LEN(ru), 200), 105);
	CHECK_EQ(order_of(rup, LEN(rup), 200), 63);
	CHECK_EQ(order_of(fr, LEN(fr), 200), 105);
	CHECK_EQ(order_of(big, LEN(big), 2000), 1260);
}

/// The superflip (20 moves, the famous hardest position): flips all 12
/// edges and puts every piece back in its own slot.
static void	test_superflip(void)
{
	static const t_move	seq[] = {MOVE_U1, MOVE_R2, MOVE_F1, MOVE_B1, MOVE_R1,
		MOVE_B2, MOVE_R1, MOVE_U2, MOVE_L1, MOVE_B2, MOVE_R1, MOVE_U3,
		MOVE_D3, MOVE_R2, MOVE_F1, MOVE_R3, MOVE_L1, MOVE_B2, MOVE_U2,
		MOVE_F2};
	t_cube				cube;
	int					i;

	cube = SOLVED_CUBE;
	cube_apply_moves(&cube, seq, LEN(seq));
	CHECK(cube_is_valid(&cube));
	i = 0;
	while (i < EDGE_COUNT)
	{
		CHECK_EQ(cube.edge_orient[i], 1);
		CHECK_EQ(cube.edge_perm[i], i);
		i++;
	}
	i = 0;
	while (i < CORNER_COUNT)
	{
		CHECK_EQ(cube.corner_orient[i], 0);
		CHECK_EQ(cube.corner_perm[i], i);
		i++;
	}
}

/// 20,000 random moves: every step stays a real, reachable cube, and
/// undoing them in reverse order with inverse moves returns to solved.
static void	test_random_walk(void)
{
	enum {STEPS = 20000};
	static t_move	history[STEPS];
	uint32_t		rng;
	t_cube			cube;
	int				i;

	rng = 12345;
	cube = SOLVED_CUBE;
	i = 0;
	while (i < STEPS)
	{
		history[i] = (t_move)(test_rand(&rng) % MOVE_COUNT);
		apply_move(&cube, history[i]);
		CHECK(is_real_permutation(&cube) && cube_is_valid(&cube));
		i++;
	}
	while (i-- > 0)
		apply_move(&cube, inverse_of(history[i]));
	CHECK(cube_is_solved(&cube));
}

static void	test_apply_moves_helper(void)
{
	static const t_move	seq[] = {MOVE_F1, MOVE_R3, MOVE_U2, MOVE_B1, MOVE_L3};
	t_cube				a;
	t_cube				b;
	size_t				i;

	a = SOLVED_CUBE;
	b = SOLVED_CUBE;
	cube_apply_moves(&a, seq, LEN(seq));
	i = 0;
	while (i < LEN(seq))
		apply_move(&b, seq[i++]);
	CHECK(cube_equal(&a, &b));
	a = SOLVED_CUBE;
	cube_apply_moves(&a, seq, 0);
	CHECK(cube_is_solved(&a));
	cube_apply_moves(&a, NULL, 0);
	CHECK(cube_is_solved(&a));
}

int	main(void)
{
	test_every_move_alone();
	test_turn_identities();
	test_inverses();
	test_orientation_rules();
	test_known_r_and_u();
	test_commutation();
	test_known_algorithms();
	test_superflip();
	test_random_walk();
	test_apply_moves_helper();
	return (test_report("moves"));
}
