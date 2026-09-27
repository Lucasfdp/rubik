#include "rubik.h"
#include "testlib.h"

/// Tests for the coordinates (coord/encode.c, decode.c, tables.c): twist,
/// flip, slice, cperm, eperm and sperm. Links only the coord files + cubie.c
/// + moves.c (moves are used to make real, reachable cubes to test on).

/// Place value of each free slot: slot 0 is the biggest place.
static const int	g_twist_weight[CORNER_COUNT - 1] = {729, 243, 81, 27, 9, 3, 1};
static const int	g_flip_weight[EDGE_COUNT - 1] = {1024, 512, 256, 128, 64, 32,
	16, 8, 4, 2, 1};

/// Solved cube gives 0, and the ranges are what the docs say.
static void	test_solved_and_sizes(void)
{
	CHECK_EQ(TWIST_COUNT, 2187);
	CHECK_EQ(FLIP_COUNT, 2048);
	CHECK_EQ(encode_twist(&SOLVED_CUBE), 0);
	CHECK_EQ(encode_flip(&SOLVED_CUBE), 0);
}

/// One twist/flip at a time: the value must be that slot's place value.
/// Written out by hand, so it checks the encoder against the docs and not
/// against itself.
static void	test_place_values(void)
{
	t_cube	cube;
	int		i;

	i = 0;
	while (i < CORNER_COUNT - 1)
	{
		cube = SOLVED_CUBE;
		cube.corner_orient[i] = 1;
		CHECK_EQ(encode_twist(&cube), g_twist_weight[i]);
		cube.corner_orient[i] = 2;
		CHECK_EQ(encode_twist(&cube), 2 * g_twist_weight[i]);
		i++;
	}
	i = 0;
	while (i < EDGE_COUNT - 1)
	{
		cube = SOLVED_CUBE;
		cube.edge_orient[i] = 1;
		CHECK_EQ(encode_flip(&cube), g_flip_weight[i]);
		i++;
	}
	cube = SOLVED_CUBE;
	cube.corner_orient[CORNER_COUNT - 1] = 2;
	CHECK_EQ(encode_twist(&cube), 0);
	cube = SOLVED_CUBE;
	cube.edge_orient[EDGE_COUNT - 1] = 1;
	CHECK_EQ(encode_flip(&cube), 0);
}

/// Values worked out by hand in the walkthrough.
static void	test_known_values(void)
{
	t_cube	cube;
	int		i;

	cube = SOLVED_CUBE;
	apply_move(&cube, MOVE_R1);
	CHECK_EQ(encode_twist(&cube), 1494);
	CHECK_EQ(encode_flip(&cube), 0);
	cube = SOLVED_CUBE;
	apply_move(&cube, MOVE_F1);
	CHECK_EQ(encode_flip(&cube), 550);
	CHECK_EQ(encode_twist(&cube), 1236);
	cube = SOLVED_CUBE;
	i = 0;
	while (i < EDGE_COUNT)
		cube.edge_orient[i++] = 1;
	CHECK_EQ(encode_flip(&cube), 2047);
	cube = SOLVED_CUBE;
	i = 0;
	while (i < CORNER_COUNT - 1)
		cube.corner_orient[i++] = 2;
	cube.corner_orient[CORNER_COUNT - 1] = 1;
	CHECK_EQ(encode_twist(&cube), 2186);
}

/// Every possible value: decode it, check the cube is legal, encode it
/// back. Decode is then one-to-one, and since there are exactly 3^7 legal
/// twist patterns, it reaches every one of them (same for flip, 2^11).
static void	test_exhaustive_round_trip(void)
{
	t_cube	cube;
	int		n;

	n = 0;
	while (n < TWIST_COUNT)
	{
		cube = SOLVED_CUBE;
		decode_twist(&cube, (uint16_t)n);
		CHECK_EQ(cube_corner_twist_sum(&cube), 0);
		CHECK_EQ(encode_twist(&cube), n);
		n++;
	}
	n = 0;
	while (n < FLIP_COUNT)
	{
		cube = SOLVED_CUBE;
		decode_flip(&cube, (uint16_t)n);
		CHECK_EQ(cube_edge_flip_sum(&cube), 0);
		CHECK_EQ(encode_flip(&cube), n);
		n++;
	}
}

