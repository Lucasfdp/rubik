#include "coord_tables.h"

/// C(n, k) ("n choose k": in how many ways can you pick k things out of n
/// without caring about order) for n = 0..11 and k = 0..4. C(n, k) is 0
/// when k > n. Row n is Pascal's triangle row n, cut at 4 columns.
const uint16_t	g_binom[EDGE_COUNT][5] = {
	{1, 0, 0, 0, 0},
	{1, 1, 0, 0, 0},
	{1, 2, 1, 0, 0},
	{1, 3, 3, 1, 0},
	{1, 4, 6, 4, 1},
	{1, 5, 10, 10, 5},
	{1, 6, 15, 20, 15},
	{1, 7, 21, 35, 35},
	{1, 8, 28, 56, 70},
	{1, 9, 36, 84, 126},
	{1, 10, 45, 120, 210},
	{1, 11, 55, 165, 330}
};

/// Factorials 0! .. 7!: g_fact[n] is n*(n-1)*...*1 (and 0! = 1). g_fact[n]
/// is how many ways there are to arrange n pieces in n slots.
const uint16_t	g_fact[CORNER_COUNT] = {1, 1, 2, 6, 24, 120, 720, 5040};
