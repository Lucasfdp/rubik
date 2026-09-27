#include "rubik.h"
#include "testlib.h"

/// Tests for the move tables (coord/movetable.c). Links the coord files +
/// cubie.c + moves.c: real cubes, turned with the real apply_move(), are
/// the referee for every table entry.

typedef uint16_t	(*t_encode_fn_ptr)(const t_cube *);

static t_move_tables	g_tables;

/// The phase 2 rule, written out on its own (NOT from MOVES_PHASE2):
/// faces are ordered U R F D L B, move / 3 is the face, move % 3 == 1 is
/// the 180. U and D may turn any way, the other four only by 180.
static bool	rule_phase2(int move)
{
	int	face;

	face = move / 3;
	return (face == 0 || face == 3 || move % 3 == 1);
}

/// Number of moves in a set.
static int	mask_size(t_move_mask mask)
{
	int	move;
	int	size;

	size = 0;
	move = 0;
	while (move < MOVE_COUNT)
	{
		if (move_in_mask(mask, (t_move)move))
			size++;
		move++;
	}
	return (size);
}

/// MOVES_ALL has 18 moves. MOVES_PHASE2 has 10 and agrees with the rule.
static void	test_masks(void)
{
	int	move;

	CHECK_EQ(mask_size(MOVES_ALL), 18);
	CHECK_EQ(mask_size(MOVES_PHASE2), 10);
	move = 0;
	while (move < MOVE_COUNT)
	{
		CHECK(move_in_mask(MOVES_ALL, (t_move)move));
		CHECK_EQ(move_in_mask(MOVES_PHASE2, (t_move)move), rule_phase2(move));
		move++;
	}
}

/// Values worked out by hand in the walkthrough: corner twists
/// [1,0,2,0,0,0,0] are 1*729 + 2*81 = 891. A U turn moves the piece in
/// slot i to slot i+1 (URF -> UFL -> ULB -> UBR -> URF) and twists
/// nothing, so the digits of slots 0..3 rotate one place:
///   U  : [0,1,0,2] = 1*243 + 2*27 = 297
///   U2 : [2,0,1,0] = 2*729 + 1*81 = 1539
///   U' : [0,2,0,1] = 2*243 + 1*27 = 513
static void	test_known_values(void)
{
	t_cube	cube;

	CHECK_EQ(move_table_next(g_tables.twist, 891, MOVE_U1), 297);
	CHECK_EQ(move_table_next(g_tables.twist, 891, MOVE_U2), 1539);
	CHECK_EQ(move_table_next(g_tables.twist, 891, MOVE_U3), 513);
	cube = SOLVED_CUBE;
	cube.corner_orient[0] = 1;
	cube.corner_orient[2] = 2;
	CHECK_EQ(encode_twist(&cube), 891);
	apply_move(&cube, MOVE_U1);
	CHECK_EQ(encode_twist(&cube), 297);
}

/// From the solved cube, the moves whose row-0 answers are all 0 (twist,
/// flip AND slice unchanged) must be exactly the 10 phase 2 moves. This
/// checks MOVES_PHASE2 against the tables instead of against itself.
static void	test_phase2_keeps_phase1_goal(void)
{
	int	move;
	int	unchanged;

	move = 0;
	while (move < MOVE_COUNT)
	{
		unchanged = (g_tables.twist[move] | g_tables.flip[move]
				| g_tables.slice[move]) == 0;
		CHECK_EQ(unchanged, move_in_mask(MOVES_PHASE2, (t_move)move));
		move++;
	}
}

/// How many cells of one table hold MT_ILLEGAL in one column.
static int	column_illegal(const uint16_t *table, int count, int move)
{
	int	state;
	int	found;

	found = 0;
	state = 0;
	while (state < count)
	{
		if (table[state * MOVE_COUNT + move] == MT_ILLEGAL)
			found++;
		state++;
	}
	return (found);
}

/// twist / flip / slice / cperm never hold the sentinel. eperm / sperm
/// hold it in every row of the 8 illegal columns, and nowhere else.
static void	test_sentinels(void)
{
	int	move;
	int	legal;

	move = 0;
	while (move < MOVE_COUNT)
	{
		legal = move_in_mask(MOVES_PHASE2, (t_move)move);
		CHECK_EQ(column_illegal(g_tables.twist, TWIST_COUNT, move), 0);
		CHECK_EQ(column_illegal(g_tables.flip, FLIP_COUNT, move), 0);
		CHECK_EQ(column_illegal(g_tables.slice, SLICE_COUNT, move), 0);
		CHECK_EQ(column_illegal(g_tables.cperm, CPERM_COUNT, move), 0);
		CHECK_EQ(column_illegal(g_tables.eperm, EPERM_COUNT, move),
			legal ? 0 : EPERM_COUNT);
		CHECK_EQ(column_illegal(g_tables.sperm, SPERM_COUNT, move),
			legal ? 0 : SPERM_COUNT);
		move++;
	}
}