/// decode_twist must not touch anything except corner_orient, and
/// decode_flip nothing except edge_orient.
static void	test_decode_touches_only_its_property(void)
{
	t_cube	cube;
	t_cube	before;
	int		i;

	cube = SOLVED_CUBE;
	i = 0;
	while (i < 25)
		apply_move(&cube, (t_move)((i++ * 7) % MOVE_COUNT));
	before = cube;
	decode_twist(&cube, 1234);
	CHECK(memcmp(cube.corner_perm, before.corner_perm,
			sizeof(cube.corner_perm)) == 0);
	CHECK(memcmp(cube.edge_perm, before.edge_perm,
			sizeof(cube.edge_perm)) == 0);
	CHECK(memcmp(cube.edge_orient, before.edge_orient,
			sizeof(cube.edge_orient)) == 0);
	before = cube;
	decode_flip(&cube, 1234);
	CHECK(memcmp(cube.corner_perm, before.corner_perm,
			sizeof(cube.corner_perm)) == 0);
	CHECK(memcmp(cube.edge_perm, before.edge_perm,
			sizeof(cube.edge_perm)) == 0);
	CHECK(memcmp(cube.corner_orient, before.corner_orient,
			sizeof(cube.corner_orient)) == 0);
}

/// Real cubes: after every step of a long random walk, the values are in
/// range and decoding them rebuilds exactly the cube's own orientations
/// (including the forced last twist / flip).
static void	test_random_walk(void)
{
	enum {STEPS = 20000};
	uint32_t	rng;
	t_cube		cube;
	t_cube		copy;
	int			i;

	rng = 424242;
	cube = SOLVED_CUBE;
	i = 0;
	while (i < STEPS)
	{
		apply_move(&cube, (t_move)(test_rand(&rng) % MOVE_COUNT));
		CHECK(encode_twist(&cube) < TWIST_COUNT);
		CHECK(encode_flip(&cube) < FLIP_COUNT);
		copy = SOLVED_CUBE;
		decode_twist(&copy, encode_twist(&cube));
		decode_flip(&copy, encode_flip(&cube));
		CHECK(memcmp(copy.corner_orient, cube.corner_orient,
				sizeof(cube.corner_orient)) == 0);
		CHECK(memcmp(copy.edge_orient, cube.edge_orient,
				sizeof(cube.edge_orient)) == 0);
		i++;
	}
}

/// Two cubes with different twists must get different values, and cubes
/// differing only in permutation must get the SAME twist and flip.
static void	test_ignores_permutation(void)
{
	t_cube	a;
	t_cube	b;

	a = SOLVED_CUBE;
	b = SOLVED_CUBE;
	b.corner_perm[0] = CORNER_UFL;
	b.corner_perm[1] = CORNER_URF;
	b.edge_perm[0] = EDGE_UF;
	b.edge_perm[1] = EDGE_UR;
	CHECK_EQ(encode_twist(&a), encode_twist(&b));
	CHECK_EQ(encode_flip(&a), encode_flip(&b));
	b.corner_orient[2] = 1;
	b.corner_orient[3] = 2;
	CHECK(encode_twist(&a) != encode_twist(&b));
}


/// Bit i is set if edge slot i holds a middle-layer edge (piece 8..11).
static unsigned	slice_mask(const t_cube *cube)
{
	unsigned	mask;
	int			i;

	mask = 0;
	i = 0;
	while (i < EDGE_COUNT)
	{
		if (cube->edge_perm[i] >= EDGE_FR)
			mask |= 1u << i;
		i++;
	}
	return (mask);
}

static int	bit_count(unsigned mask)
{
	int	count;

	count = 0;
	while (mask)
	{
		count += (int)(mask & 1u);
		mask >>= 1;
	}
	return (count);
}

static bool	edges_are_a_permutation(const t_cube *cube)
{
	int	seen[EDGE_COUNT];
	int	i;

	i = 0;
	while (i < EDGE_COUNT)
		seen[i++] = 0;
	i = 0;
	while (i < EDGE_COUNT)
		seen[cube->edge_perm[i++]]++;
	i = 0;
	while (i < EDGE_COUNT)
		if (seen[i++] != 1)
			return (false);
	return (true);
}

