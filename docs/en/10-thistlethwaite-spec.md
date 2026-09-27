# Thistlethwaite's Four-Phase Algorithm — Implementation Spec

*(Part of the Rubik prep docs — see `00-overview.md` for the index. This is the handoff spec for the second-algorithm bonus scoped in `02-algorithms.md` §3.B, `06-roadmap-bonus.md` item 3, and `08-risks-and-open-decisions.md` #1.)*

This spec grounds Thistlethwaite in the code that already exists (`include/movetable.h`, `include/prune.h`, `include/ida.h`, `include/solve.h`) rather than deriving it from scratch. Every coordinate below either reuses a Kociemba table unchanged, or reuses its move mask exactly. Read `02-algorithms.md` §3.B first for the conceptual picture; this file is the "how to actually build it" follow-up — with one correction to that picture, below.

## 0. [CORRECTION] The group chain in `02-algorithms.md` §3.B has the wrong axis restricted in G1 and G2

That doc writes `G1 = <U2, D2, L, R, F, B>` (U/D restricted to half-turn, L/R/F/B free) and `G2 = <U2, D2, L, R, F2, B2>`. Checked directly against this codebase's own move tables (`src/cube/moves.c`'s `FACE_TABLES`, which 20,000+ existing tests already trust), that's backwards for *this* coordinate convention:

- `encode_flip` is only changed by a face whose `edge_flip` isn't all zero — and per `FACE_TABLES`, that's F and B ("F and B quarter turns flip all 4 edges they move; U, D, L, R never flip any edge"). A quarter U or D turn can never break `flip == 0`; a quarter F or B turn always does, and F2/B2 flip each edge twice (net zero, mod 2) so they're safe.
- I confirmed this by direct simulation (every one of the 18 moves, from solved): exactly `{U1,U2,U3, R1,R2,R3, D1,D2,D3, L1,L2,L3, F2, B2}` — 14 moves — preserves `flip == 0`. That is `<U, D, L, R, F2, B2>`, not `<U2, D2, L, R, F, B>`.
- The same check for `twist == 0 && slice == 0` together (from solved, and again from a random state already at `twist=flip=slice=0`, to rule out a solved-cube coincidence) gives exactly `{U1,U2,U3, D1,D2,D3, R2, F2, L2, B2}` — 10 moves — which is `MOVES_PHASE2`, already in `include/movetable.h` and already the group this codebase's own Kociemba solver uses. That's `<U, D, L2, R2, F2, B2>`, not `<U2, D2, L, R, F2, B2>`.

Both versions have the right *move counts* (14, then 10) — only which axis is restricted is swapped, likely a labeling mismatch between whatever source `02-algorithms.md` was transcribed from and this project's own U/R/F/D/L/B convention. This spec uses the version verified against the actual code below. **Worth a one-line fix to `02-algorithms.md` §3.B** alongside the 1,024-vs-2,048 typo already flagged there — I haven't touched that file myself since you're actively editing it.

The corrected chain:

```
G0 = <U, D, L, R, F, B>          all 18 moves        — any scrambled cube
G1 = <U, D, L, R, F2, B2>        edge orientation fixed        (14 moves)
G2 = <U, D, L2, R2, F2, B2>      + corner orientation + slice  (10 moves = MOVES_PHASE2)
G3 = <U2, D2, L2, R2, F2, B2>    half turns only                 (6 moves)
G4 = {identity}                    solved
```

Phase *k* searches **from a G(k-1) state to a G(k) state, using only G(k-1)'s own moves** — any other move would immediately undo the property phase *k-1* just locked in.

One consequence of the correction: **G2 above is exactly Kociemba's own intermediate group.** Thistlethwaite's phase 3 (G2→G3) uses the same 10 moves, and the same `eperm`/`sperm` legal-move domain, that your Kociemba solver already has — no table rebuild needed anywhere in this spec.

**[OPEN QUESTION, unchanged]** `02-algorithms.md` §3.B also lists the phase-1 table size as 1,024. The `flip` coordinate this spec reuses is `FLIP_COUNT` = 2,048 (`2^11`, `include/coord.h`), the standard value in every source in `09-sources.md`. Treat 1,024 as a second typo in that doc, not a target to hit.

## 1. New move masks (`include/movetable.h`)

Only two new constants are needed — phase 3 reuses `MOVES_PHASE2` verbatim (§0):

```c
/// Phase 2's search mask: G1's own moves (docs/en/10-thistlethwaite-spec.md
/// §0). Any U, D, L or R turn is safe — none of them ever flip an edge.
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
/// every one of these columns — no table changes needed here either.
# define MOVES_G3 ( \
	MOVE_BIT(MOVE_U2) | MOVE_BIT(MOVE_D2) | MOVE_BIT(MOVE_L2) \
	| MOVE_BIT(MOVE_R2) | MOVE_BIT(MOVE_F2) | MOVE_BIT(MOVE_B2))
```

`thistlethwaite.h` (§7) should add `# define MOVES_G2 MOVES_PHASE2` as a naming alias only — pure readability, so a reader of the Thistlethwaite code doesn't have to already know the two groups coincide.

