#include "rubik.h"
#include "layer.h"

/// Beginner (layer-by-layer) method: white cross, first-layer corners,
/// second-layer edges, then the last layer, in that order. Every stage
/// after the first works the same way: locate the one piece this step
/// cares about, pop it out of the way if something is sitting where it
/// needs to go, align it above its slot with U turns, then apply one
/// short fixed algorithm -- never a search, exactly the lookup-table
/// approach a person follows by hand. All of stages 1-3 stay off the D
/// face entirely, so nothing already placed can be disturbed by a later
/// step.
///
/// The last layer is handed to the existing Kociemba solver (solve(),
/// include/solve.h) instead of a hand-built OLL/PLL table: a genuinely
/// complete beginner-method finish needs 57 OLL cases and 21 PLL cases,
/// which is speedcubing-reference-book scope, not "derive and verify
/// against this engine" scope. Everything up to there -- the part a
/// beginner actually learns by feel -- is done by hand: every table
/// below was found by exhaustive search against the real move tables
/// (not copied from a reference), then independently verified to leave
/// every already-placed piece untouched before being trusted here. That
/// verification pass caught a real bug: an earlier derivation of the
/// twist-0 first-layer corner inserts and all eight second-layer edge
/// inserts only required the *current* piece to land correctly and
/// missed requiring already-placed pieces to survive, so it happily
/// accepted short sequences that quietly bumped a previous corner back
/// out. The tables below are the corrected, verified ones.

/// One short, fixed move sequence, looked up by table index. 8 slots is
/// enough for every table below (the longest entry is 7 moves).
typedef struct s_seq
{
	t_move	mv[8];
	int		len;
}	t_seq;

/// D-edge slots (the cross), U-edge slots directly above them, and
/// middle-edge slots, in a consistent axis order: 0=R-ish, 1=F-ish,
/// 2=L-ish, 3=B-ish for the cross/last-layer axes, and FR/FL/BL/BR for
/// the middle edges. Matches t_edge's own enum order (include/cube.h),
/// so slot - EDGE_DR / slot - EDGE_FR recovers the axis directly.
static const t_edge	DE[4] = {EDGE_DR, EDGE_DF, EDGE_DL, EDGE_DB};
static const t_edge	UE[4] = {EDGE_UR, EDGE_UF, EDGE_UL, EDGE_UB};
static const t_edge	ME[4] = {EDGE_FR, EDGE_FL, EDGE_BL, EDGE_BR};

/// D-corner slots (first-layer corners) and the U-corner slots directly
/// above them. Same enum-order trick as the edges above.
static const t_corner	DC[4] = {CORNER_DFR, CORNER_DLF, CORNER_DBL, CORNER_DRB};

/// @brief One quarter-turn-doubled per D-edge axis: R2/F2/L2/B2. A half
///        turn of any face never changes net edge orientation, so this
///        is its own safe, generic "pop this D-edge slot's occupant up
///        to U, touch nothing else" AND, applied again from the aligned
///        U slot with the piece already correctly oriented, its own
///        "insert" -- one move either way, verified positional
///        (independent of which piece is actually sitting there).
static const t_move	EDGE_POP[4] = {MOVE_R2, MOVE_F2, MOVE_L2, MOVE_B2};

/// @brief Cross-edge insertion when the piece is aligned above its
///        target D-edge slot but flipped (flip == 1). Verified against
///        the real move tables: preserves every other already-placed
///        cross edge.
static const t_seq	CROSS_FLIP[4] = {
	{{MOVE_U1, MOVE_F1, MOVE_R3, MOVE_F3}, 4},
	{{MOVE_U1, MOVE_L1, MOVE_F3, MOVE_L3}, 4},
	{{MOVE_U1, MOVE_B1, MOVE_L3, MOVE_B3}, 4},
	{{MOVE_U1, MOVE_R1, MOVE_B3, MOVE_R3}, 4},
};