/// Builds a cube whose middle-layer edges sit exactly in the slots of
/// mask. reverse flips the order the pieces are dropped in (both for the
/// middle-layer pieces and the others), to prove the order is ignored.
static t_cube	cube_from_mask(unsigned mask, bool reverse)
{
	t_cube	cube;
	int		slice_piece;
	int		other_piece;
	int		i;

	cube = SOLVED_CUBE;
	slice_piece = reverse ? EDGE_BR : EDGE_FR;
	other_piece = reverse ? EDGE_DB : EDGE_UR;
	i = 0;
	while (i < EDGE_COUNT)
	{
		if (mask & (1u << i))
		{
			cube.edge_perm[i] = (t_edge)slice_piece;
			slice_piece += reverse ? -1 : 1;
		}
		else
		{
			cube.edge_perm[i] = (t_edge)other_piece;
			other_piece += reverse ? -1 : 1;
		}
		i++;
	}
	return (cube);
}

static void	test_slice_hand_values(void)
{
	t_cube	cube;

	CHECK_EQ(SLICE_COUNT, 495);
	CHECK_EQ(encode_slice(&SOLVED_CUBE), 0);
	cube = cube_from_mask(0xF00, false);
	CHECK_EQ(encode_slice(&cube), 0);
	cube = cube_from_mask((1u << 7) | 0xE00, false);
	CHECK_EQ(encode_slice(&cube), 1);
	cube = cube_from_mask((1u << 6) | 0xE00, false);
	CHECK_EQ(encode_slice(&cube), 5);
	cube = cube_from_mask(0x00F, false);
	CHECK_EQ(encode_slice(&cube), 494);
	cube = cube_from_mask((1u << 0) | (1u << 4) | (1u << 9) | (1u << 10), false);
	CHECK_EQ(encode_slice(&cube), 367);
	cube = SOLVED_CUBE;
	apply_move(&cube, MOVE_R1);
	CHECK_EQ(encode_slice(&cube), 367);
}

/// Every one of the C(12,4) = 495 ways to pick 4 slots gets its own value
/// in 0..494 (nothing repeats, nothing is skipped), whichever order the
/// pieces are in, and decode puts the marked slots back where they were.
static void	test_slice_covers_every_pattern(void)
{
	int			seen[SLICE_COUNT];
	unsigned	mask;
	t_cube		cube;
	t_cube		other;
	t_cube		back;
	int			patterns;

	patterns = 0;
	memset(seen, 0, sizeof(seen));
	mask = 0;
	while (mask < (1u << EDGE_COUNT))
	{
		if (bit_count(mask) == 4)
		{
			cube = cube_from_mask(mask, false);
			other = cube_from_mask(mask, true);
			CHECK(edges_are_a_permutation(&cube));
			CHECK(encode_slice(&cube) < SLICE_COUNT);
			CHECK_EQ(encode_slice(&cube), encode_slice(&other));
			if (encode_slice(&cube) < SLICE_COUNT)
				seen[encode_slice(&cube)]++;
			back = SOLVED_CUBE;
			decode_slice(&back, encode_slice(&cube));
			CHECK_EQ(slice_mask(&back), mask);
			patterns++;
		}
		mask++;
	}
	CHECK_EQ(patterns, SLICE_COUNT);
	mask = 0;
	while (mask < SLICE_COUNT)
		CHECK_EQ(seen[mask++], 1);
}

static void	test_slice_exhaustive_round_trip(void)
{
	t_cube	cube;
	int		n;

	n = 0;
	while (n < SLICE_COUNT)
	{
		cube = SOLVED_CUBE;
		decode_slice(&cube, (uint16_t)n);
		CHECK(edges_are_a_permutation(&cube));
		CHECK_EQ(bit_count(slice_mask(&cube)), 4);
		CHECK_EQ(encode_slice(&cube), n);
		n++;
	}
	cube = SOLVED_CUBE;
	decode_slice(&cube, 0);
	CHECK(cube_is_solved(&cube));
}

