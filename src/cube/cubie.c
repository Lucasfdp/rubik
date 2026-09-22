/* ========================================================================
 * cube/cubie.c — the solved-state constant, and pure data-layer helpers.
 *
 * Nothing here is "the algorithm." apply_move() and the 18 move tables
 * (cube/moves.c) are Sprint 0's actual pairing task and are deliberately
 * NOT in this file — see the prototype comment in include/cube.h.
 * ======================================================================== */

#include "cube.h"
#include <string.h>

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

bool	cube_equal(const t_cube *a, const t_cube *b)
{
	return (memcmp(a, b, sizeof(t_cube)) == 0);
}

bool	cube_is_solved(const t_cube *cube)
{
	return (cube_equal(cube, &SOLVED_CUBE));
}

/* Returns the sum of all 8 corner_orient values, mod 3. A valid cube
 * always yields 0 — see docs/en/02a-cube-notation.md §4. */
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

/* Same idea for the 12 edge_orient values, mod 2 — §5 of the same file. */
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

/* Parity of a permutation: count out-of-order pairs (inversions), true if
 * that count is odd. Generic algorithm, nothing cube-specific. */
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

/* A physically scrambled cube always has corner permutation parity equal
 * to edge permutation parity — GLOSSARY.md, "Parity". Combined with the
 * two orientation invariants above, this is the full legality check R7
 * needs before handing input to the solver. */
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