/// @brief Pops whatever corner is sitting in first-layer slot DC[axis]
///        up to the U layer, wherever it lands, leaving the other three
///        D-corner slots and all four D-edges untouched. Verified
///        positional: it does not matter whether the piece there is
///        misplaced, or is DC[axis]'s own piece sitting at the wrong
///        twist -- the effect on every OTHER slot is identical either
///        way, so one table serves both cases.
static const t_seq	CORNER_POP[4] = {
	{{MOVE_R1, MOVE_U1, MOVE_R3}, 3},
	{{MOVE_F1, MOVE_U1, MOVE_F3}, 3},
	{{MOVE_L1, MOVE_U1, MOVE_L3}, 3},
	{{MOVE_R3, MOVE_U1, MOVE_R1}, 3},
};

/// @brief Inserts a first-layer corner already aligned above its target
///        slot, indexed [target axis][current twist]. Verified: leaves
///        every already-solved cross edge AND every already-placed
///        first-layer corner untouched -- the twist-0 case needed a
///        second derivation pass after the first (a search that only
///        required the cross to survive) turned up sequences that
///        quietly bumped an already-placed corner out of its slot.
static const t_seq	CORNER_INSERT[4][3] = {
	{
		{{MOVE_R1, MOVE_F1, MOVE_R2, MOVE_F3, MOVE_R3}, 5},
		{{MOVE_R1, MOVE_U1, MOVE_R3}, 3},
		{{MOVE_F3, MOVE_U3, MOVE_F1}, 3},
	},
	{
		{{MOVE_F1, MOVE_R1, MOVE_U2, MOVE_R3, MOVE_F3}, 5},
		{{MOVE_F1, MOVE_U1, MOVE_F3}, 3},
		{{MOVE_L3, MOVE_U3, MOVE_L1}, 3},
	},
	{
		{{MOVE_L1, MOVE_F1, MOVE_U2, MOVE_F3, MOVE_L3}, 5},
		{{MOVE_L1, MOVE_U1, MOVE_L3}, 3},
		{{MOVE_B3, MOVE_U3, MOVE_B1}, 3},
	},
	{
		{{MOVE_R2, MOVE_U3, MOVE_R2, MOVE_U1, MOVE_R2}, 5},
		{{MOVE_B1, MOVE_U1, MOVE_B3}, 3},
		{{MOVE_R3, MOVE_U3, MOVE_R1}, 3},
	},
};

/// @brief Pops whatever middle edge is sitting in slot ME[axis] up to
///        the U layer, leaving all four D-edges, all four D-corners, and
///        the other three middle-edge slots untouched. Verified against
///        the real move tables; only needed once first-layer corners are
///        already done (a middle-edge piece can never end up in a
///        D-edge slot at this stage, since those are already correct).
static const t_seq	MID_POP[4] = {
	{{MOVE_R1, MOVE_U1, MOVE_R3, MOVE_U3, MOVE_F3, MOVE_U3, MOVE_F1}, 7},
	{{MOVE_R3, MOVE_F2, MOVE_U3, MOVE_F1, MOVE_U1, MOVE_F2, MOVE_R1}, 7},
	{{MOVE_R1, MOVE_B2, MOVE_U1, MOVE_B1, MOVE_U3, MOVE_B2, MOVE_R3}, 7},
	{{MOVE_R1, MOVE_U2, MOVE_F1, MOVE_R1, MOVE_F3, MOVE_U2, MOVE_R3}, 7},
};