/// Even a wrong (too big) value must not read outside the table or give a
/// broken cube. Nothing to say about which cube it is, only that it is real.
static void	test_slice_decode_is_safe(void)
{
	t_cube	cube;
	int		n;

	n = SLICE_COUNT;
	while (n < 65536)
	{
		cube = SOLVED_CUBE;
		decode_slice(&cube, (uint16_t)n);
		CHECK(edges_are_a_permutation(&cube));
		CHECK_EQ(bit_count(slice_mask(&cube)), 4);
		n += 251;
	}
}

/// The moves phase 2 will use (U, D any turn, and R2 L2 F2 B2) keep the
/// middle-layer edges in the middle layer, so slice stays 0. Every other
/// move breaks it. This is the reason slice is the phase 1 goal.
static void	test_slice_and_moves(void)
{
	static const t_move	keeps[] = {MOVE_U1, MOVE_U2, MOVE_U3, MOVE_D1, MOVE_D2,
		MOVE_D3, MOVE_R2, MOVE_L2, MOVE_F2, MOVE_B2};
	static const t_move	breaks[] = {MOVE_R1, MOVE_R3, MOVE_L1, MOVE_L3,
		MOVE_F1, MOVE_F3, MOVE_B1, MOVE_B3};
	t_cube				cube;
	size_t				i;

	i = 0;
	while (i < sizeof(keeps) / sizeof(keeps[0]))
	{
		cube = SOLVED_CUBE;
		apply_move(&cube, keeps[i++]);
		CHECK_EQ(encode_slice(&cube), 0);
	}
	i = 0;
	while (i < sizeof(breaks) / sizeof(breaks[0]))
	{
		cube = SOLVED_CUBE;
		apply_move(&cube, breaks[i++]);
		CHECK(encode_slice(&cube) != 0);
	}
}

/// Real cubes: decoding a cube's slice value rebuilds the same set of
/// marked slots, only edge_perm changes, and edge_orient / the corners are
/// never looked at.
static void	test_slice_random_walk(void)
{
	enum {STEPS = 20000};
	uint32_t	rng;
	t_cube		cube;
	t_cube		copy;
	t_cube		tweaked;
	int			i;

	rng = 777;
	cube = SOLVED_CUBE;
	i = 0;
	while (i < STEPS)
	{
		apply_move(&cube, (t_move)(test_rand(&rng) % MOVE_COUNT));
		CHECK(encode_slice(&cube) < SLICE_COUNT);
		copy = cube;
		decode_slice(&copy, encode_slice(&cube));
		CHECK_EQ(slice_mask(&copy), slice_mask(&cube));
		CHECK(edges_are_a_permutation(&copy));
		CHECK(memcmp(copy.edge_orient, cube.edge_orient,
				sizeof(cube.edge_orient)) == 0);
		CHECK(memcmp(copy.corner_perm, cube.corner_perm,
				sizeof(cube.corner_perm)) == 0);
		CHECK(memcmp(copy.corner_orient, cube.corner_orient,
				sizeof(cube.corner_orient)) == 0);
		tweaked = cube;
		tweaked.edge_orient[3] ^= 1;
		tweaked.edge_orient[5] ^= 1;
		tweaked.corner_orient[0] = 2;
		tweaked.corner_perm[0] = cube.corner_perm[1];
		CHECK_EQ(encode_slice(&tweaked), encode_slice(&cube));
		i++;
	}
}

static bool	corners_are_a_permutation(const t_cube *cube)
{
	int	seen[CORNER_COUNT];
	int	i;

	i = 0;
	while (i < CORNER_COUNT)
		seen[i++] = 0;
	i = 0;
	while (i < CORNER_COUNT)
		seen[cube->corner_perm[i++]]++;
	i = 0;
	while (i < CORNER_COUNT)
		if (seen[i++] != 1)
			return (false);
	return (true);
}

/// Builds a cube whose corner_perm is exactly the 8 given pieces.
static t_cube	cube_with_corners(int a, int b, int c, int d, int e, int f,
	int g, int h)
{
	t_cube	cube;

	cube = SOLVED_CUBE;
	cube.corner_perm[0] = (t_corner)a;
	cube.corner_perm[1] = (t_corner)b;
	cube.corner_perm[2] = (t_corner)c;
	cube.corner_perm[3] = (t_corner)d;
	cube.corner_perm[4] = (t_corner)e;
	cube.corner_perm[5] = (t_corner)f;
	cube.corner_perm[6] = (t_corner)g;
	cube.corner_perm[7] = (t_corner)h;
	return (cube);
}

