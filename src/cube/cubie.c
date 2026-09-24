#include "cube.h"
#include <string.h>

/// Definition of SOLVED_CUBE (declared in cube.h).
///
/// Slot i holds piece i for both corners and edges, and every orientation
/// is 0.
const t_cube	SOLVED_CUBE = {
	.corner_perm = {
		CORNER_URF, CORNER_UFL, CORNER_ULB, CORNER_UBR,
		CORNER_DFR, CORNER_DLF, CORNER_DBL, CORNER_DRB
	},
	.corner_orient = {0, 0, 0, 0, 0, 0, 0, 0},
	.edge_perm = {
		EDGE_UR, EDGE_UF, EDGE_UL, EDGE_UB,
		EDGE_DR, EDGE_DF, EDGE_DL, EDGE_DB,
		EDGE_FR, EDGE_FL, EDGE_BL, EDGE_BR
	},
	.edge_orient = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}
};

/// @brief Compares two cubes by state.
///
/// A plain memcmp is exact here because t_cube has no padding bytes today
/// (only enums and uint8_t arrays). If fields are ever added, check that
/// still holds or compare field by field.
///
/// @param a First cube.
/// @param b Second cube.
/// @return true if identical.
bool	cube_equal(const t_cube *a, const t_cube *b)
{
	return (memcmp(a, b, sizeof(t_cube)) == 0);
}

/// @brief Tells whether the cube is solved.
///
/// @param cube Cube to check.
/// @return true if it equals SOLVED_CUBE.
bool	cube_is_solved(const t_cube *cube)
{
	return (cube_equal(cube, &SOLVED_CUBE));
}

/// @brief Sums the 8 corner twists, mod 3.
///
/// Every reachable cube gives 0 (docs/en/02a-cube-notation.md, section
/// 4). Anything else means a corner was twisted by hand.
///
/// @param cube Cube to check.
/// @return 0 if valid, 1 or 2 otherwise.
int	cube_corner_twist_sum(const t_cube *cube)
{
	int	sum;
	int	i;

	sum = 0;
	i = 0;
	while (i < CORNER_COUNT)
	{
		sum += cube->corner_orient[i];
		i++;
	}
	return (sum % 3);
}

/// @brief Sums the 12 edge flips, mod 2.
///
/// Every reachable cube gives 0 (docs/en/02a-cube-notation.md, section
/// 5). Anything else means an edge was flipped by hand.
///
/// @param cube Cube to check.
/// @return 0 if valid, 1 otherwise.
int	cube_edge_flip_sum(const t_cube *cube)
{
	int	sum;
	int	i;

	sum = 0;
	i = 0;
	while (i < EDGE_COUNT)
	{
		sum += cube->edge_orient[i];
		i++;
	}
	return (sum % 2);
}

/// @brief Tells whether a permutation is odd (its parity).
///
/// Counts inversions, i.e. pairs (i, j) with i < j and perm[i] > perm[j].
/// An odd count means an odd permutation. Generic algorithm, nothing
/// cube-specific. It is O(n^2), which is fine for n = 8 and n = 12.
///
/// @param perm Array of n distinct integers.
/// @param n    Length of perm.
/// @return true if the permutation is odd.
static bool	permutation_is_odd(const int *perm, int n)
{
	int	inversions;
	int	i;
	int	j;

	inversions = 0;
	i = 0;
	while (i < n)
	{
		j = i + 1;
		while (j < n)
		{
			if (perm[i] > perm[j])
				inversions++;
			j++;
		}
		i++;
	}
	return (inversions % 2 != 0);
}

/// @brief Full legality check: could this cube come from a real one?
///
/// A cube is reachable only if all three hold:
///   1. corner twists sum to 0 mod 3
///   2. edge flips sum to 0 mod 2
///   3. corner permutation parity == edge permutation parity
///      (every quarter turn flips both parities at once, so they can
///      never disagree; see GLOSSARY.md, "Parity")
///
/// Assumes corner_perm and edge_perm are real permutations (each piece
/// exactly once). That is not checked here.
///
/// @param cube Cube to check.
/// @return true if all three conditions hold.
bool	cube_is_valid(const t_cube *cube)
{
	int	corners[CORNER_COUNT];
	int	edges[EDGE_COUNT];
	int	i;

	if (cube_corner_twist_sum(cube) != 0)
		return (false);
	if (cube_edge_flip_sum(cube) != 0)
		return (false);
	i = 0;
	while (i < CORNER_COUNT)
	{
		corners[i] = (int)cube->corner_perm[i];
		i++;
	}
	i = 0;
	while (i < EDGE_COUNT)
	{
		edges[i] = (int)cube->edge_perm[i];
		i++;
	}
	return (permutation_is_odd(corners, CORNER_COUNT)
		== permutation_is_odd(edges, EDGE_COUNT));
}