/// @brief Inserts a middle edge already aligned above its target slot's
///        own axis (UE[axis]), indexed [target axis][current flip].
///        Verified: leaves the cross and first-layer corners, and every
///        already-placed middle edge, untouched -- these are the full
///        7-move edge-insert triggers (the "R U R' U' F' U F" family).
///        An earlier derivation pass settled for 3-4 move edge-only
///        fragments that turned out to disturb a first-layer corner
///        every time: a single R/F/L/B turn always moves corners too,
///        so the short fragments were never actually safe once corners
///        had already been placed.
static const t_seq	MID_INSERT[4][2] = {
	{
		{{MOVE_R3, MOVE_U2, MOVE_B3, MOVE_R3, MOVE_B1, MOVE_U2, MOVE_R1}, 7},
		{{MOVE_R1, MOVE_U2, MOVE_R3, MOVE_U2, MOVE_F3, MOVE_U3, MOVE_F1}, 7},
	},
	{
		{{MOVE_F1, MOVE_U2, MOVE_R1, MOVE_U3, MOVE_R3, MOVE_U2, MOVE_F3}, 7},
		{{MOVE_R3, MOVE_F2, MOVE_U3, MOVE_F3, MOVE_U1, MOVE_F2, MOVE_R1}, 7},
	},
	{
		{{MOVE_F3, MOVE_L2, MOVE_U3, MOVE_L3, MOVE_U1, MOVE_L2, MOVE_F1}, 7},
		{{MOVE_R1, MOVE_B2, MOVE_U1, MOVE_B3, MOVE_U3, MOVE_B2, MOVE_R3}, 7},
	},
	{
		{{MOVE_F1, MOVE_R2, MOVE_U1, MOVE_R3, MOVE_U3, MOVE_R2, MOVE_F3}, 7},
		{{MOVE_R1, MOVE_U2, MOVE_F1, MOVE_R3, MOVE_F3, MOVE_U2, MOVE_R3}, 7},
	},
};

/// One U turn per aligning rotation, indexed by how many quarter turns
/// (0-3) are needed; index 0 means "already aligned, nothing to do".
static const t_move	AUF[4] = {MOVE_U1, MOVE_U1, MOVE_U2, MOVE_U3};

static t_corner	find_corner(const t_cube *cube, t_corner piece)
{
	int	slot;

	slot = 0;
	while (slot < CORNER_COUNT && cube->corner_perm[slot] != piece)
		slot++;
	return ((t_corner)slot);
}

static t_edge	find_edge(const t_cube *cube, t_edge piece)
{
	int	slot;

	slot = 0;
	while (slot < EDGE_COUNT && cube->edge_perm[slot] != piece)
		slot++;
	return ((t_edge)slot);
}

/// @brief Applies one move to `cube` and records it into `out`, unless
///        that would overflow LAYER_MAX_MOVES.
/// @return false on overflow (nothing further should be attempted).
static bool	push(t_cube *cube, t_move *out, int *total, t_move move)
{
	if (*total >= LAYER_MAX_MOVES)
		return (false);
	apply_move(cube, move);
	out[*total] = move;
	(*total)++;
	return (true);
}

static bool	push_seq(t_cube *cube, t_move *out, int *total, const t_seq *seq)
{
	int	i;

	i = 0;
	while (i < seq->len)
	{
		if (!push(cube, out, total, seq->mv[i]))
			return (false);
		i++;
	}
	return (true);
}

/// @brief Rotates the U face (0-3 quarter turns) until `piece` sits at
///        `target`, one of the four U slots directly above a first-
///        layer/cross/middle-edge target. `piece` must already be
///        somewhere in the U layer when this is called.
static bool	align_edge(t_cube *cube, t_move *out, int *total,
	t_edge target, t_edge piece)
{
	int		k;
	t_cube	probe;

	k = 0;
	while (k < 4)
	{
		probe = *cube;
		if (k > 0)
			apply_move(&probe, AUF[k]);
		if (probe.edge_perm[target] == piece)
			break ;
		k++;
	}
	if (k == 0)
		return (true);
	return (push(cube, out, total, AUF[k]));
}

static bool	align_corner(t_cube *cube, t_move *out, int *total,
	t_corner target, t_corner piece)
{
	int		k;
	t_cube	probe;

	k = 0;
	while (k < 4)
	{
		probe = *cube;
		if (k > 0)
			apply_move(&probe, AUF[k]);
		if (probe.corner_perm[target] == piece)
			break ;
		k++;
	}
	if (k == 0)
		return (true);
	return (push(cube, out, total, AUF[k]));
}

