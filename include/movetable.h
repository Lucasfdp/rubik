#ifndef MOVETABLE_H
# define MOVETABLE_H

# include <stdbool.h>
# include <stdint.h>
# include "cube.h"
# include "coord.h"

/// A move table answers one question with ONE array lookup: "if the cube
/// is in state N and I turn a face (move M), which state is it in
/// afterwards?" Without it, every step of the search would have to rebuild
/// a whole cube from N, turn it, and squash it back down to a number
/// (docs/en/02-algorithms.md, item 5).
///
/// There is one table per coordinate. A table is a flat array with one row
/// per coordinate value and MOVE_COUNT (18) columns, one per move, so the
/// answer for state N and move M is table[N * MOVE_COUNT + M]. That is
/// "table[coord][move]" written flat: one malloc, one free. Use
/// move_table_next() instead of typing the formula by hand.

/// A SENTINEL is a value put in a spot on purpose to mean "there is no real
/// answer here". Real coordinates are small (the biggest is 40319), so
/// 0xFFFF (65535) can never be a real one. Table cells for moves that are
/// not allowed hold MT_ILLEGAL. If it ever leaks into a lookup as an index,
/// it lands far outside the table and crashes loudly, instead of quietly
/// giving a wrong-but-plausible number.
# define MT_ILLEGAL 0xFFFF

/// A set of moves packed into one number: bit m is 1 when move m (a t_move)
/// is in the set. Lets the search ask "may I use this move?" with one AND,
/// and lets phase 1 and phase 2 share the same search code with a
/// different set each.
typedef uint32_t	t_move_mask;

/// The bit for one move: 1 shifted left by the move number.
# define MOVE_BIT(m) (1u << (m))

/// All 18 moves. Phase 1 may use all of them.
# define MOVES_ALL ((1u << MOVE_COUNT) - 1)

/// The 10 moves phase 2 may use: any turn of U and D, and only the 180
/// of R, F, L and B. Those are exactly the moves that keep the phase 1
/// goal (twist 0, flip 0, slice 0) true. A quarter turn of R, F, L or B
/// would break it again, so phase 2 must never use one.
# define MOVES_PHASE2 ( \
	MOVE_BIT(MOVE_U1) | MOVE_BIT(MOVE_U2) | MOVE_BIT(MOVE_U3) \
	| MOVE_BIT(MOVE_D1) | MOVE_BIT(MOVE_D2) | MOVE_BIT(MOVE_D3) \
	| MOVE_BIT(MOVE_R2) | MOVE_BIT(MOVE_F2) \
	| MOVE_BIT(MOVE_L2) | MOVE_BIT(MOVE_B2))

/// Thistlethwaite's own phase masks (docs/en/10-thistlethwaite-spec.md §1).
/// Each is the previous group's own moves: using anything else while
/// searching inside that group would undo the property already locked in.

/// Phase 2's search mask: G1's own moves. Any U, D, L or R turn is safe
/// (none of them ever flip an edge, per FACE_TABLES in src/cube/moves.c).
/// Only F and B are locked to half-turn: a quarter F or B flips 4 edges
/// (breaking flip == 0); F2/B2 flip each of those edges twice, net zero.
# define MOVES_G1 ( \
	MOVE_BIT(MOVE_U1) | MOVE_BIT(MOVE_U2) | MOVE_BIT(MOVE_U3) \
	| MOVE_BIT(MOVE_D1) | MOVE_BIT(MOVE_D2) | MOVE_BIT(MOVE_D3) \
	| MOVE_BIT(MOVE_L1) | MOVE_BIT(MOVE_L2) | MOVE_BIT(MOVE_L3) \
	| MOVE_BIT(MOVE_R1) | MOVE_BIT(MOVE_R2) | MOVE_BIT(MOVE_R3) \
	| MOVE_BIT(MOVE_F2) | MOVE_BIT(MOVE_B2))

/// Phase 4's search mask: G3's own moves, the 6 half turns. A strict
/// subset of MOVES_PHASE2, so eperm/sperm already have real answers for
/// every one of these columns.
# define MOVES_G3 ( \
	MOVE_BIT(MOVE_U2) | MOVE_BIT(MOVE_D2) | MOVE_BIT(MOVE_L2) \
	| MOVE_BIT(MOVE_R2) | MOVE_BIT(MOVE_F2) | MOVE_BIT(MOVE_B2))

/// @brief True if the move is in the set.
static inline bool	move_in_mask(t_move_mask mask, t_move move)
{
	return ((mask & MOVE_BIT(move)) != 0);
}

/// All six move tables. Each pointer is a flat array of
/// (number of values of that coordinate) * MOVE_COUNT uint16_t cells.
///
/// - twist, flip, slice, cperm: every move works, no cell is MT_ILLEGAL.
/// - eperm, sperm: only the 10 phase-2 moves work. The other 8 columns
///   (quarter turns of R, F, L, B) hold MT_ILLEGAL in every row, because
///   those coordinates only mean something while the middle-layer edges
///   are in the middle layer (see coord.h).
typedef struct s_move_tables
{
	uint16_t	*twist;
	uint16_t	*flip;
	uint16_t	*slice;
	uint16_t	*cperm;
	uint16_t	*eperm;
	uint16_t	*sperm;
}	t_move_tables;

/// @brief State reached from `state` after `move`: one table lookup.
///
/// @param table A table from t_move_tables.
/// @param state Coordinate value, below that coordinate's COUNT.
/// @param move  Any move below MOVE_COUNT.
/// @return The new coordinate value, or MT_ILLEGAL if `move` is not
///         allowed for this table (eperm / sperm only).
static inline uint16_t	move_table_next(const uint16_t *table, uint16_t state,
	t_move move)
{
	return (table[state * MOVE_COUNT + move]);
}

/// @brief Builds all six tables (allocates them). Call once at startup.
///
/// @param tables Filled in. Its old contents are ignored, not freed.
/// @return true on success. On false (malloc failed) nothing is left
///         allocated and every pointer in `tables` is NULL.
bool	move_tables_build(t_move_tables *tables);

/// @brief Frees all six tables and sets the pointers to NULL. Safe to call
///        twice, or on a struct that was never built and set to zero.
void	move_tables_free(t_move_tables *tables);

#endif
