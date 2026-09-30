# Third Algorithm (Layer-by-Layer) & Cross-Algorithm Benchmark — Spec

*(Part of the Rubik prep docs — see `00-overview.md` for the index and `GLOSSARY.md` for term definitions.)*

> **Implementation note:** this doc was written before `src/solve/layer.c`
> existed, as the up-front plan. Two things changed once real derivation
> against the engine started: there is no `cube/facelet.c` in this
> codebase (§13.B below is stale on that point) — `layer_solve()` works
> entirely on the cubie model (`t_corner`/`t_edge` perm+orient arrays),
> locating pieces and reading orientation directly off `t_cube`, the same
> as every other solver here. And stage 4 (§13.C) is not a repeat-until-
> solved loop over a single trigger: that approach was tried and found to
> cover only a fraction of the possible last-layer states within a
> reasonable repetition bound (see `02-algorithms.md` §3.A's updated
> note), so the shipped version hands the last layer to the existing
> Kociemba `solve()` instead of a hand-built OLL/PLL table. Stages 1-3
> (cross, first-layer corners, second-layer edges) match this doc's plan:
> fixed, short, hand-derived-and-verified algorithms, dispatched by table
> lookup, never a runtime search. `LAYER_MAX_MOVES` shipped as `200`, not
> the `400` estimated below — see `layer.h`'s own comment for the
> worst-case arithmetic behind that number.

`02-algorithms.md` §3.A vetoed layer-by-layer as *the* solver (100–210 moves, fails R4). This doc reverses only the shipping decision, not the analysis: LBL becomes a **third, non-default** bonus algorithm whose entire purpose is pedagogy — it's the one a person can follow by hand — plus a benchmark mode that runs all three algorithms on one scramble so their move counts can be compared live. Both live only in the bonus binary; the mandatory binary (no `BONUS_ALGO`) is untouched.

## 13.A CLI surface

Two independent, orthogonal flags — following the existing pattern where each flag toggles one thing (`-a` picks the algorithm, `-r` picks render-vs-print):

- `-a layer` — new third value alongside the existing `kociemba` / `thistlethwaite`.
- `-c` (new, boolean) — benchmark mode: run all three solvers on the same resulting cube, print each one's move count and solution, ignore `-a`. Mutually exclusive with `-r` (comparison is print-only; rendering one of three solutions at once is a separate feature nobody asked for) — reject the combination as a usage error, same style as the existing `parse_args()` checks.

**Why not fold benchmark mode into `-a` (e.g. `-a all`)?** `-a`'s contract, per its own doc comment, is "which solver `-a` picked" — singular. Overloading it to mean "run all three" breaks that contract for every future reader of `parse_args()`. A separate flag keeps each flag's meaning single-purpose, which is the existing convention (`-r` doesn't hide inside `-a` either).

Usage becomes:
```
rubik_bonus "<scramble>" [-a kociemba|thistlethwaite|layer] [-r] [-c]
```

## 13.B Where it lives

LBL needs **none** of the shared substrate in `02-algorithms.md` §3.0 — no coordinates, no move tables, no pruning tables, no IDA* for stages 1-3. It solves directly on the cubie model (`cube/cubie.c`), locating each piece by scanning `corner_perm`/`edge_perm` and reading its orientation, applying moves with the existing move-application code exactly like the renderer's manual-turn path does. (As shipped, stage 4 — the last layer — does reuse Kociemba's `solve()`/IDA* rather than a hand-built OLL/PLL table; see the implementation note above.) That makes stages 1-3 structurally independent of Kociemba/Thistlethwaite — zero risk of destabilizing either.

```
include/layer.h       LAYER_MAX_MOVES, layer_solve() declaration
src/solve/layer.c     the four stages below
tests/test_layer.c    mirrors test_thistlethwaite.c
```

Add one line to `04-architecture.md`'s tree under `solve/`: `layer.c    (optional) the beginner-method variant`.

