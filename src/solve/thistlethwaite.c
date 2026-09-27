#include <stdlib.h>
#include <string.h>
#include "thistlethwaite.h"
#include "solve.h"

/// The dummy coordinate for phase 1 (docs/en/10-thistlethwaite-spec.md §4):
/// one value, and every move keeps it at that one value. All-zero static
/// storage, so no malloc is needed for it.
static const uint16_t	DUMMY_MOVES[MOVE_COUNT];

/// @brief Phase 1: G0 -> G1 (flip == 0), all 18 moves.
///
/// flip is the only real coordinate, so it is duplicated into BOTH
/// coordinate slots 0 and 1, paired against the dummy in slot 2.
/// ida_estimate() then reads the same flip_dummy table twice and takes the
/// max of two identical numbers: the correct answer, with no changes
/// needed to ida.c or prune.h (docs/en/10-thistlethwaite-spec.md §4).
static void	thistle_phase1_setup(t_ida_phase *phase,
	const t_move_tables *moves, const t_thistle_tables *prune)
{
	phase->move_table[0] = moves->flip;
	phase->move_table[1] = moves->flip;
	phase->move_table[2] = DUMMY_MOVES;
	phase->prune_xz = &prune->flip_dummy;
	phase->prune_yz = &prune->flip_dummy;
	phase->moves = MOVES_ALL;
	phase->max_depth = THISTLE_PHASE1_CAP;
}

/// @brief Phase 2: G1 -> G2 (twist == 0 && slice == 0), G1's 14 moves.
///
/// Same duplicate-slot trick as phase 1, this time pairing twist (in slots
/// 0 and 1) against slice (in slot 2) through the one twist_slice table:
/// there is only one real pair here, not two tables to take the max of.
static void	thistle_phase2_setup(t_ida_phase *phase,
	const t_move_tables *moves, const t_thistle_tables *prune)
{
	phase->move_table[0] = moves->twist;
	phase->move_table[1] = moves->twist;
	phase->move_table[2] = moves->slice;
	phase->prune_xz = &prune->twist_slice;
	phase->prune_yz = &prune->twist_slice;
	phase->moves = MOVES_G1;
	phase->max_depth = THISTLE_PHASE2_CAP;
}

/// @brief Phase 3: G2 -> G3 (cperm/eperm/sperm all inside G3), G2's 10
///        moves (== MOVES_PHASE2). Same three-coordinate, two-table shape
///        as Kociemba's own phase 2 (include/ida.h), just with the coset
///        tables built by thistle_init() and a set goal instead of (0,0).
static void	thistle_phase3_setup(t_ida_phase *phase,
	const t_move_tables *moves, const t_thistle_tables *prune)
{
	phase->move_table[0] = moves->cperm;
	phase->move_table[1] = moves->eperm;
	phase->move_table[2] = moves->sperm;
	phase->prune_xz = &prune->cperm_sperm3;
	phase->prune_yz = &prune->eperm_sperm3;
	phase->moves = MOVES_G2;
	phase->max_depth = THISTLE_PHASE3_CAP;
}

/// @brief Phase 4: G3 -> solved, G3's 6 half turns.
static void	thistle_phase4_setup(t_ida_phase *phase,
	const t_move_tables *moves, const t_thistle_tables *prune)
{
	phase->move_table[0] = moves->cperm;
	phase->move_table[1] = moves->eperm;
	phase->move_table[2] = moves->sperm;
	phase->prune_xz = &prune->cperm_sperm4;
	phase->prune_yz = &prune->eperm_sperm4;
	phase->moves = MOVES_G3;
	phase->max_depth = THISTLE_PHASE4_CAP;
}

/// @brief Phase 1's start state: flip, duplicated, plus the dummy's one
///        value (0).
static void	thistle_phase1_start(const t_cube *cube, uint16_t start[3])
{
	start[0] = encode_flip(cube);
	start[1] = start[0];
	start[2] = 0;
}

/// @brief Phase 2's start state: twist, duplicated, plus slice.
static void	thistle_phase2_start(const t_cube *cube, uint16_t start[3])
{
	start[0] = encode_twist(cube);
	start[1] = start[0];
	start[2] = encode_slice(cube);
}

/// Phase 3 and phase 4 both read (cperm, eperm, sperm) — exactly what
/// ida_phase2_start() (include/ida.h) already computes for Kociemba, so
/// both phases reuse it instead of a new thistle-specific function.

/// @brief Frees the three reachable-set arrays. free() on NULL is safe, so
///        this is safe whether or not thistle_init() got this far.
static void	free_goal_sets(t_thistle_tables *prune)
{
	free(prune->cp_g3);
	free(prune->ep_g3);
	free(prune->sp_g3);
	prune->cp_g3 = NULL;
	prune->ep_g3 = NULL;
	prune->sp_g3 = NULL;
}

