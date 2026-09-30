# Other Puzzles — what 2×2×2 took, and why 4×4×4 / Megaminx / Square-1 don't

*(Part of the Rubik prep docs — see `00-overview.md` for the index and `GLOSSARY.md` for term definitions.)*

The subject's bonus list (page with the bonus items) says:

> Ways to work with other puzzles (4x4x4, 2x2x2, Megaminx, Square-1?)

That is a request to show a way, not a mandate to ship four solvers. Given R11 (the bonus is worth zero unless the mandatory part is perfect) and this item being explicitly last on `06-roadmap-bonus.md`'s own priority list, the scope taken here is: **implement 2×2×2 for real** (it turned out to reuse almost everything already built), and **document, precisely, against the actual code, why the other three are not a smaller version of the same job.** That second half is worth as much at defence as the first — it shows the architecture was understood, not just extended by luck.

## What shipped: 2×2×2

Run it with `-p 2x2x2` on the bonus binary:

```
./rubik_bonus "R U2 F' D" -p 2x2x2
```

### Why it was cheap

A 2×2×2 has corners and nothing else — no edges, no centres. `t_cube` (`cube.h`) already keeps corners and edges as two entirely separate arrays (`corner_perm`/`corner_orient` vs. `edge_perm`/`edge_orient`), and `coord.h` already has `encode_twist`/`decode_twist` (corner orientation, 3⁷) and `encode_cperm`/`decode_cperm` (corner permutation, 8!) as coordinates that read **only** the corner arrays. So "solve a 2×2×2" reduces to: apply the ordinary 18-move `apply_move()` to a cube exactly as before (edges tag along and are never looked at again), and search for the shortest move sequence that drives `(twist, cperm)` to `(0, 0)`.

That search is a *single* IDA* pass, not Kociemba's two phases — there is only one goal (corners solved), so there is nothing for a phase boundary to split. It reuses, unmodified:

- the same six move tables `move_tables_build()` already produces (only `->twist` and `->cperm` are read);
- `prune_build()` and `ida_search()` exactly as they exist, because a state here is described with the *same* three-coordinate, two-pruning-pair shape `t_ida_phase` already expects — the third "coordinate" is a constant that never moves (`g_zero_table[MOVE_COUNT] = {0}`), which turns each pruning pair into a plain 1-D table for twist alone and cperm alone. Nothing in `prune.c` or `ida.c` needed to change.

The whole new module is `src/solve/two_by_two.c` (~70 lines) plus its header — see `include/twobytwo.h` for the full contract, in particular why validation there is *only* `cube_corner_twist_sum(cube) == 0` and not the full `cube_is_valid()` used by the 3×3×3 path: a 2×2×2 has no edges, so there is no permutation-parity invariant linking corners to anything else to enforce. Any corner permutation is reachable on its own.

### The 11-move bound

`TWOBYTWO_MAX_MOVES` is `11`, and that is not a guess. Corner-only 2×2×2 solving with the 3-generator set {U, R, F} (the usual speedcubing convention, since a 2×2×2 has no centres to distinguish which of the six faces you call "R" from a whole-cube rotation) has a proven half-turn-metric diameter of 11 — see `09-sources.md`-style provenance: this is a long-established computational result (full breadth-first enumeration of the 3,674,160-state group), not something re-derived here. Our representation is a strict superset of that generator set (all 18 moves, all six faces distinguishable because our cubie model keeps a fixed frame regardless of whether a real 2×2×2 would have one) — a bigger generator set can only ever find an optimal solution *at least as short*, never longer, so 11 remains a valid upper bound here too. `test_two_by_two.c` checks every random scramble's solution length against it directly.

### Tests

`tests/test_two_by_two.c` (`make test_two_by_two`, included in `make test`): already-solved, random scrambles solved and re-verified with `two_cube_is_solved()`, the corner-twist invariant rejecting a bad state, and — the test that actually proves the design claim — two cubes with identical corners but *different* edge permutations and orientations get byte-identical solutions, because `two_solve()` never reads `edge_perm` or `edge_orient` at all.

### The 3D bonus renders it too