static void	test_cperm_hand_values(void)
{
	t_cube	cube;

	CHECK_EQ(CPERM_COUNT, 40320);
	CHECK_EQ(encode_cperm(&SOLVED_CUBE), 0);
	cube = cube_with_corners(0, 1, 2, 3, 4, 5, 7, 6);
	CHECK_EQ(encode_cperm(&cube), 1);
	cube = cube_with_corners(0, 2, 1, 3, 4, 5, 6, 7);
	CHECK_EQ(encode_cperm(&cube), 720);
	cube = cube_with_corners(1, 0, 2, 3, 4, 5, 6, 7);
	CHECK_EQ(encode_cperm(&cube), 5040);
	cube = cube_with_corners(6, 1, 2, 3, 4, 5, 0, 7);
	CHECK_EQ(encode_cperm(&cube), 31112);
	cube = cube_with_corners(7, 6, 5, 4, 3, 2, 1, 0);
	CHECK_EQ(encode_cperm(&cube), 40319);
	cube = SOLVED_CUBE;
	apply_move(&cube, MOVE_U1);
	CHECK_EQ(encode_cperm(&cube), 15120);
	cube = SOLVED_CUBE;
	apply_move(&cube, MOVE_U2);
	CHECK_EQ(encode_cperm(&cube), 11520);
}

/// Next arrangement in dictionary order, in place. Returns false after the
/// last one. Written here on its own so the test does not lean on the
/// encoder to know what "position in the list" means.
static bool	next_arrangement(int *a, int n)
{
	int	i;
	int	j;
	int	tmp;

	i = n - 2;
	while (i >= 0 && a[i] > a[i + 1])
		i--;
	if (i < 0)
		return (false);
	j = n - 1;
	while (a[j] < a[i])
		j--;
	tmp = a[i];
	a[i] = a[j];
	a[j] = tmp;
	i++;
	j = n - 1;
	while (i < j)
	{
		tmp = a[i];
		a[i++] = a[j];
		a[j--] = tmp;
	}
	return (true);
}

/// The k-th arrangement in dictionary order must have cperm k, and decoding
/// k must give that arrangement back. All 40320 of them, one by one.
static void	test_cperm_matches_dictionary_order(void)
{
	int		arr[CORNER_COUNT];
	t_cube	cube;
	t_cube	back;
	int		k;
	int		i;

	i = 0;
	while (i < CORNER_COUNT)
	{
		arr[i] = i;
		i++;
	}
	k = 0;
	while (1)
	{
		cube = cube_with_corners(arr[0], arr[1], arr[2], arr[3], arr[4],
				arr[5], arr[6], arr[7]);
		CHECK_EQ(encode_cperm(&cube), k);
		back = SOLVED_CUBE;
		decode_cperm(&back, (uint16_t)k);
		CHECK(memcmp(back.corner_perm, cube.corner_perm,
				sizeof(cube.corner_perm)) == 0);
		k++;
		if (!next_arrangement(arr, CORNER_COUNT))
			break ;
	}
	CHECK_EQ(k, CPERM_COUNT);
}

static void	test_cperm_exhaustive_round_trip(void)
{
	t_cube	cube;
	int		n;

	n = 0;
	while (n < CPERM_COUNT)
	{
		cube = SOLVED_CUBE;
		decode_cperm(&cube, (uint16_t)n);
		CHECK(corners_are_a_permutation(&cube));
		CHECK_EQ(encode_cperm(&cube), n);
		n++;
	}
	cube = SOLVED_CUBE;
	decode_cperm(&cube, 0);
	CHECK(cube_is_solved(&cube));
}

/// A wrong (too big) value must wrap around instead of reading outside the
/// pool, and still give a real permutation.
static void	test_cperm_decode_is_safe(void)
{
	t_cube	cube;
	t_cube	wrapped;
	int		n;

	n = CPERM_COUNT;
	while (n < 65536)
	{
		cube = SOLVED_CUBE;
		decode_cperm(&cube, (uint16_t)n);
		CHECK(corners_are_a_permutation(&cube));
		wrapped = SOLVED_CUBE;
		decode_cperm(&wrapped, (uint16_t)(n % CPERM_COUNT));
		CHECK(cube_equal(&cube, &wrapped));
		n += 97;
	}
}