/// One face, one state: do the table columns of this face agree with each
/// other? Quarter twice is the 180, three times is the counter-clockwise,
/// four times is back at the start. If only the 180 is allowed for this
/// face, the 180 done twice must give back the start.
static bool	face_is_consistent(const uint16_t *table, uint16_t state,
	int face, t_move_mask allowed)
{
	t_move		q;
	uint16_t	a;
	uint16_t	b;
	uint16_t	c;

	q = (t_move)(face * 3);
	if (!move_in_mask(allowed, q))
	{
		a = move_table_next(table, state, (t_move)(q + 1));
		return (move_table_next(table, a, (t_move)(q + 1)) == state);
	}
	a = move_table_next(table, state, q);
	b = move_table_next(table, a, q);
	c = move_table_next(table, b, q);
	return (b == move_table_next(table, state, (t_move)(q + 1))
		&& c == move_table_next(table, state, (t_move)(q + 2))
		&& move_table_next(table, c, q) == state);
}

/// Checks every state and every face.
///
/// @return The first state that breaks a rule, or -1 if none does.
static int	first_law_break(const uint16_t *table, int count,
	t_move_mask allowed)
{
	int	state;
	int	face;

	state = 0;
	while (state < count)
	{
		face = 0;
		while (face < 6)
		{
			if (!face_is_consistent(table, (uint16_t)state, face, allowed))
				return (state);
			face++;
		}
		state++;
	}
	return (-1);
}

static void	test_face_laws(void)
{
	CHECK_EQ(first_law_break(g_tables.twist, TWIST_COUNT, MOVES_ALL), -1);
	CHECK_EQ(first_law_break(g_tables.flip, FLIP_COUNT, MOVES_ALL), -1);
	CHECK_EQ(first_law_break(g_tables.slice, SLICE_COUNT, MOVES_ALL), -1);
	CHECK_EQ(first_law_break(g_tables.cperm, CPERM_COUNT, MOVES_ALL), -1);
	CHECK_EQ(first_law_break(g_tables.eperm, EPERM_COUNT, MOVES_PHASE2), -1);
	CHECK_EQ(first_law_break(g_tables.sperm, SPERM_COUNT, MOVES_PHASE2), -1);
}

/// Turns the cube by one random move out of `allowed`. Walking one move at
/// a time gives a long chain of real, reachable states cheaply (with
/// MOVES_PHASE2 they are all states phase 1 has already solved).
static void	random_step(t_cube *cube, t_move_mask allowed, uint32_t *rng)
{
	t_move	move;

	move = (t_move)(test_rand(rng) % MOVE_COUNT);
	while (!move_in_mask(allowed, move))
		move = (t_move)(test_rand(rng) % MOVE_COUNT);
	apply_move(cube, move);
}

/// Random walk: at every state, for every allowed move, the table entry
/// for the cube's coordinate must equal the coordinate of the cube after
/// really turning it (roadmap sprint 1 gate:
/// movetable[encode(c)][m] == encode(apply(c,m))). The walk uses only
/// `allowed` moves, so eperm / sperm see only states where they mean
/// something.
///
/// @return How many (state, move) pairs disagree.
static int	count_mismatches(const uint16_t *table, t_encode_fn_ptr encode,
	t_move_mask allowed, uint32_t seed)
{
	t_cube		cube;
	t_cube		turned;
	int			bad;
	int			i;
	int			move;

	bad = 0;
	cube = SOLVED_CUBE;
	i = 0;
	while (i < 50000)
	{
		random_step(&cube, allowed, &seed);
		move = 0;
		while (move < MOVE_COUNT)
		{
			turned = cube;
			if (move_in_mask(allowed, (t_move)move))
			{
				apply_move(&turned, (t_move)move);
				bad += move_table_next(table, encode(&cube), (t_move)move)
					!= encode(&turned);
			}
			move++;
		}
		i++;
	}
	return (bad);
}

static void	test_random_cubes(void)
{
	CHECK_EQ(count_mismatches(g_tables.twist, encode_twist, MOVES_ALL, 1), 0);
	CHECK_EQ(count_mismatches(g_tables.flip, encode_flip, MOVES_ALL, 2), 0);
	CHECK_EQ(count_mismatches(g_tables.slice, encode_slice, MOVES_ALL, 3), 0);
	CHECK_EQ(count_mismatches(g_tables.cperm, encode_cperm, MOVES_ALL, 4), 0);
	CHECK_EQ(count_mismatches(g_tables.eperm, encode_eperm, MOVES_PHASE2, 5),
		0);
	CHECK_EQ(count_mismatches(g_tables.sperm, encode_sperm, MOVES_PHASE2, 6),
		0);
}

/// Building again works, and free is safe to repeat.
static void	test_build_free_cycle(void)
{
	t_move_tables	second;

	CHECK(move_tables_build(&second));
	CHECK(second.eperm != NULL);
	CHECK_EQ(move_table_next(second.twist, 891, MOVE_U1), 297);
	move_tables_free(&second);
	CHECK(second.twist == NULL && second.sperm == NULL);
	move_tables_free(&second);
}

int	main(void)
{
	if (!move_tables_build(&g_tables))
	{
		fprintf(stderr, "  could not build the move tables\n");
		return (1);
	}
	test_masks();
	test_known_values();
	test_phase2_keeps_phase1_goal();
	test_sentinels();
	test_face_laws();
	test_random_cubes();
	test_build_free_cycle();
	move_tables_free(&g_tables);
	return (test_report("movetable"));
}
