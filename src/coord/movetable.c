#include <stdlib.h>
#include <string.h>
#include "movetable.h"

/// The two function shapes every coordinate has: decode_* writes one
/// coordinate value into a cube, encode_* reads it back out. Passing them
/// in lets one builder make all six tables.
typedef void		(*t_decode_fn)(t_cube *, uint16_t);
typedef uint16_t	(*t_encode_fn)(const t_cube *);

/// @brief One table cell, worked out the slow, obviously-correct way.
///
/// Rebuild a cube that has this coordinate value (starting from
/// SOLVED_CUBE, so every property the coordinate does not describe is a
/// valid solved value), turn it with the real apply_move(), and read the
/// coordinate back. The whole cube is turned, including the piece the
/// coordinate does not store (e.g. corner 7's twist), so the "forced"
/// values stay right on their own.
///
/// @return The coordinate value after the move.
static uint16_t	next_state(t_decode_fn decode, t_encode_fn encode,
	uint16_t state, t_move move)
{
	t_cube	cube;

	cube = SOLVED_CUBE;
	decode(&cube, state);
	apply_move(&cube, move);
	return (encode(&cube));
}

/// @brief Fills the 18 cells of one row (one coordinate value).
///
/// A move NOT in `allowed` is never applied, only marked MT_ILLEGAL.
/// This order matters: encode_eperm() on a cube where a quarter turn of R
/// just pushed a middle-layer edge into the top layer does not describe a
/// real permutation, and would return garbage (or read out of range).
static void	fill_row(uint16_t *row, t_decode_fn decode, t_encode_fn encode,
	uint16_t state, t_move_mask allowed)
{
	int	move;

	move = 0;
	while (move < MOVE_COUNT)
	{
		if (move_in_mask(allowed, (t_move)move))
			row[move] = next_state(decode, encode, state, (t_move)move);
		else
			row[move] = MT_ILLEGAL;
		move++;
	}
}

/// @brief Allocates and fills one whole table.
///
/// @param count   How many values the coordinate has (its *_COUNT).
/// @param allowed Moves that get a real answer; the rest get MT_ILLEGAL.
/// @return The table, or NULL if malloc failed.
static uint16_t	*build_table(int count, t_decode_fn decode,
	t_encode_fn encode, t_move_mask allowed)
{
	uint16_t	*table;
	int			state;

	table = malloc((size_t)count * MOVE_COUNT * sizeof(uint16_t));
	if (!table)
		return (NULL);
	state = 0;
	while (state < count)
	{
		fill_row(table + state * MOVE_COUNT, decode, encode,
			(uint16_t)state, allowed);
		state++;
	}
	return (table);
}

void	move_tables_free(t_move_tables *tables)
{
	free(tables->twist);
	free(tables->flip);
	free(tables->slice);
	free(tables->cperm);
	free(tables->eperm);
	free(tables->sperm);
	memset(tables, 0, sizeof(*tables));
}

/// The corner arrangement (cperm) is well defined after ANY move, so it
/// gets all 18. eperm and sperm only get the 10 phase-2 moves.
bool	move_tables_build(t_move_tables *tables)
{
	memset(tables, 0, sizeof(*tables));
	tables->twist = build_table(TWIST_COUNT, decode_twist, encode_twist,
			MOVES_ALL);
	tables->flip = build_table(FLIP_COUNT, decode_flip, encode_flip,
			MOVES_ALL);
	tables->slice = build_table(SLICE_COUNT, decode_slice, encode_slice,
			MOVES_ALL);
	tables->cperm = build_table(CPERM_COUNT, decode_cperm, encode_cperm,
			MOVES_ALL);
	tables->eperm = build_table(EPERM_COUNT, decode_eperm, encode_eperm,
			MOVES_PHASE2);
	tables->sperm = build_table(SPERM_COUNT, decode_sperm, encode_sperm,
			MOVES_PHASE2);
	if (!tables->twist || !tables->flip || !tables->slice
		|| !tables->cperm || !tables->eperm || !tables->sperm)
	{
		move_tables_free(tables);
		return (false);
	}
	return (true);
}