## 2. No table changes needed at all

Unlike the (withdrawn) earlier draft of this spec, **nothing in `src/coord/movetable.c` needs to change.** Every coordinate this spec uses is already built the right way:

| Phase | Coordinate(s) | Move table(s) | Search mask | Goal |
|---|---|---|---|---|
| 1 (G0→G1) | `flip` | `flip` (`MOVES_ALL`, already built) | `MOVES_ALL` | `flip == 0` |
| 2 (G1→G2) | `twist`, `slice` | `twist`, `slice` (`MOVES_ALL`, already built) | `MOVES_G1` | `twist == 0 && slice == 0` |
| 3 (G2→G3) | `cperm`, `eperm`, `sperm` | same three tables, unchanged | `MOVES_PHASE2` (= `MOVES_G2`) | `cperm ∈ CP_G3 && eperm ∈ EP_G3 && sperm ∈ SP_G3` (§3) |
| 4 (G3→solved) | `cperm`, `eperm`, `sperm` | same three tables, unchanged | `MOVES_G3` | `cperm == 0 && eperm == 0 && sperm == 0` |

`twist`/`slice`/`cperm` are built with `MOVES_ALL` (every column real) and `eperm`/`sperm` are built with `MOVES_PHASE2` (real for exactly the 10 moves phase 3 and phase 4 both need, since `MOVES_G3 ⊂ MOVES_PHASE2`). Phases 1, 2 and 4 are single-point goals — the shape `include/ida.h` already handles. Phase 3 is the one genuine addition: a **set** of goal states (a coset), not a single point.

## 3. Phase 3's coset: reachability sets and a multi-source pruning table

`G3` is a subgroup: every `cperm`, `eperm` and `sperm` value it can ever produce is reachable from solved using *only* `MOVES_G3` (the 6 half turns). So the goal set for phase 3 is defined operationally, not by a formula:

```c
/// Forward BFS from the solved value (0) using only `moves`. Reuses the
/// exact same layer-by-layer walk as prune_build(), just without pairing
/// a second coordinate. Marks every value reachable from 0.
///
/// @param table Move table of one coordinate (e.g. cperm).
/// @param count How many values that coordinate has.
/// @param moves Moves allowed during the walk (MOVES_G3 for this spec).
/// @return A malloc'd bool[count] the caller frees, or NULL on OOM.
bool	*reachable_set(const uint16_t *table, int count, t_move_mask moves);
```

Call this once at startup with `moves = MOVES_G3` to get `CP_G3 = reachable_set(cperm, CPERM_COUNT, MOVES_G3)`, and the same for `EP_G3`/`eperm` and `SP_G3`/`sperm`. **[SHOULD-FIX]** verify `SP_G3` isn't all 24 values (if it is, drop it from phase 3's goal test — a coordinate that's never restrictive adds a lookup for nothing).

The pruning table itself needs a **multi-source** BFS: instead of seeding cell `(0, 0)` at distance 0 (what `prune_build()` does today), seed *every* cell `(a, b)` where `goal_a[a] && goal_b[b]`. The rest of the algorithm — expand a whole layer, stop when nothing new is found — is identical to `prune_build()`, and it stays exact and admissible for the same reason the existing comment in `include/prune.h` gives: every move in `MOVES_PHASE2` has its own undo inside `MOVES_PHASE2` (U1/U3, D1/D3 undo each other; U2, R2, F2, L2, B2 are self-inverse), so "moves from any goal member to here" equals "moves from here to the nearest goal member."

```c
/// Same shape as prune_build(), except every cell where goal_a[a] &&
/// goal_b[b] starts at distance 0, instead of only cell (0, 0). Distances
/// are then still exact minimums, and still never overestimate, by the
/// same undo-move argument prune_build() already relies on.
bool	prune_build_coset(t_prune_table *table, const uint16_t *move_a,
			int count_a, const bool *goal_a, const uint16_t *move_b,
			int count_b, const bool *goal_b, t_move_mask moves);
```

Build `cperm_sperm` and `eperm_sperm` variants of this for phase 3 (mirroring the two-table max-of-two pattern `ida_estimate()` already uses), using search mask `MOVES_PHASE2`.

## 4. Phase 1's one-coordinate table: reuse, don't add a new shape

Phase 1 only has one real coordinate (`flip`), but `t_ida_phase` always carries two pruning tables and three coordinate slots. Rather than adding a 1-D special case to `prune.h`/`ida.h`, pair `flip` with a **dummy coordinate of size 1** (a move table that always returns 0, whatever the move):

- `move_table = { flip, flip, dummy }`
- `prune_xz = prune_yz = &flip_dummy_table` (built once with `prune_build(table, flip, FLIP_COUNT, dummy, 1, MOVES_ALL)`)

`ida_estimate()` then computes `prune_dist(flip, flip_value, 0)` twice and takes the max of two identical numbers — the correct answer, with zero changes to `ida.c` or `prune.h`.

## 5. Reuse vs. rebuild: pruning-table tightness

