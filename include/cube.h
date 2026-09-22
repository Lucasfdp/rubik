/* ========================================================================
 * cube.h — the cubie model: t_cube, and the 26 movable-piece identities
 *
 * This is the authoritative model (docs/en/04-architecture.md, cube/cubie.c)
 * — the one solve/ actually reasons about. cube/facelet.c will convert to
 * and from the 54-sticker view for I/O and the renderer; nothing in solve/
 * should ever touch a facelet string directly.
 *
 * Naming and slot order here are NOT a free choice — they match Kociemba's
 * own convention exactly (docs/en/02a-cube-notation.md), and every
 * permutation array, move table, and test fixture in this project assumes
 * it. Write it into DECISIONS.md and never change it (05-roadmap-mandatory,
 * Sprint 0) — if you think you need to, re-read that file instead.
 * ======================================================================== */

#ifndef CUBE_H
# define CUBE_H

# include <stdint.h>
# include <stdbool.h>

# define CORNER_COUNT 8
# define EDGE_COUNT   12

/* --------------------------------------------------------------------
 * Corner identity — which physical piece, not which slot. corner_perm[]
 * below is indexed BY slot and stores one of these as its VALUE:
 *   corner_perm[CORNER_UFL] == CORNER_URF
 * means "the URF piece currently sits in the UFL slot." On a solved cube,
 * corner_perm[i] == i for every i.
 * -------------------------------------------------------------------- */
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

/* Same idea, for the 12 edges. */
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

/* --------------------------------------------------------------------
 * The 18 legal moves: 6 faces x {CW, 180, CCW}. Kept as ONE flat enum
 * (rather than a {face, turn} pair) because coord/movetable.c will index
 * straight into move tables with it: table[coord][move]. Face order is
 * U R F D L B — same order as the facelet string in 02a-cube-notation.md,
 * on purpose, so the two are easy to cross-check by eye.
 *
 *   MOVE_x1 = 90 degrees clockwise   (notation: "R")
 *   MOVE_x2 = 180 degrees            (notation: "R2")
 *   MOVE_x3 = 90 degrees counter-cw  (notation: "R'")
 *
 * parse/notation.c is what rejects M, E, S, x, y, z — this enum simply has
 * no slot for them, which is the structural half of that rule.
 * -------------------------------------------------------------------- */
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

/* --------------------------------------------------------------------
 * The cubie model itself. Centres are deliberately absent — they never
 * move relative to each other, so they carry no information (GLOSSARY.md,
 * "Cubie").
 *
 * corner_orient[slot] in {0, 1, 2} — see 02a-cube-notation.md §4 (twist).
 *   Sum of all 8, mod 3, is always 0 — assert it in every test.
 * edge_orient[slot]   in {0, 1}    — see 02a-cube-notation.md §5 (flip).
 *   Sum of all 12, mod 2, is always 0 — assert that too.
 * -------------------------------------------------------------------- */
typedef struct s_cube
{
	t_corner	corner_perm[CORNER_COUNT];
	uint8_t		corner_orient[CORNER_COUNT];
	t_edge		edge_perm[EDGE_COUNT];
	uint8_t		edge_orient[EDGE_COUNT];
}	t_cube;

/* The fixed starting point: corner_perm[i] == i, edge_perm[i] == i, every
 * orientation 0. Defined once in cube/cubie.c — build every test and every
 * "apply this scramble" path off a copy of this, never a hand-rolled zeroed
 * struct (an easy way to end up with something that only looks valid). */
extern const t_cube	SOLVED_CUBE;

/* --- cube/cubie.c — pure data-layer helpers, no move logic --------------
 * Implemented already: straightforward comparisons/sums over the arrays
 * above, nothing cube-solving-specific. */
bool	cube_equal(const t_cube *a, const t_cube *b);
bool	cube_is_solved(const t_cube *cube);
int		cube_corner_twist_sum(const t_cube *cube);	/* the mod-3 invariant */
int		cube_edge_flip_sum(const t_cube *cube);		/* the mod-2 invariant */
bool	cube_is_valid(const t_cube *cube);				/* both invariants + permutation parity */

/* --- cube/moves.c — Sprint 0's actual pairing task ----------------------
 * apply_move() is where the 18 move permutation tables live. Declared
 * here, deliberately NOT implemented: this is the part of Sprint 0
 * (05-roadmap-mandatory.md) meant to be worked out on a whiteboard
 * together, not generated for you. */
void	apply_move(t_cube *cube, t_move move);

#endif
