#include <stdbool.h>
#include "coord.h"
#include "coord_tables.h"

/// coordinate -> cube. The opposite direction (cube -> coordinate) lives in
/// encode.c, and the constant tables both need are in tables.c.

/// @brief Writes corner orientations for a twist value (see coord.h).
///
/// The reverse of encode_twist(), so the digits come out last to first.
/// twist % 3 is the last digit (remainder after dividing by 3), and
/// twist / 3 drops it. Repeat for slots 6 down to 0. The 8th twist is not
/// stored in the number: it is whatever brings the total to a multiple of
/// 3, which is (3 - sum % 3) % 3.
///
/// @param cube  Only corner_orient[] is written.
/// @param twist Value below TWIST_COUNT. Bigger values lose their top part.
void	decode_twist(t_cube *cube, uint16_t twist)
{
	int	sum;
	int	i;

	sum = 0;
	i = CORNER_COUNT - 2;
	while (i >= 0)
	{
		cube->corner_orient[i] = (uint8_t)(twist % 3);
		sum += twist % 3;
		twist = (uint16_t)(twist / 3);
		i--;
	}
	cube->corner_orient[CORNER_COUNT - 1] = (uint8_t)((3 - sum % 3) % 3);
}

/// @brief Writes edge orientations for a flip value (see coord.h).
///
/// The reverse of encode_flip(): flip % 2 is the last bit, flip / 2 drops
/// it, for slots 10 down to 0. The 12th flip is sum % 2: if the other
/// eleven add to an odd number, this one must be 1 to make the total even.
///
/// @param cube Only edge_orient[] is written.
/// @param flip Value below FLIP_COUNT. Bigger values lose their top part.
void	decode_flip(t_cube *cube, uint16_t flip)
{
	int	sum;
	int	i;

	sum = 0;
	i = EDGE_COUNT - 2;
	while (i >= 0)
	{
		cube->edge_orient[i] = (uint8_t)(flip % 2);
		sum += flip % 2;
		flip = (uint16_t)(flip / 2);
		i--;
	}
	cube->edge_orient[EDGE_COUNT - 1] = (uint8_t)(sum % 2);
}

/// @brief Writes an edge arrangement for a slice value (see coord.h).
///
/// The reverse of encode_slice(), done greedily: for the 4th sticker, take
/// the biggest d whose C(d, 4) still fits in the value, subtract it, then
/// do the same with C(d, 3) for the 3rd, C(d, 2), and C(d, 1). Each d found
/// marks slot 11 - d. Then the marked slots are filled with the middle-layer
/// pieces (8..11) and the rest with the others (0..7), each in slot order.
///
/// Any input, even one that is too big, gives 4 marked slots and a real
/// permutation, so it can never index outside the table.
///
/// @param cube  Only edge_perm[] is written.
/// @param slice Value below SLICE_COUNT.
void	decode_slice(t_cube *cube, uint16_t slice)
{
	bool	marked[EDGE_COUNT];
	int		next_slice;
	int		next_other;
	int		d;
	int		k;

	d = 0;
	while (d < EDGE_COUNT)
		marked[d++] = false;
	d = EDGE_COUNT - 1;
	k = 4;
	while (k >= 1)
	{
		while (g_binom[d][k] > slice)
			d--;
		marked[EDGE_COUNT - 1 - d] = true;
		slice = (uint16_t)(slice - g_binom[d][k]);
		d--;
		k--;
	}
	next_slice = EDGE_FR;
	next_other = EDGE_UR;
	d = 0;
	while (d < EDGE_COUNT)
	{
		if (marked[d])
			cube->edge_perm[d] = (t_edge)next_slice++;
		else
			cube->edge_perm[d] = (t_edge)next_other++;
		d++;
	}
}

/// @brief The reverse of lehmer_encode() (in encode.c): writes the arrangement of the
///        pieces 0..n-1 at a given position in the list.
///
/// Keep a pool of the pieces not used yet, in increasing order. For each
/// slot, divide what is left of the value by (slots left after this one)!:
/// the quotient says how many pool pieces to skip, so the piece at that
/// position in the pool goes in this slot (and leaves the pool), and the
/// remainder carries on to the next slot.
///
/// Worked example: 31112 (the reverse of the example in lehmer_encode())
///   31112 / 7! = 6 remainder 872: skip 6 pieces, so the pool's 7th piece
///   (piece 6) goes in slot 0. 872 / 6! = 1 remainder 152: the pool is now
///   {0,1,2,3,4,5,7}, skip 1, piece 1 goes in slot 1, and so on.
///
/// @param pieces Written: n values, a permutation of 0..n-1.
/// @param n      How many slots (at most 8).
/// @param value  Below n!. (Callers wrap it first, so this never reads
///               outside the pool.)
static void	lehmer_decode(int *pieces, int n, int value)
{
	int	pool[CORNER_COUNT];
	int	left;
	int	slot;
	int	pick;
	int	i;

	i = 0;
	while (i < n)
	{
		pool[i] = i;
		i++;
	}
	left = n;
	slot = 0;
	while (slot < n)
	{
		pick = value / g_fact[n - 1 - slot];
		value = value % g_fact[n - 1 - slot];
		pieces[slot] = pool[pick];
		i = pick;
		while (i < left - 1)
		{
			pool[i] = pool[i + 1];
			i++;
		}
		left--;
		slot++;
	}
}

/// @brief Writes a corner arrangement for a cperm value (see coord.h).
///
/// @param cube  Only corner_perm[] is written.
/// @param cperm Value below CPERM_COUNT (bigger values wrap around).
void	decode_cperm(t_cube *cube, uint16_t cperm)
{
	int	pieces[CORNER_COUNT];
	int	i;

	lehmer_decode(pieces, CORNER_COUNT, cperm % CPERM_COUNT);
	i = 0;
	while (i < CORNER_COUNT)
	{
		cube->corner_perm[i] = (t_corner)pieces[i];
		i++;
	}
}

/// @brief Writes the top and bottom edge order for an eperm value.
///
/// @param cube  Only edge_perm[0..7] is written. Slots 8..11 are untouched.
/// @param eperm Value below EPERM_COUNT (bigger values wrap around).
void	decode_eperm(t_cube *cube, uint16_t eperm)
{
	int	pieces[UD_EDGE_COUNT];
	int	i;

	lehmer_decode(pieces, UD_EDGE_COUNT, eperm % EPERM_COUNT);
	i = 0;
	while (i < UD_EDGE_COUNT)
	{
		cube->edge_perm[i] = (t_edge)pieces[i];
		i++;
	}
}

/// @brief Writes the middle-layer edge order for a sperm value.
///
/// @param cube  Only edge_perm[8..11] is written. Slots 0..7 are untouched.
/// @param sperm Value below SPERM_COUNT (bigger values wrap around).
void	decode_sperm(t_cube *cube, uint16_t sperm)
{
	int	pieces[SLICE_EDGE_COUNT];
	int	i;

	lehmer_decode(pieces, SLICE_EDGE_COUNT, sperm % SPERM_COUNT);
	i = 0;
	while (i < SLICE_EDGE_COUNT)
	{
		cube->edge_perm[UD_EDGE_COUNT + i] = (t_edge)(EDGE_FR + pieces[i]);
		i++;
	}
}