A pruning table built with *more* moves than a phase actually searches with is still **admissible** (it can only find shorter-or-equal paths, so it never overestimates) — just looser. Phase 2 (`twist × slice`) could reuse Kociemba's existing `twist_slice` table (built with `MOVES_ALL ⊇ MOVES_G1`) instead of rebuilding tight with `MOVES_G1`; phase 4 could similarly reuse Kociemba's `cperm_sperm`/`eperm_sperm` (built with `MOVES_PHASE2 ⊇ MOVES_G3`).

**Recommendation:** build fresh, tight tables for all four phases from the start. Unlike the withdrawn earlier draft, every one of these tables is now the *same size* as an existing Kociemba table (§2 — no coordinate got bigger), so there's no memory reason to reuse loosely, and a tight table gives IDA* a better heuristic for free. Reuse-for-speed-of-implementation is a fallback if the build order in §8 turns out to take longer than expected, not the default.

## 6. Coordinate size: raw reuse vs. Jaap's reduced coordinates

`02-algorithms.md` §3.B cites 29,400 and 663,552 as the phase-3 and phase-4 table sizes — those are Jaap Scherphuis's *reduced* coset coordinates (`09-sources.md`), which fold away symmetry this codebase's raw `cperm`/`eperm`/`sperm` (40,320 / 40,320 / 24 each) don't exploit.

This spec deliberately does **not** derive those reduced coordinates — the algebra is real group-theory work, not a coding task, and the raw coordinates already built are small enough:

| Table | Raw (this spec) | Jaap's reduced (not derived here) |
|---|---|---|
| `cperm_sperm` (phase 3 or 4) | 40,320 × 24 = 967,680 cells (~0.9 MB) | 29,400-ish (combined) |
| `eperm_sperm` (phase 3 or 4) | 40,320 × 24 = 967,680 cells (~0.9 MB) | — |

Total extra memory for all of Thistlethwaite's own tables (phase 1's flip-dummy table, phase 2's fresh twist×slice, phase 3's two coset tables, phase 4's fresh tables) stays under 5 MB, and every table still builds in well under a second — same BFS, same table sizes `prune_build()` already handles at ~0.3 s total for Kociemba's four. **[SHOULD-FIX later]** if defence wants to explain the exact 45-move worst-case bound from `02-algorithms.md`, that requires the reduced coordinates — flag it as a possible follow-up, not part of this pass.

## 7. New files

Mirroring `src/solve/ida.c` / `src/solve/solve.c`:

- `include/thistlethwaite.h`, `src/solve/thistlethwaite.c` — the four `t_ida_phase` setups (`thistle_phase1_setup` … `phase4_setup`), the `reachable_set()` helper call sites, the three goal-membership arrays, and a `thistle_solve()` mirroring `solve()`'s shape (validate → four `ida_search()` calls chained like `solve()` chains two → `simplify_moves()` on the joined result, which already handles any number of segments).
- `include/prune.h` / `src/coord/prune.c` — add `prune_build_coset()` and `reachable_set()` (§3).
- `include/movetable.h` — add `MOVES_G1`, `MOVES_G3` only (§1). No changes to `move_tables_build()`.
- `tests/test_thistlethwaite.c` — same shape as `tests/test_ida.c`: write a `cube_in_g1/g2/g3(cube)` checker per phase (analogous to `phase1_done()` in `tests/test_ida.c`) and apply every returned path to a real cube to confirm it lands in the next group; cross-check `reachable_set()` against a brute-force BFS on real cubes for a small case; confirm `prune_build_coset()` never overestimates on random walks; confirm the *four*-phase total stays inside the 45-move worst case (`02-algorithms.md`) over many random scrambles, the way `tests/test_solve.c` already does for Kociemba's ~23-move average.
- `Makefile`, `include/rubik.h`, `.gitignore` — wire `test_thistlethwaite` in, following the exact pattern already used for `test_movetable`/`test_prune`/`test_ida`/`test_solve` (one `TESTS` entry, one `*_SRCS`/`*_OBJS` pair, one `ALL_OBJS`/`TOTAL` addition, one build rule).

Out of scope for this spec: the CLI surface for "run both algorithms, print the shorter" is `02-algorithms.md` §3.E / `06-roadmap-bonus.md` item 3 — a separate, small decision once both solvers exist.

## 8. Build order

1. §1 (the two new masks) — no behavior change, just constants; `MOVES_G2` alias in `thistlethwaite.h`.
2. §4 (phase 1: flip + dummy) — simplest phase, proves the reuse-via-wiring trick, uses only existing `prune_build()`.
3. §3 (`reachable_set()`, `prune_build_coset()`, phase 3's coset) — the one genuinely new piece of infrastructure. Test it in isolation before wiring it into `t_ida_phase`.
4. Phase 2 (fresh `twist × slice` with `MOVES_G1`) and phase 4 (fresh `cperm_sperm`/`eperm_sperm` with `MOVES_G3`) — both pure `prune_build()` calls, no new code.
5. `thistle_solve()` chaining all four, `simplify_moves()` on the joined result.
6. Benchmark: 1,000 random scrambles, expect ~40–45 moves worst case, well under R4's 50-word average ceiling.