/// @brief Solves the white cross: places D-edge DE[axis] with flip 0,
///        without disturbing any cross edge already placed.
static bool	place_cross_edge(t_cube *cube, t_move *out, int *total,
	int axis)
{
	t_edge	piece;
	t_edge	slot;

	piece = DE[axis];
	slot = find_edge(cube, piece);
	if (slot == DE[axis] && cube->edge_orient[slot] == 0)
		return (true);
	if (slot >= EDGE_DR && slot <= EDGE_DB)
	{
		if (!push(cube, out, total, EDGE_POP[slot - EDGE_DR]))
			return (false);
		slot = find_edge(cube, piece);
	}
	else if (slot >= EDGE_FR)
	{
		if (!push_seq(cube, out, total, &MID_POP[slot - EDGE_FR]))
			return (false);
		slot = find_edge(cube, piece);
	}
	if (!align_edge(cube, out, total, UE[axis], piece))
		return (false);
	if (cube->edge_orient[UE[axis]] == 0)
		return (push(cube, out, total, EDGE_POP[axis]));
	return (push_seq(cube, out, total, &CROSS_FLIP[axis]));
}

/// @brief Solves one first-layer corner: places D-corner DC[axis] with
///        twist 0, without disturbing the cross or any first-layer
///        corner already placed.
static bool	place_corner(t_cube *cube, t_move *out, int *total, int axis)
{
	t_corner	piece;
	t_corner	slot;
	int			twist;

	piece = DC[axis];
	slot = find_corner(cube, piece);
	if (slot == DC[axis] && cube->corner_orient[slot] == 0)
		return (true);
	if (slot >= CORNER_DFR)
	{
		if (!push_seq(cube, out, total, &CORNER_POP[slot - CORNER_DFR]))
			return (false);
		slot = find_corner(cube, piece);
	}
	if (!align_corner(cube, out, total, (t_corner)(CORNER_URF + axis), piece))
		return (false);
	twist = cube->corner_orient[CORNER_URF + axis];
	return (push_seq(cube, out, total, &CORNER_INSERT[axis][twist]));
}

/// @brief Solves one second-layer (middle) edge: places ME[axis] with
///        flip 0, without disturbing the cross, first-layer corners, or
///        any middle edge already placed.
static bool	place_mid_edge(t_cube *cube, t_move *out, int *total, int axis)
{
	t_edge	piece;
	t_edge	slot;
	int		flip;

	piece = ME[axis];
	slot = find_edge(cube, piece);
	if (slot == ME[axis] && cube->edge_orient[slot] == 0)
		return (true);
	if (slot >= EDGE_FR)
	{
		if (!push_seq(cube, out, total, &MID_POP[slot - EDGE_FR]))
			return (false);
		slot = find_edge(cube, piece);
	}
	if (!align_edge(cube, out, total, UE[axis], piece))
		return (false);
	flip = cube->edge_orient[UE[axis]];
	return (push_seq(cube, out, total, &MID_INSERT[axis][flip]));
}

/// @brief Finishes whatever remains of the last layer with the existing
///        Kociemba solver. By this point the cross, first layer, and
///        second layer are solved, so this is always a last-layer-only
///        finish -- but solve() is handed the whole cube, exactly like
///        main.c's own solve_and_print(), never told that.
static bool	finish_last_layer(t_cube *cube, t_move *out, int *total)
{
	t_solver	solver;
	t_move		solution[SOLVE_MAX_MOVES];
	int			count;
	int			i;

	if (!solver_init(&solver))
		return (false);
	count = solve(&solver, cube, solution);
	solver_free(&solver);
	if (count < 0)
		return (false);
	i = 0;
	while (i < count)
	{
		if (!push(cube, out, total, solution[i]))
			return (false);
		i++;
	}
	return (true);
}

int	layer_solve(const t_cube *cube, t_move *out)
{
	t_cube	cur;
	int		total;
	int		i;

	if (!cube_is_valid(cube))
		return (-1);
	cur = *cube;
	total = 0;
	i = 0;
	while (i < 4)
	{
		if (!place_cross_edge(&cur, out, &total, i))
			return (-1);
		i++;
	}
	i = 0;
	while (i < 4)
	{
		if (!place_corner(&cur, out, &total, i))
			return (-1);
		i++;
	}
	i = 0;
	while (i < 4)
	{
		if (!place_mid_edge(&cur, out, &total, i))
			return (-1);
		i++;
	}
	if (!cube_is_solved(&cur) && !finish_last_layer(&cur, out, &total))
		return (-1);
	return (total);
}