/// Real cubes: decoding a cube's cperm rebuilds its corner_perm, only
/// corner_perm changes, and twists and edges are never looked at.
static void	test_cperm_random_walk(void)
{
	enum {STEPS = 20000};
	uint32_t	rng;
	t_cube		cube;
	t_cube		copy;
	t_cube		tweaked;
	int			i;

	rng = 31337;
	cube = SOLVED_CUBE;
	i = 0;
	while (i < STEPS)
	{
		apply_move(&cube, (t_move)(test_rand(&rng) % MOVE_COUNT));
		CHECK(encode_cperm(&cube) < CPERM_COUNT);
		copy = cube;
		copy.corner_perm[0] = CORNER_URF;
		decode_cperm(&copy, encode_cperm(&cube));
		CHECK(memcmp(copy.corner_perm, cube.corner_perm,
				sizeof(cube.corner_perm)) == 0);
		CHECK(memcmp(copy.corner_orient, cube.corner_orient,
				sizeof(cube.corner_orient)) == 0);
		CHECK(memcmp(copy.edge_perm, cube.edge_perm,
				sizeof(cube.edge_perm)) == 0);
		CHECK(memcmp(copy.edge_orient, cube.edge_orient,
				sizeof(cube.edge_orient)) == 0);
		tweaked = cube;
		tweaked.corner_orient[1] = 2;
		tweaked.corner_orient[2] = 1;
		tweaked.edge_perm[0] = cube.edge_perm[1];
		tweaked.edge_orient[4] ^= 1;
		CHECK_EQ(encode_cperm(&tweaked), encode_cperm(&cube));
		i++;
	}
}

/// Builds a cube whose n edge slots starting at first_slot hold the pieces
/// arr[i] + piece_offset; everything else is solved.
static t_cube	cube_with_edge_run(const int *arr, int first_slot, int n,
	int piece_offset)
{
	t_cube	cube;
	int		i;

	cube = SOLVED_CUBE;
	i = 0;
	while (i < n)
	{
		cube.edge_perm[first_slot + i] = (t_edge)(arr[i] + piece_offset);
		i++;
	}
	return (cube);
}

/// True if the 4 slots 8..11 hold the pieces 8..11, each once.
static bool	slice_slots_hold_slice_pieces(const t_cube *cube)
{
	int	seen[4];
	int	i;

	i = 0;
	while (i < 4)
		seen[i++] = 0;
	i = 8;
	while (i < 12)
	{
		if (cube->edge_perm[i] < 8)
			return (false);
		seen[cube->edge_perm[i++] - 8]++;
	}
	i = 0;
	while (i < 4)
		if (seen[i++] != 1)
			return (false);
	return (true);
}

static void	test_eperm_sperm_hand_values(void)
{
	static const int	sperm_swap_last[4] = {0, 1, 3, 2};
	static const int	sperm_swap_first[4] = {1, 0, 2, 3};
	static const int	sperm_reversed[4] = {3, 2, 1, 0};
	t_cube				cube;

	CHECK_EQ(EPERM_COUNT, 40320);
	CHECK_EQ(SPERM_COUNT, 24);
	CHECK_EQ(encode_eperm(&SOLVED_CUBE), 0);
	CHECK_EQ(encode_sperm(&SOLVED_CUBE), 0);
	cube = cube_with_edge_run((const int[8]){0, 1, 2, 3, 4, 5, 7, 6}, 0, 8, 0);
	CHECK_EQ(encode_eperm(&cube), 1);
	cube = cube_with_edge_run((const int[8]){1, 0, 2, 3, 4, 5, 6, 7}, 0, 8, 0);
	CHECK_EQ(encode_eperm(&cube), 5040);
	cube = cube_with_edge_run((const int[8]){7, 6, 5, 4, 3, 2, 1, 0}, 0, 8, 0);
	CHECK_EQ(encode_eperm(&cube), 40319);
	cube = cube_with_edge_run(sperm_swap_last, 8, 4, 8);
	CHECK_EQ(encode_sperm(&cube), 1);
	cube = cube_with_edge_run(sperm_swap_first, 8, 4, 8);
	CHECK_EQ(encode_sperm(&cube), 6);
	cube = cube_with_edge_run(sperm_reversed, 8, 4, 8);
	CHECK_EQ(encode_sperm(&cube), 23);
	cube = SOLVED_CUBE;
	apply_move(&cube, MOVE_U1);
	CHECK_EQ(encode_eperm(&cube), 15120);
	CHECK_EQ(encode_sperm(&cube), 0);
	cube = SOLVED_CUBE;
	apply_move(&cube, MOVE_D1);
	CHECK_EQ(encode_eperm(&cube), 9);
	cube = SOLVED_CUBE;
	apply_move(&cube, MOVE_R2);
	CHECK_EQ(encode_eperm(&cube), 21024);
	CHECK_EQ(encode_sperm(&cube), 21);
}

