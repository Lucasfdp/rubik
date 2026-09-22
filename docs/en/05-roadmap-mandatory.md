# Roadmap — Mandatory Part

*(Part of the Rubik prep docs — see `00-overview.md` for the index and `GLOSSARY.md` for term definitions.)*

Both of you touch solver internals over the course of this roadmap. The seam is the **coordinate layer** (see `04-architecture.md`), because it's a clean interface that can be agreed in advance and tested independently on both sides.

## Sprint 0 — Foundations (both, pair-programmed, ~2 days)

Do this one together, in the same room/call, writing code side by side. It sets the shared vocabulary for everything that follows, and a disagreement here costs a full week later if it's not caught now.

- Agree cubie indexing: corner order URF, UFL, ULB, UBR, DFR, DLF, DBL, DRB; edge order UR, UF, UL, UB, DR, DF, DL, DB, FR, FL, BL, BR — this matches Kociemba's own naming exactly (see `02a-cube-notation.md`), which is what makes your test vectors comparable to reference solvers. **Write it down in `DECISIONS.md` and never change it** — every piece of code either of you writes from here on assumes this exact ordering.
- Agree the facelet layout string order (U-R-F-D-L-B, 9 stickers each).
- Build the `t_cube` struct, the 18 move permutation tables, and `apply_move()`.
- Write a test harness: apply a move 4 times → should return to solved (four 90° turns = 360° = no change). Apply the sequence `R U R' U'` six times → should return to solved (this is a well-known cube-theory identity). Apply a scramble, then its exact inverse → should return to solved.
- Makefile skeleton, plus a `make valgrind` target from day one (not bolted on later).

## Sprint 1 — Split at the coordinate seam (~4 days)

| | Dev A | Dev B |
|---|---|---|
| **Owns** | `parse/`, `cube/facelet.c`, `main.c` | `coord/encode.c`, `coord/movetable.c` |
| **Builds** | Notation parser (including rejecting M/E/S/x/y/z), cube legality validation, facelet↔cubie conversion, full error handling, the CLI | All six coordinate encoders/decoders, the generic move-table builder, unit tests proving `movetable[encode(c)][m] == encode(apply(c,m))` |
| **Gate** (must pass before moving on) | `./rubik ""`, `./rubik "R2 X"`, `./rubik` with no args, and 10 other garbage inputs all fail cleanly with a useful message and no memory leak | Every coordinate round-trips correctly, and every move table entry matches the cubie-level result, across 100,000 random states |

## Sprint 2 — Split by phase (~5 days)

| | Dev A | Dev B |
|---|---|---|
| **Owns** | `coord/prune.c` (generic BFS builder) + **Phase 1** | `solve/ida.c` (generic driver) + **Phase 2** |
| **Builds** | `twist×slice` and `flip×slice` tables; the phase-1 goal test; the phase-1 move set (all 18 moves) | IDA* with the pruning heuristic; `cperm×sperm` and `eperm×sperm` tables; the phase-2 goal test; the phase-2 move set (10 moves) |
| **Gate** | From 1,000 random states, phase 1 reaches G1 in ≤12 moves, every single time | From 1,000 random G1 states, phase 2 solves in ≤18 moves, every single time |

*Note the deliberate cross-pairing here:* Dev A writes the pruning-table builder, but Dev B is the one who actually consumes it inside the search driver — and vice versa for the phase logic. This is intentional: neither of you should be able to ship a module the other person doesn't understand well enough to explain.

## Sprint 3 — Integration & hardening (both, ~3 days)

- Wire phase 1 into phase 2, then add the **sub-optimal iteration loop** described in `02-algorithms.md` (keep enumerating phase-1 solutions, keep the shortest combined total, stop on a time budget).
- Add redundancy pruning to move generation (no same-face repeats, a fixed ordering rule for opposite-face pairs).
- **Build a benchmark harness:** run 1,000 random scrambles and report average move count, max move count, average solve time, and **max solve time**. This is the artefact that actually proves R4 at defence — commit its output to the repo.
- Run Valgrind clean on every code path, including every error path (not just the happy path).
- Finish the Makefile: `all clean fclean re`.

## Sprint 4 — Mandatory freeze

Nobody touches the bonus until every item on this checklist is green:

- [ ] `./rubik "F R U2 B' L' D'" | cat -e` outputs exactly one line, ending in `$`, nothing else on the line
- [ ] `wc -w` average over 1,000 scrambles is ≤ 50 (expect roughly 21)
- [ ] Worst-case wall-clock time over 1,000 scrambles is under 3 s (expect under 100 ms)
- [ ] Every error path produces a clean message, a non-zero exit code, and is Valgrind-clean
- [ ] Solver's function signature takes a cube *state*, never a string (the anti-cheat rule, `01-requirements.md`)
- [ ] Both of you can explain **every module** — including the other person's — on a whiteboard, cold