**Signature** (no init/free pair needed — there are no tables to build):
```c
# define LAYER_MAX_MOVES 200   // worst case across every stage's pop+align+insert, see layer.h

/// Solves with the beginner (layer-by-layer) method. Same anti-cheat
/// contract as solve()/thistle_solve(): takes only the resulting cube.
/// @return move count, or -1 if cube is invalid (mirrors solve()).
int  layer_solve(const t_cube *cube, t_move *out);
```

## 13.C The four stages (standard beginner method)

1. **White cross** — for each of the 4 white edges: locate it, get it to the top face via U/D/wide turns, align it above its target center, insert with the 1-2 move case for its current orientation.
2. **First-layer corners** — for each of the 4 white corners: locate, bring to top-right-front slot via U turns, apply one of 2 canned insertion algorithms depending on which side the white sticker faces.
3. **Second-layer edges** — for each of the 4 middle edges: locate, position above its slot, apply the "insert left" or "insert right" 9-move algorithm (2 canned algorithms, mirrored).
4. **Last layer (2-look OLL + 2-look PLL)** — the part worth simplifying deliberately: rather than a full 57-case OLL / 21-case PLL table (speedcubing scope, not beginner scope), use the classic **repeat-until-solved** trick — apply a single fixed algorithm (e.g. Sune) plus a U turn between attempts, checking progress after each, which resolves *any* orientation/permutation case within a handful of repeats without case recognition. This is exactly what makes LBL "trivial to understand" (§3.A's own pro) and is also why its move count lands in the 100–210 range that already disqualified it as the default — expected here, not a bug.

None of stages 1–3 need new primitives beyond what `cube/moves.c` already exposes; stage 4 reuses `solve.h`'s existing `solver_init`/`solve`/`solver_free` (see implementation note above).

## 13.D Benchmark mode output

One line per algorithm, fixed order (kociemba, thistlethwaite, layer) so a reader can eyeball the spread without re-sorting:

```
kociemba:        21 moves   R2 U F' ...
thistlethwaite:  43 moves   U2 D2 L ...
layer:          134 moves   F R U' ...
```

Move count first (that's the thing being compared), solution text after — same `format_moves()` already used everywhere else. No timing column: all three finish in low milliseconds or less, so move count is the only number that actually differs meaningfully, which matches what was asked for ("solutions moves amount ... to see the difference").

Implementation is glue, not new logic: build every solver's tables once (skip layer's — it has none), run all three against the *same* validated cube (all three take `const t_cube *`, so one shared cube works), print, free tables. ~40–50 lines in `main.c`, same estimate `02-algorithms.md` §3.E already gave this feature.

## 13.E Docs/tests to touch

- `02-algorithms.md` §3.A: append one line noting LBL shipped as a non-default teaching/benchmark-only algorithm, decision matrix otherwise unchanged (still ❌ on "passes R4" — it's never the default and never graded).
- `04-architecture.md`: add `layer.c` to the tree (13.B above).
- `06-roadmap-bonus.md`: add a task line for `-a layer` and `-c`.
- `tests/test_layer.c`: solved-cube → 0 moves; scrambled cube → valid solve (apply output, assert solved) — same shape as `test_thistlethwaite.c`, no move-count assertion (LBL's count isn't bounded the way the other two are).
- `tests/test_cli.sh`: add `-a layer` and `-c` cases.

## 13.F Effort estimate

Stages 1–3 are mechanical (~150–250 lines total, fixed algorithms from any speedcubing reference). Stage 4's repeat-until-solved loop is the only part with real logic (~40 lines: apply algorithm, re-check last-layer state, cap iterations, bail to -1 past a generous ceiling so a bug can't hang instead of failing loudly). Benchmark mode is ~40–50 lines of `main.c` glue. No changes anywhere in `coord/`, `prune/`, `ida.c`, or the mandatory binary.