/// The k-th arrangement in dictionary order must have value k (checked
/// with the independent next_arrangement()), for every k, and decoding k
/// must give it back without touching the other slots.
static void	test_eperm_matches_dictionary_order(void)
{
	int		arr[8];
	t_cube	cube;
	t_cube	back;
	int		k;
	int		i;

	i = 0;
	while (i < 8)
	{
		arr[i] = i;
		i++;
	}
	k = 0;
	while (1)
	{
		cube = cube_with_edge_run(arr, 0, 8, 0);
		CHECK_EQ(encode_eperm(&cube), k);
		back = SOLVED_CUBE;
		decode_eperm(&back, (uint16_t)k);
		CHECK(cube_equal(&back, &cube));
		k++;
		if (!next_arrangement(arr, 8))
			break ;
	}
	CHECK_EQ(k, EPERM_COUNT);
}

static void	test_sperm_matches_dictionary_order(void)
{
	int		arr[4];
	t_cube	cube;
	t_cube	back;
	int		k;
	int		i;

	i = 0;
	while (i < 4)
	{
		arr[i] = i;
		i++;
	}
	k = 0;
	while (1)
	{
		cube = cube_with_edge_run(arr, 8, 4, 8);
		CHECK_EQ(encode_sperm(&cube), k);
		back = SOLVED_CUBE;
		decode_sperm(&back, (uint16_t)k);
		CHECK(cube_equal(&back, &cube));
		k++;
		if (!next_arrangement(arr, 4))
			break ;
	}
	CHECK_EQ(k, SPERM_COUNT);
}

/// Every value round-trips, decode writes only its own slots, and out-of-
/// range values wrap around into a real arrangement (never read outside).
static void	test_eperm_sperm_round_trip_and_safety(void)
{
	t_cube	cube;
	t_cube	scrambled;
	int		n;

	n = 0;
	while (n < EPERM_COUNT)
	{
		cube = SOLVED_CUBE;
		decode_eperm(&cube, (uint16_t)n);
		CHECK_EQ(encode_eperm(&cube), n);
		CHECK(edges_are_a_permutation(&cube));
		CHECK(slice_slots_hold_slice_pieces(&cube));
		n++;
	}
	n = 0;
	while (n < SPERM_COUNT)
	{
		cube = SOLVED_CUBE;
		decode_sperm(&cube, (uint16_t)n);
		CHECK_EQ(encode_sperm(&cube), n);
		CHECK(edges_are_a_permutation(&cube));
		n++;
	}
	scrambled = SOLVED_CUBE;
	n = 0;
	while (n < 25)
		apply_move(&scrambled, (t_move)((n++ * 5) % MOVE_COUNT));
	cube = scrambled;
	decode_eperm(&cube, 777);
	CHECK(memcmp(cube.edge_perm + 8, scrambled.edge_perm + 8,
			4 * sizeof(t_edge)) == 0);
	CHECK(memcmp(cube.corner_perm, scrambled.corner_perm,
			sizeof(cube.corner_perm)) == 0);
	CHECK(memcmp(cube.edge_orient, scrambled.edge_orient,
			sizeof(cube.edge_orient)) == 0);
	cube = scrambled;
	decode_sperm(&cube, 13);
	CHECK(memcmp(cube.edge_perm, scrambled.edge_perm, 8 * sizeof(t_edge)) == 0);
	CHECK(memcmp(cube.corner_perm, scrambled.corner_perm,
			sizeof(cube.corner_perm)) == 0);
	n = EPERM_COUNT;
	while (n < 65536)
	{
		cube = SOLVED_CUBE;
		decode_eperm(&cube, (uint16_t)n);
		CHECK(edges_are_a_permutation(&cube));
		n += 89;
	}
	n = SPERM_COUNT;
	while (n < 65536)
	{
		cube = SOLVED_CUBE;
		decode_sperm(&cube, (uint16_t)n);
		CHECK(edges_are_a_permutation(&cube));
		n += 7;
	}
}

