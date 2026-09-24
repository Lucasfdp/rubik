#ifndef CUBE_H
# define CUBE_H

# include <stdint.h>
# include <stdbool.h>

/// Number of corner pieces on a 3x3x3 cube.
# define CORNER_COUNT 8

/// Number of edge pieces on a 3x3x3 cube.
# define EDGE_COUNT   12

/// Identity of each of the 8 corner pieces (which piece, not which slot).
///
/// Names are the three faces meeting at the corner: URF is the corner
/// between Up, Right and Front. Order and names follow Kociemba's
/// convention (docs/en/02a-cube-notation.md) and must never change: every
/// move table and test fixture depends on them.
///
/// t_cube.corner_perm is indexed BY SLOT and stores one of these as its
/// VALUE. corner_perm[CORNER_UFL] == CORNER_URF means "the URF piece
/// currently sits in the UFL slot". On a solved cube, corner_perm[i] == i.
typedef enum e_corner
{
	CORNER_URF,
	CORNER_UFL,
	CORNER_ULB,
	CORNER_UBR,
	CORNER_DFR,
	CORNER_DLF,
	CORNER_DBL,
	CORNER_DRB
}	t_corner;

/// Identity of each of the 12 edge pieces (which piece, not which slot).
///
/// Same idea as t_corner. Names are the two faces the edge touches: UR is
/// the Up-Right edge. Order follows Kociemba's convention; do not reorder.
typedef enum e_edge
{
	EDGE_UR,
	EDGE_UF,
	EDGE_UL,
	EDGE_UB,
	EDGE_DR,
	EDGE_DF,
	EDGE_DL,
	EDGE_DB,
	EDGE_FR,
	EDGE_FL,
	EDGE_BL,
	EDGE_BR
}	t_edge;

/// The 18 legal moves: 6 faces x {clockwise, 180, counter-clockwise}.
///
/// The suffix says the turn: 1 = 90 deg clockwise ("R"), 2 = 180 deg
/// ("R2"), 3 = 90 deg counter-clockwise ("R'"). Faces are ordered
/// U R F D L B, so for any move: move / 3 is the face index and
/// move % 3 is the turn (0, 1, 2).
///
/// Kept as ONE flat enum (not a {face, turn} pair) so later code can index
/// move tables directly: table[coord][move]. There is no value for
/// M, E, S, x, y, z, so they cannot be represented at all.
///
/// MOVE_COUNT is not a move, it is how many there are (18).
typedef enum e_move
{
	MOVE_U1,
	MOVE_U2,
	MOVE_U3,
	MOVE_R1,
	MOVE_R2,
	MOVE_R3,
	MOVE_F1,
	MOVE_F2,
	MOVE_F3,
	MOVE_D1,
	MOVE_D2,
	MOVE_D3,
	MOVE_L1,
	MOVE_L2,
	MOVE_L3,
	MOVE_B1,
	MOVE_B2,
	MOVE_B3,
	MOVE_COUNT
}	t_move;

/// Cubie model of a 3x3x3 cube: which piece sits in each slot, and how it
/// is turned. This is the authoritative representation the solver uses.
///
/// Centres are left out on purpose: they never move relative to each
/// other, so they carry no information.
///
/// - corner_perm[slot]:   corner piece in that slot (see t_corner).
/// - corner_orient[slot]: twist 0, 1 or 2. The 8 values always sum to
///                        0 mod 3.
/// - edge_perm[slot]:     edge piece in that slot (see t_edge).
/// - edge_orient[slot]:   flip 0 or 1. The 12 values always sum to 0 mod 2.
///
/// See docs/en/02a-cube-notation.md, section 4 (twist) and 5 (flip).
typedef struct s_cube
{
	t_corner	corner_perm[CORNER_COUNT];
	uint8_t		corner_orient[CORNER_COUNT];
	t_edge		edge_perm[EDGE_COUNT];
	uint8_t		edge_orient[EDGE_COUNT];
}	t_cube;

/// The solved cube: every piece in its own slot, every orientation 0.
///
/// Defined once in cube/cubie.c. Build tests and scrambles from a copy of
/// this, never from a zeroed struct: zeroing puts the URF corner in every
/// slot, which looks valid but is not a cube.
extern const t_cube	SOLVED_CUBE;

/// @brief True if both cubes have exactly the same state.
bool	cube_equal(const t_cube *a, const t_cube *b);

/// @brief True if the cube equals SOLVED_CUBE.
bool	cube_is_solved(const t_cube *cube);

/// @brief Sum of all corner twists, mod 3 (0 on a valid cube).
int		cube_corner_twist_sum(const t_cube *cube);

/// @brief Sum of all edge flips, mod 2 (0 on a valid cube).
int		cube_edge_flip_sum(const t_cube *cube);

/// @brief True if the cube can be reached by turning a real cube.
bool	cube_is_valid(const t_cube *cube);

/// Move table for one face: what ONE clockwise quarter turn of it does.
///
/// - corners[] / edges[]: the 4 slots that cycle, in cycle order. The
///   piece in slot [i] moves to slot [i + 1], and [3] wraps back to [0].
/// - corner_twist[] / edge_flip[]: amount added to the orientation of the
///   piece that ARRIVES in slot [i] (twist mod 3 for corners, flip mod 2
///   for edges).
///
/// The 6 actual tables (one per face) live in cube/moves.c.
typedef struct s_face_table
{
	t_corner	corners[4];
	int			corner_twist[4];
	t_edge		edges[4];
	int			edge_flip[4];
}	t_face_table;

/// @brief Applies one of the 18 moves to a cube, in place.
void	apply_move(t_cube *cube, t_move move);

#endif
