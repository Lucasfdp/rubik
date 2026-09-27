#include "coord.h"
#include "coord_tables.h"

/// cube -> coordinate. The opposite direction (coordinate -> cube) lives in
/// decode.c, and the constant tables both need are in tables.c.

/// @brief Corner orientations as one number (see coord.h).
///
/// "Shift and add", the way you read a decimal number digit by digit: start
/// at 0, and for each digit do total = total * 3 + digit. Here the digits
/// are the twists of slots 0..6 (0, 1 or 2 each), slot 0 first. Slot 7 is
/// skipped because it is forced by the other seven.
///
/// Worked example: twists {2,0,0,1,1,0,0,2} (what one R turn gives)
///   0*3+2 = 2, 2*3+0 = 6, 6*3+0 = 18, 18*3+1 = 55, 55*3+1 = 166,
///   166*3+0 = 498, 498*3+0 = 1494.
///
/// @param cube Cube to read. Only corner_orient[0..6] is looked at.
/// @return Value in 0 .. TWIST_COUNT-1. The solved cube gives 0.
uint16_t	encode_twist(const t_cube *cube)
{
	uint16_t	twist;
	int			i;

	twist = 0;
	i = 0;
	while (i < CORNER_COUNT - 1)
	{
		twist = (uint16_t)(twist * 3 + cube->corner_orient[i]);
		i++;
	}
	return (twist);
}

/// @brief Edge orientations as one number (see coord.h).
///
/// Same shift-and-add as encode_twist(), in base 2: total = total * 2 +
/// bit, over the flips of slots 0..10, slot 0 first. Slot 11 is forced.
///
/// @param cube Cube to read. Only edge_orient[0..10] is looked at.
/// @return Value in 0 .. FLIP_COUNT-1. The solved cube gives 0.
uint16_t	encode_flip(const t_cube *cube)
{
	uint16_t	flip;
	int			i;

	flip = 0;
	i = 0;
	while (i < EDGE_COUNT - 1)
	{
		flip = (uint16_t)(flip * 2 + cube->edge_orient[i]);
		i++;
	}
	return (flip);
}

/// @brief Where the 4 middle-layer edges are, as one number (see coord.h).
///
/// Measure each slot by its distance d from the right end: slot 11 is
/// d = 0, slot 10 is d = 1, ... slot 0 is d = 11. Walk the slots from slot
/// 11 down to slot 0. The first slot that holds a middle-layer edge (so
/// the smallest d) adds C(d, 1), the second adds C(d, 2), the third C(d, 3)
/// and the fourth C(d, 4). This "counting system" gives each of the 495
/// possible sets of 4 slots its own number, 0 to 494, with no gaps.
///
/// Worked example: after one R turn the middle-layer edges sit in slots
/// 10, 9, 4 and 0, so d = 1, 2, 7, 11 and the value is
/// C(1,1) + C(2,2) + C(7,3) + C(11,4) = 1 + 1 + 35 + 330 = 367.
///
/// @param cube Cube to read. Only edge_perm[] is looked at.
/// @return Value in 0 .. SLICE_COUNT-1. The solved cube gives 0.
uint16_t	encode_slice(const t_cube *cube)
{
	uint16_t	slice;
	int			found;
	int			slot;

	slice = 0;
	found = 0;
	slot = EDGE_COUNT - 1;
	while (slot >= 0)
	{
		if (cube->edge_perm[slot] >= EDGE_FR && found < 4)
		{
			found++;
			slice = (uint16_t)(slice + g_binom[EDGE_COUNT - 1 - slot][found]);
		}
		slot--;
	}
	return (slice);
}

/// @brief Position of an arrangement in the dictionary-order list of all
///        arrangements of n pieces (n <= 8). Shared by cperm, eperm, sperm.
///
/// For each slot, from slot 0 on, count how many pieces SMALLER than the
/// one in this slot are still to come in the slots after it. Each of those
/// smaller pieces would have come first in the list, and each hides a whole
/// block of arrangements of the remaining slots, so the count is multiplied
/// by (slots left after this one)! and added up. (This is called the
/// Lehmer code.) Only the ORDER of the values matters, not the values.
///
/// Worked example: {6,1,2,3,4,5,0,7}
///   slot 0 holds 6, six smaller pieces come later: 6 * 7! = 30240
///   slot 1 holds 1, one smaller (the 0) comes later: 1 * 6! = 720
///   slot 2 holds 2, the 0 comes later: 1 * 5! = 120
///   slots 3, 4, 5 (holding 3, 4, 5) each see the 0 later: 24 + 6 + 2
///   slots 6 and 7: nothing smaller comes later: 0
///   total 31112
///
/// @param pieces The n values, one per slot.
/// @param n      How many slots (at most 8).
/// @return 0 .. n!-1, and 0 when the pieces are in increasing order.
static int	lehmer_encode(const int *pieces, int n)
{
	int	value;
	int	smaller;
	int	slot;
	int	later;

	value = 0;
	slot = 0;
	while (slot < n)
	{
		smaller = 0;
		later = slot + 1;
		while (later < n)
		{
			if (pieces[later] < pieces[slot])
				smaller++;
			later++;
		}
		value += smaller * g_fact[n - 1 - slot];
		slot++;
	}
	return (value);
}

/// @brief Which corner is in which slot, as one number (see coord.h).
///
/// @param cube Cube to read. Only corner_perm[] is looked at.
/// @return Value in 0 .. CPERM_COUNT-1. The solved cube gives 0.
uint16_t	encode_cperm(const t_cube *cube)
{
	int	pieces[CORNER_COUNT];
	int	i;

	i = 0;
	while (i < CORNER_COUNT)
	{
		pieces[i] = (int)cube->corner_perm[i];
		i++;
	}
	return ((uint16_t)lehmer_encode(pieces, CORNER_COUNT));
}

/// @brief Order of the top and bottom edges as one number (see coord.h).
///
/// Same counting as cperm, over the pieces in slots 0..7. Only valid once
/// the middle-layer edges are in slots 8..11, when slots 0..7 hold exactly
/// the pieces 0..7.
///
/// @param cube Cube to read. Only edge_perm[0..7] is looked at.
/// @return Value in 0 .. EPERM_COUNT-1. The solved cube gives 0.
uint16_t	encode_eperm(const t_cube *cube)
{
	int	pieces[UD_EDGE_COUNT];
	int	i;

	i = 0;
	while (i < UD_EDGE_COUNT)
	{
		pieces[i] = (int)cube->edge_perm[i];
		i++;
	}
	return ((uint16_t)lehmer_encode(pieces, UD_EDGE_COUNT));
}

/// @brief Order of the 4 middle-layer edges as one number (see coord.h).
///
/// Same counting on the 4 pieces in slots 8..11 (which are the pieces 8..11
/// in some order once phase 1 is done). Only the order of the values
/// matters, so they need no shifting down to 0..3.
///
/// @param cube Cube to read. Only edge_perm[8..11] is looked at.
/// @return Value in 0 .. SPERM_COUNT-1. The solved cube gives 0.
uint16_t	encode_sperm(const t_cube *cube)
{
	int	pieces[SLICE_EDGE_COUNT];
	int	i;

	i = 0;
	while (i < SLICE_EDGE_COUNT)
	{
		pieces[i] = (int)cube->edge_perm[UD_EDGE_COUNT + i];
		i++;
	}
	return ((uint16_t)lehmer_encode(pieces, SLICE_EDGE_COUNT));
}