`render_run()` opens on `-p 2x2x2 -r`, or a running window switches live with **K** (`render/app.c`'s `switch_puzzle()`), between the two views — the HUD always shows which one is active, next to the palette/algorithm/auto-loop hints.

The reason this stayed cheap is the same reason the solver did: `render/geometry.c`'s `t_render_scene` already keeps all `RENDER_CUBIE_COUNT = 8 + 12 + 6` slots populated and synced every frame (`geometry_sync()`), corners first (slots `0..CORNER_COUNT-1`), regardless of which puzzle is showing — a 2×2×2 view never needed a second scene, a second position table, or a `geometry_init_2x2()`. What changed is *how many of those slots get drawn or raycast*, decided per-frame by three small `t_puzzle`-aware helpers in `geometry.h`:

- `geometry_visible_count()` — `RENDER_CUBIE_COUNT` for a 3×3×3, just `CORNER_COUNT` for a 2×2×2, since its edge/centre slots exist and stay synced but are simply never among the first N a 2×2×2 draws;
- `geometry_body_size()` — `CUBIE_BODY_SIZE` normally, or `CUBIE_BODY_SIZE_2X2` (`= CUBIE_SPACING + CUBIE_BODY_SIZE`) for a 2×2×2, so its 8 corner cubies grow to fill the gap the (now invisible) edge cubies used to occupy, keeping the same seam width the 3×3×3 has between its own cubies — one derived constant, no new position table;
- `geometry_half_extent()` — the matching bounding-box size `input_pick_start()`'s single-box raycast (`docs/en/11-drag-review.md` §1.4/B4) needs for mouse turning to hit-test the bigger 2×2×2 cubies correctly.

`draw_scene()` loops to `visible_count` instead of a hard-coded `RENDER_CUBIE_COUNT`, and scales its sticker size and rounded-corner fillets by `body_size / CUBIE_BODY_SIZE` so a 2×2×2's larger cubies get proportionally-sized stickers instead of the old small ones floating on a much bigger body. `input.c`'s mouse-drag turning needed no logic changes at all beyond that one box size: a 2×2×2 pick is always a corner pick (all three lattice coordinates non-zero), which was already routed through the ordinary axis-lock path — the existing "no centre piece here" and "no middle slice here" degradation (`is_center_pick()`, `drag->blocked`) already covers a puzzle with neither, because it was written to ask "what did the geometry say I hit," never "which puzzle is this."

Solving follows the same switch: `solve_with_algo()` checks `app->puzzle` before `app->algo` — `PUZZLE_2X2X2` always calls `two_solve()` (there is only one algorithm to pick between), and `app_is_solved()` picks `two_cube_is_solved()` or the full `cube_is_solved()` the same way, so the practice timer, the solve-celebration effect, and auto-loop all read "solved" correctly for whichever puzzle is on screen. Switching (K) never touches `app->cube` itself: both views are the same full cube the whole time, exactly as `-p 2x2x2` and plain solving already agree — a 2×2×2 is "look at fewer of the same pieces," not a different cube.

## Why the other three are not "2×2×2, but more"

The reason 2×2×2 was nearly free is that it is a **strict subset** of the existing model: fewer piece types, same piece types otherwise, same move alphabet, same group-theoretic machinery. The other three each break a *different* one of those assumptions, and none of them are subsets.

### 4×4×4 — breaks the piece model and the move alphabet

A 4×4×4 has no fixed corner/edge/centre counts of 8/12/6 the way `CORNER_COUNT`/`EDGE_COUNT`/`RENDER_FACE_COUNT` assume. It has 8 corners (fine, same as 3×3×3) but 24 edge *pieces that come in 12 indistinguishable-looking pairs* ("wings") and 24 centre pieces in 6 groups of 4 that carry colour but no fixed identity relative to each other. None of that fits `t_cube`'s `edge_perm[EDGE_COUNT]`/`edge_orient[EDGE_COUNT]` — a wing's "identity" is only meaningful once it is paired with its partner, and a centre piece has no orientation concept in `t_cube` at all (centres are deliberately left out of `t_cube` right now, per its own doc comment, because on a 3×3×3 "they never move relative to each other, so they carry no information" — false the moment the puzzle has more than one center per face).

The move alphabet breaks too: `t_move` is a flat `enum` of exactly `6 faces × 3 turns = 18`, and every move table in the codebase is sized and addressed as `state * MOVE_COUNT + move`. A 4×4×4 needs *inner-slice* turns (`Rw`, `2R`, or however you notate them) in addition to outer-layer turns — moves that don't exist in the 3×3×3 alphabet at all, not extra values of an existing one.

And the standard solving approach (reduction: pair the wing edges, group the centres, then solve what is left as a 3×3×3) introduces **parity cases that a real 3×3×3 can never produce** — OLL-parity and PLL-parity states that only arise because two originally-identical wing pieces can end up swapped during pairing. `validate_cube()`'s invariants (corner-twist ≡ 0 mod 3, edge-flip ≡ 0 mod 2, matching permutation parity) are 3×3×3 physical laws; a 4×4×4 with parity mid-solve *legitimately* violates the exact things that function exists to reject. None of `coord.h`'s combinatorial constants (3⁷, 2¹¹, C(12,4), 8!) are the right numbers for a puzzle whose piece counts are different in kind, not just degree.

### Megaminx — breaks the move-set's storage type, not just its size

A Megaminx is a dodecahedron: 12 faces, 20 corners (each touching 3 faces, like a 3×3×3 corner), 30 edges (each touching 2 faces). The corner/edge *shape* of the model actually generalises — a Megaminx corner is combinatorially the same kind of object as a `t_corner`. What breaks is size and storage: `12 faces × 3 turns = 36` moves, and `t_move_mask` — the type every pruning table, move table and IDA* driver in this codebase uses to pass "which moves are allowed" — is a `uint32_t` (`movetable.h`). Thirty-six move bits do not fit in 32. `MOVES_ALL`, `MOVE_BIT()`, `move_in_mask()`, and every function signature carrying a `t_move_mask` would need to move to a 64-bit (or two-word) mask type — a mechanical but real change that touches `movetable.h`, `prune.h`, `ida.h` and every caller.

Past that: 20!, 30!, and the orientation counts for a piece with 3-fold (corner) or 2-fold (edge) symmetry on a dodecahedron are all-new combinatorial constants requiring their own from-scratch derivation of which coordinate pairs make a tractable pruning table (the 3×3×3's split into twist/flip/slice and cperm/eperm/sperm is *specific to the group chain Kociemba found for the cube group* — Thistlethwaite/Kociemba's phase structure does not just "port"; a dodecahedral analogue is its own unsolved-by-this-project research question). And every position table in `render/geometry.c` (`CORNER_POS`, `EDGE_POS`, `CENTER_POS`, all sized and shaped for a cube lattice) would need a dodecahedral-lattice equivalent from scratch.

### Square-1 — breaks the state representation itself

The first two puzzles keep this codebase's central assumption: a fixed number of pieces, each always occupying exactly one of a fixed number of slots, so a "state" is a permutation-plus-orientation pair. Square-1 does not have that property at all. Its top and bottom layers are each made of 8 wedge-shaped pieces that are **not the same size** — a "corner" wedge spans 60° and an "edge" wedge spans 30°, so the number of pieces that fit between two turns is variable, not fixed at 8 or 12. The signature move (the "slice"/tip-over that swaps the top and bottom halves) is only legal when the top and bottom layers happen to be split into two groups of exactly six wedge-widths each — a *geometric alignment precondition* on the move itself, with no analogue anywhere in this codebase, where every one of the 18 moves is legal from every state unconditionally.

Modelling this honestly means replacing `t_cube`'s fixed-size arrays with something like an ordered list of variably-sized wedge tokens per layer — a different data structure, not a bigger version of `t_corner`/`t_edge`. `apply_move()`, every coordinate encoder, every move table, and the renderer's cubie lattice (`RENDER_CUBIE_COUNT`, fixed positions) are all built on "N pieces, N fixed slots," and Square-1 has neither N fixed pieces nor fixed slots. This is a new project sharing a name, not an extension.

## Summary

| Puzzle | Piece model | Move alphabet | Group-theory / coordinates | Renderer | Verdict |
|---|---|---|---|---|---|
| 2×2×2 | Subset (corners only) — reuses `t_cube` as-is | Subset (same 18) | Reuses `twist`/`cperm` as-is | Reuses the same 26-slot scene, just draws/picks fewer of its slots | **Shipped, CLI and 3D** |
| 4×4×4 | New piece types (wing pairs, anonymous centres) | New moves (inner slices) | New parity cases `validate_cube()` must NOT reject | New position table + wing/centre geometry | Rewrite of `cube/`, `coord/`, part of `parse/` |
| Megaminx | Same *kind* of pieces, new counts (20/30) | `t_move_mask` (`uint32_t`) too small for 36 moves | All-new combinatorial constants and phase structure | Dodecahedral lattice from scratch | Rewrite of `movetable.h`'s mask type + all of `coord/` |
| Square-1 | No fixed piece/slot count at all | Moves with alignment preconditions | No permutation-coordinate model applies | Variable-geometry cubies | New project: `t_cube` itself does not fit |

The common thread: this codebase's speed and correctness both come from choosing one fixed, small, well-understood state shape (`corner_perm[8]`, `corner_orient[8]`, `edge_perm[12]`, `edge_orient[12]`) and building every later layer — coordinates, move tables, pruning tables, IDA*, the renderer's cubie lattice — as code that is *generic in the coordinate values* but *not generic in what a piece is*. 2×2×2 fits because it needs fewer of the same pieces. The other three each need a different notion of "piece," and that assumption is load-bearing everywhere above `cube.h`, not a detail confined to one file.
