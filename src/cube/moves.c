#include "cube.h"

/// The 6 face tables: one clockwise quarter turn of each face.
///
/// Indexed U, R, F, D, L, B, the same face order used everywhere else
/// (FACES in parse/notation.c, t_move), so move / 3 picks the right row
/// with no lookup. Field meaning: see t_face_table in cube.h.
///
/// These values were NOT typed from memory. They come from simulating the
/// cube in 3D and were checked with these tests: every face turned 4 times
/// gives solved; (R U R' U')^6 and sune ^6 give solved; a 10,000-move
/// random scramble keeps twist sum % 3 == 0 and flip sum % 2 == 0 after
/// every move, and undoing it gives solved. A wrong sign here is a silent
/// bug (the solver never ends, or ends on garbage), so do not edit a value
/// without re-running checks like these.
///
/// Rules worth remembering (docs/en/02a-cube-notation.md, section 5):
///   - F and B quarter turns flip all 4 edges they move; U, D, L, R never
///     flip any edge.
///   - R, L, F, B twist corners (alternating +1/+2 around the face); U and
///     D never twist any corner.
static const t_face_table	FACE_TABLES[6] = {
	/// U (up)
	[0] = {
		.corners = {CORNER_UBR, CORNER_URF, CORNER_UFL, CORNER_ULB},
		.corner_twist = {0, 0, 0, 0},
		.edges = {EDGE_UB, EDGE_UR, EDGE_UF, EDGE_UL},
		.edge_flip = {0, 0, 0, 0}
	},
	/// R (right)
	[1] = {
		.corners = {CORNER_DFR, CORNER_URF, CORNER_UBR, CORNER_DRB},
		.corner_twist = {1, 2, 1, 2},
		.edges = {EDGE_BR, EDGE_DR, EDGE_FR, EDGE_UR},
		.edge_flip = {0, 0, 0, 0}
	},
	/// F (front)
	[2] = {
		.corners = {CORNER_DFR, CORNER_DLF, CORNER_UFL, CORNER_URF},
		.corner_twist = {2, 1, 2, 1},
		.edges = {EDGE_DF, EDGE_FL, EDGE_UF, EDGE_FR},
		.edge_flip = {1, 1, 1, 1}
	},
	/// D (down)
	[3] = {
		.corners = {CORNER_DBL, CORNER_DLF, CORNER_DFR, CORNER_DRB},
		.corner_twist = {0, 0, 0, 0},
		.edges = {EDGE_DB, EDGE_DL, EDGE_DF, EDGE_DR},
		.edge_flip = {0, 0, 0, 0}
	},
	/// L (left)
	[4] = {
		.corners = {CORNER_DBL, CORNER_ULB, CORNER_UFL, CORNER_DLF},
		.corner_twist = {1, 2, 1, 2},
		.edges = {EDGE_BL, EDGE_UL, EDGE_FL, EDGE_DL},
		.edge_flip = {0, 0, 0, 0}
	},
	/// B (back)
	[5] = {
		.corners = {CORNER_DBL, CORNER_DRB, CORNER_UBR, CORNER_ULB},
		.corner_twist = {2, 1, 2, 1},
		.edges = {EDGE_BL, EDGE_DB, EDGE_BR, EDGE_UB},
		.edge_flip = {1, 1, 1, 1}
	}
};

/// @brief Applies one clockwise step of a 4-cycle to the corners.
///
/// The piece in slots[i], with its orientation, moves to slots[i + 1];
/// slots[3] wraps around to slots[0]. The shift runs backwards (i = 3 down
/// to 1) so it works in place with a single saved slot instead of a whole
/// second array. After the shift, twist[i] is added (mod 3) to the piece
/// that just arrived in slots[i].
///
/// @param cube  Modified in place.
/// @param slots The 4 corner slots of the cycle, in cycle order.
/// @param twist Twist added per DESTINATION slot (0, 1 or 2).
static void	rotate_corners(t_cube *cube, const t_corner slots[4],
	const int twist[4])
{
	t_corner	last_perm;
	uint8_t		last_orient;
	int			i;

	last_perm = cube->corner_perm[slots[3]];
	last_orient = cube->corner_orient[slots[3]];
	i = 3;
	while (i > 0)
	{
		cube->corner_perm[slots[i]] = cube->corner_perm[slots[i - 1]];
		cube->corner_orient[slots[i]] = cube->corner_orient[slots[i - 1]];
		i--;
	}
	cube->corner_perm[slots[0]] = last_perm;
	cube->corner_orient[slots[0]] = last_orient;
	i = 0;
	while (i < 4)
	{
		cube->corner_orient[slots[i]]
			= (cube->corner_orient[slots[i]] + twist[i]) % 3;
		i++;
	}
}

/// @brief Applies one clockwise step of a 4-cycle to the edges.
///
/// Same shape as rotate_corners(), with flip mod 2 instead of twist mod 3.
/// Kept as two short functions instead of one generic (void *) version:
/// t_corner and t_edge are different types, and two obviously-correct
/// functions beat one clever one.
///
/// @param cube  Modified in place.
/// @param slots The 4 edge slots of the cycle, in cycle order.
/// @param flip  Flip added per DESTINATION slot (0 or 1).
static void	rotate_edges(t_cube *cube, const t_edge slots[4],
	const int flip[4])
{
	t_edge		last_perm;
	uint8_t		last_orient;
	int			i;

	last_perm = cube->edge_perm[slots[3]];
	last_orient = cube->edge_orient[slots[3]];
	i = 3;
	while (i > 0)
	{
		cube->edge_perm[slots[i]] = cube->edge_perm[slots[i - 1]];
		cube->edge_orient[slots[i]] = cube->edge_orient[slots[i - 1]];
		i--;
	}
	cube->edge_perm[slots[0]] = last_perm;
	cube->edge_orient[slots[0]] = last_orient;
	i = 0;
	while (i < 4)
	{
		cube->edge_orient[slots[i]]
			= (cube->edge_orient[slots[i]] + flip[i]) % 2;
		i++;
	}
}

/// @brief Applies one of the 18 moves to a cube, in place.
///
/// move / 3 picks the face's table in FACE_TABLES. move % 3 is 0, 1 or 2
/// for clockwise / 180 / counter-clockwise, so + 1 is how many times to
/// apply the clockwise table: R2 is R done twice, R' is R done three
/// times. There are no separate 180 or counter-clockwise tables to keep in
/// sync.
///
/// @param cube Modified in place.
/// @param move Any value below MOVE_COUNT.
void	apply_move(t_cube *cube, t_move move)
{
	const t_face_table	*table;
	int					turns;
	int					i;

	table = &FACE_TABLES[move / 3];
	turns = move % 3 + 1;
	i = 0;
	while (i < turns)
	{
		rotate_corners(cube, table->corners, table->corner_twist);
		rotate_edges(cube, table->edges, table->edge_flip);
		i++;
	}
}