void	thistle_free(t_thistle_solver *solver)
{
	prune_free(&solver->prune.flip_dummy);
	prune_free(&solver->prune.twist_slice);
	prune_free(&solver->prune.cperm_sperm3);
	prune_free(&solver->prune.eperm_sperm3);
	prune_free(&solver->prune.cperm_sperm4);
	prune_free(&solver->prune.eperm_sperm4);
	free_goal_sets(&solver->prune);
}

/// G3's members: every cperm / eperm / sperm value reachable from solved
/// using only the 6 half turns (docs/en/10-thistlethwaite-spec.md §3).
/// These are what phase 3 aims for, and phase 4's own tables (built with
/// the same MOVES_G3) never need to look outside them.
static bool	build_goal_sets(t_thistle_tables *prune,
	const t_move_tables *moves)
{
	prune->cp_g3 = reachable_set(moves->cperm, CPERM_COUNT, MOVES_G3);
	prune->ep_g3 = reachable_set(moves->eperm, EPERM_COUNT, MOVES_G3);
	prune->sp_g3 = reachable_set(moves->sperm, SPERM_COUNT, MOVES_G3);
	return (prune->cp_g3 && prune->ep_g3 && prune->sp_g3);
}

bool	thistle_init(t_thistle_solver *solver, const t_move_tables *moves)
{
	t_thistle_tables	*prune;

	memset(solver, 0, sizeof(*solver));
	solver->moves = moves;
	prune = &solver->prune;
	if (!build_goal_sets(prune, moves)
		|| !prune_build(&prune->flip_dummy, moves->flip, FLIP_COUNT,
			DUMMY_MOVES, 1, MOVES_ALL)
		|| !prune_build(&prune->twist_slice, moves->twist, TWIST_COUNT,
			moves->slice, SLICE_COUNT, MOVES_G1)
		|| !prune_build_coset(&prune->cperm_sperm3, moves->cperm,
			CPERM_COUNT, prune->cp_g3, moves->sperm, SPERM_COUNT,
			prune->sp_g3, MOVES_G2)
		|| !prune_build_coset(&prune->eperm_sperm3, moves->eperm,
			EPERM_COUNT, prune->ep_g3, moves->sperm, SPERM_COUNT,
			prune->sp_g3, MOVES_G2)
		|| !prune_build(&prune->cperm_sperm4, moves->cperm, CPERM_COUNT,
			moves->sperm, SPERM_COUNT, MOVES_G3)
		|| !prune_build(&prune->eperm_sperm4, moves->eperm, EPERM_COUNT,
			moves->sperm, SPERM_COUNT, MOVES_G3))
	{
		thistle_free(solver);
		return (false);
	}
	thistle_phase1_setup(&solver->phase1, moves, prune);
	thistle_phase2_setup(&solver->phase2, moves, prune);
	thistle_phase3_setup(&solver->phase3, moves, prune);
	thistle_phase4_setup(&solver->phase4, moves, prune);
	return (true);
}

/// @brief Runs one phase and appends its moves to `out` at `offset`.
///
/// @return The phase's move count, or -1 if it found no solution.
static int	run_phase(const t_ida_phase *phase, const uint16_t start[3],
	t_move *out, int offset)
{
	t_move	path[IDA_MAX_DEPTH];
	int		length;

	length = ida_search(phase, start, path);
	if (length < 0)
		return (-1);
	memcpy(out + offset, path, (size_t)length * sizeof(t_move));
	return (length);
}

/// Chains all four phases on one working cube, exactly like solve()
/// (include/solve.h) chains Kociemba's two: search, apply the result,
/// re-encode the next phase's coordinates from the moved cube, repeat.
int	thistle_solve(const t_thistle_solver *solver, const t_cube *cube,
	t_move *out)
{
	t_cube		work;
	uint16_t	start[3];
	int			total;
	int			length;

	if (!cube_is_valid(cube))
		return (-1);
	work = *cube;
	total = 0;
	thistle_phase1_start(&work, start);
	length = run_phase(&solver->phase1, start, out, total);
	if (length < 0)
		return (-1);
	total += length;
	cube_apply_moves(&work, out + total - length, (size_t)length);
	thistle_phase2_start(&work, start);
	length = run_phase(&solver->phase2, start, out, total);
	if (length < 0)
		return (-1);
	total += length;
	cube_apply_moves(&work, out + total - length, (size_t)length);
	ida_phase2_start(&work, start);
	length = run_phase(&solver->phase3, start, out, total);
	if (length < 0)
		return (-1);
	total += length;
	cube_apply_moves(&work, out + total - length, (size_t)length);
	ida_phase2_start(&work, start);
	length = run_phase(&solver->phase4, start, out, total);
	if (length < 0)
		return (-1);
	total += length;
	return (simplify_moves(out, total));
}