/// The reason these coordinates exist. Walk randomly using only the 10
/// moves phase 2 is allowed (U, D any turn, and R2 L2 F2 B2): the cube
/// stays in the "half-solved" state (twist = flip = slice = 0), and the
/// three permutation coordinates alone must rebuild it EXACTLY.
static void	test_phase2_coordinates_describe_the_cube(void)
{
	static const t_move	allowed[10] = {MOVE_U1, MOVE_U2, MOVE_U3, MOVE_D1,
		MOVE_D2, MOVE_D3, MOVE_R2, MOVE_L2, MOVE_F2, MOVE_B2};
	enum {STEPS = 20000};
	uint32_t	rng;
	t_cube		cube;
	t_cube		rebuilt;
	int			i;

	rng = 2024;
	cube = SOLVED_CUBE;
	i = 0;
	while (i < STEPS)
	{
		apply_move(&cube, allowed[test_rand(&rng) % 10]);
		CHECK_EQ(encode_twist(&cube), 0);
		CHECK_EQ(encode_flip(&cube), 0);
		CHECK_EQ(encode_slice(&cube), 0);
		rebuilt = SOLVED_CUBE;
		decode_cperm(&rebuilt, encode_cperm(&cube));
		decode_eperm(&rebuilt, encode_eperm(&cube));
		decode_sperm(&rebuilt, encode_sperm(&cube));
		CHECK(cube_equal(&rebuilt, &cube));
		i++;
	}
}

/// On any cube, eperm and sperm stay in range, and ignore everything but
/// their own slots.
static void	test_eperm_sperm_random_walk(void)
{
	enum {STEPS = 20000};
	uint32_t	rng;
	t_cube		cube;
	t_cube		tweaked;
	int			i;

	rng = 8675309;
	cube = SOLVED_CUBE;
	i = 0;
	while (i < STEPS)
	{
		apply_move(&cube, (t_move)(test_rand(&rng) % MOVE_COUNT));
		CHECK(encode_eperm(&cube) < EPERM_COUNT);
		CHECK(encode_sperm(&cube) < SPERM_COUNT);
		tweaked = cube;
		tweaked.corner_perm[0] = cube.corner_perm[1];
		tweaked.corner_orient[3] = 1;
		tweaked.edge_orient[5] ^= 1;
		tweaked.edge_perm[9] = cube.edge_perm[10];
		CHECK_EQ(encode_eperm(&tweaked), encode_eperm(&cube));
		tweaked = cube;
		tweaked.edge_perm[2] = cube.edge_perm[3];
		tweaked.edge_perm[7] = cube.edge_perm[6];
		tweaked.corner_orient[1] = 2;
		CHECK_EQ(encode_sperm(&tweaked), encode_sperm(&cube));
		i++;
	}
}

int	main(void)
{
	test_solved_and_sizes();
	test_place_values();
	test_known_values();
	test_exhaustive_round_trip();
	test_decode_touches_only_its_property();
	test_random_walk();
	test_ignores_permutation();
	test_slice_hand_values();
	test_slice_covers_every_pattern();
	test_slice_exhaustive_round_trip();
	test_slice_decode_is_safe();
	test_slice_and_moves();
	test_slice_random_walk();
	test_cperm_hand_values();
	test_cperm_matches_dictionary_order();
	test_cperm_exhaustive_round_trip();
	test_cperm_decode_is_safe();
	test_cperm_random_walk();
	test_eperm_sperm_hand_values();
	test_eperm_matches_dictionary_order();
	test_sperm_matches_dictionary_order();
	test_eperm_sperm_round_trip_and_safety();
	test_phase2_coordinates_describe_the_cube();
	test_eperm_sperm_random_walk();
	return (test_report("coord"));
}
