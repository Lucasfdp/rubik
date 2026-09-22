# Solver Algorithm — Options & Decision

*(Part of the Rubik prep docs — see `00-overview.md` for the index and `GLOSSARY.md` for term definitions.)*

**Recommendation: Kociemba two-phase, built on a generic coordinate + IDA* framework.**

## 3.0 The shared substrate (build this first, regardless of which algorithm you pick)

Before comparing algorithms, it helps to know that options B–E below share about 70% of their code. That shared code is:

1. **Cubie model** — the cube represented as 8 corner pieces and 12 edge pieces, each tracked by *where it is* (its permutation — see glossary) and *how it's twisted/flipped* (its orientation). Corners have 3 possible orientations (0, 1, or 2 — think "not twisted, twisted clockwise, twisted counter-clockwise"); edges have 2 (flipped or not).
2. **Facelet model** — the cube as 54 individual stickers. This is what you use for reading input, writing output, and driving the 3D renderer — it's the "what does it look like" view, as opposed to the cubie model's "what piece is where" view. (For the actual piece names, facelet index numbering, and how twist/flip are encoded, see `02a-cube-notation.md`.)
3. **Move application** — the 18 possible moves (6 faces × {90° clockwise, 90° counter-clockwise, 180°}), each implemented as a permutation table: a precomputed lookup of "piece in slot X moves to slot Y" for that move.
4. **Coordinate encoders** — functions that compress a full cubie-model cube down into one small integer (a "coordinate" — see below). This is the trick that makes the whole thing searchable.
5. **Move tables** — precomputed lookups of the form `new_coordinate = table[old_coordinate][move]`. Built once, at startup, by applying each move to every coordinate value at the cubie level and recording the result.
6. **Pruning-table generator** — a generic breadth-first search (BFS — see glossary) over a coordinate space, starting from the goal state and working backwards, that records "how many moves minimum to reach the goal from this coordinate value."
7. **IDA* driver** — the actual search algorithm (iterative deepening with a pruning-table heuristic — see glossary) that uses the tables above to find a solution.

So the real decision isn't "which codebase" — it's "which coordinates, and how many phases." **Build 1–7 generically and the algorithm choice becomes cheap and reversible**, which is why option E (run two algorithms, keep the better answer) is nearly free once this substrate exists. This is the single most important structural point in this file.

---

## 3.A Layer-by-layer / beginner method

Solve the first layer, then the middle-layer edges, then the last layer using a fixed set of memorised move sequences (this is how most humans learn to solve a cube).

| Pros | Cons |
|---|---|
| Trivial to understand and explain | **Fails R4 outright** — 100–210 moves average, subject requires ≤50 |
| No tables, no search, no memory overhead | Lots of tedious case-handling code; ironically not *less* implementation work than the "hard" algorithms |
| Solves instantly (<1 ms) | Nothing to defend algorithmically — doesn't demonstrate the "moderate notions of group theory" the subject asks for |

**Verdict: do not ship.** One genuine use: implement it as a throwaway *oracle* to sanity-check your move engine ("does my cube model actually behave like a real cube?"). Even that's optional — a simpler round-trip test (apply a scramble, then its inverse, check you're back to solved) covers the same ground more cheaply.

---

## 3.B Thistlethwaite's four-phase algorithm

The idea: descend through a chain of nested **subgroups** (see glossary — informally, a subgroup here means "a smaller set of allowed moves that still lets you reach a restricted set of cube states"), restricting which moves you're allowed to use at each step:

```
G0 = <U, D, L, R, F, B>        all 18 moves       — any scrambled cube
G1 = <U2, D2, L, R, F, B>      edge orientations fixed
G2 = <U2, D2, L, R, F2, B2>    corner orientations + UD-slice edges fixed
G3 = <U2, D2, L2, R2, F2, B2>  half turns only
G4 = {identity}                 solved
```

(Reading that notation: `U2` means "only 180° turns of the U face are allowed in this phase," `U` alone means any turn of U is still allowed. As you move down the chain, moves get *more restricted*, because each phase locks in a property that the later, more-restricted moves can't undo.)

Thistlethwaite's original version bounded the four phases at 7, 13, 15, and 17 moves worst-case (52 moves total). A later, more exhaustive search of the intermediate spaces (called coset spaces — see glossary) showed the true worst case is actually 7, 10, 13, and 15 moves per phase — 45 total.

The tables needed have these many entries (this is the count of distinct positions within each intermediate coset space — you don't need to memorise these, just know they're all small enough to fit in RAM easily): 1,024 · 1,082,565 · 29,400 · 663,552.

| Pros | Cons |
|---|---|
| **Easiest to explain visually** — "each phase locks a property that later phases can't break" | Move count 40–45 average: passes R4, but with much less margin than Kociemba |
| Largest table is ~1.08M entries — trivially fits in RAM, generates in well under a second | Four phases = four sets of coordinates to get right, vs. two for Kociemba |
| Each phase is independently unit-testable ("is the cube in G2 now?") | Result is visibly far from the 20-move optimal solutions the subject references |
| Exact-distance tables (built with plain BFS) mean no heuristic subtleties to get wrong | You end up doing most of Kociemba's implementation work anyway, for a worse move count |

**Verdict:** excellent *teaching* algorithm, and a genuinely cheap second algorithm once the shared substrate (§3.0) exists. Not the one to ship alone.

---

## 3.C Kociemba's two-phase algorithm ← **recommended**

Kociemba's 1992 insight: Thistlethwaite's G1 (see above) is *already* restrictive enough. Once a cube reaches G1, the remaining puzzle lives in a small enough space to search directly — no need to subdivide further into G2/G3. So the four-phase chain collapses to two phases:

```
Phase 1: any state  →  G1 = <U, D, R2, L2, F2, B2>    using all 18 moves
Phase 2: G1         →  solved                          using only the 10 G1 moves
```

**Phase 1's goal, stated plainly:** get all 12 edges oriented correctly, all 8 corners oriented correctly, and the 4 "UD-slice" edges (the ones that belong in the middle layer — FR, FL, BL, BR) somewhere in the middle layer (not necessarily in the right *order* yet, just the right *layer*).

**Coordinates** (these numbers are worth memorising — you'll want them on the whiteboard at defence):

| Phase | Coordinate | Size | Meaning |
|---|---|---|---|
| 1 | `twist` | **2,187** = 3⁷ | Corner orientations. Only 7 corners need tracking — the 8th is *determined*, because the total twist across all corners is always a multiple of 3 (a mathematical invariant of the cube). |
| 1 | `flip` | **2,048** = 2¹¹ | Edge orientations. Same idea: only 11 edges need tracking, the 12th is determined because total flip is always even. |
| 1 | `slice` | **495** = C(12,4) | *Which 4 of the 12 edge slots* currently hold the 4 UD-slice edges, ignoring their order. (C(12,4) is "12 choose 4" — the number of ways to pick an unordered group of 4 items from 12.) |
| 2 | `cperm` | **40,320** = 8! | Permutation (arrangement) of the 8 corners. |
| 2 | `eperm` | **40,320** = 8! | Permutation of the 8 U/D-layer edges. |
| 2 | `sperm` | **24** = 4! | Permutation of the 4 slice edges *within* the slice. |

A **coordinate**, in plain terms, is a lossy fingerprint: a small integer that captures *one specific property* of the cube's state while throwing away everything else. Millions of different actual cube arrangements can share the same `twist` value, because `twist` only cares about corner orientation, not position. That's deliberate — it's what makes the search space small enough to handle.

Phase 1's full state space is the **Cartesian product** of `twist × flip × slice` (see glossary for what a Cartesian product is — briefly, it's "every possible combination of one value from each set"): 2,187 × 2,048 × 495 ≈ 2.2 billion. That's too big to store as one giant table, so it's factored into pairwise sub-tables instead (see the pruning table row below) — you look up two smaller tables and combine the results, rather than one table with 2.2 billion entries.

**Pruning tables actually needed** (a "pruning table" is a precomputed lookup of "minimum moves needed from this coordinate value" — see glossary; one byte per entry is fine, no need for anything fancier):

| Table | Entries | Bytes @1B | Phase |
|---|---|---|---|
| `twist × slice` | 2,187 × 495 = 1,082,565 | ~1.0 MB | 1 |
| `flip × slice` | 2,048 × 495 = 1,013,760 | ~1.0 MB | 1 |
| `twist × flip` *(optional, stronger)* | 2,187 × 2,048 = 4,478,976 | ~4.3 MB | 1 |
| `cperm × sperm` | 40,320 × 24 = 967,680 | ~0.9 MB | 2 |
| `eperm × sperm` | 40,320 × 24 = 967,680 | ~0.9 MB | 2 |

Total is under 10 MB, and every table is generated by plain BFS in well under a second. **This matters a lot for R4**: it means you can regenerate every table fresh at every program startup and *still* be far inside the 3-second budget — which means you never have to defend an awkward "but the tables are cached on disk from a previous run, does that count as cheating?" question.

**Move quality.** Kociemba's own reference implementation, without any extra optimisation, lands solutions in the 20–23 move range in low milliseconds on modern hardware. Every position in the subject's own referenced context is solvable in at most 20 moves optimally — so 20–23 is very close to optimal, while running orders of magnitude faster than a solver that searches for the true optimum (see option D below).

**The sub-optimal iteration loop** — the part people skip, and shouldn't: don't just take phase 1's *first* solution and run with it. Keep enumerating phase-1 solutions of increasing length; for each one, run phase 2 on the result; keep whichever *combined* total (phase 1 length + phase 2 length) is shortest so far. Stop when you hit a time budget or a target length. This one technique is what turns a mediocre ~25-move result into the ~20–21 move result quoted above.

| Pros | Cons |
|---|---|
| **~20–23 moves** — enormous margin under R4's 50-move ceiling | More coordinate machinery than Thistlethwaite (though only 2 phases, not 4) |
| Solves in **milliseconds**, tables build in **well under a second** | Pruning heuristics need care — an incorrect table silently breaks correctness (see "known traps" below) |
| The canonical, well-documented answer for this exact problem | The `eperm` coordinate **cannot be tracked through phase-1 moves** — see traps below |
| Explains cleanly with one visual: "first make the cube well-behaved, then finish with only safe moves" | The sub-optimal iteration loop adds a layer of search-control logic beyond a plain single-shot search |
| Symmetry reduction (an optimisation that exploits the cube's physical symmetries to shrink tables further) is **optional** — you can skip it entirely and still be 1000× inside the time budget | |

**Verdict: ship this.**

### Known traps (write these on the whiteboard before you start coding)

1. `eperm` (permutation of the U/D-layer edges) is only *meaningful* once you're inside G1. Don't try to build a move-table for it across phase-1 moves — instead, compute it directly from the cubie-level cube at the phase-1/phase-2 boundary. (A more advanced approach tracks it incrementally via separate `u_edges`/`d_edges` coordinates; simplest is to just recompute it fresh.)
2. Pruning values must always be a **lower bound** on the true distance — i.e., the table must never claim "you need at least 8 more moves" when the true answer is 6. If a table ever *overestimates*, IDA* (see glossary — this property is called being "admissible") will return wrong answers, or no answer at all, in a way that's very hard to debug because it only shows up on specific inputs.
3. Move ordering / redundancy pruning: don't let the search try `R` right after `R` (that's equivalent to a single `R2` or nothing, and is always wasteful), and pick a fixed ordering rule for opposite-face pairs (e.g. don't try `L` immediately after `R` if you've already tried `R` after `L` for the same branch). Skipping this roughly doubles search time for no benefit.
4. Corner-orientation and edge-orientation **parity** (see glossary) must be validated on input, before search starts. A "physically impossible" cube — one that could never result from actually scrambling a real cube — sends the search hunting for a solution that doesn't exist, which looks exactly like an infinite loop. That's an instant R6 failure, and it's entirely preventable with an upfront validity check.

---

## 3.D Korf's IDA* with pattern databases (the optimal solver)

Korf's approach precomputes three **pattern databases** (large pruning tables, one per group of pieces): one for the 8 corners, one for 6 of the 12 edges, and one for the remaining 6 edges. During the search, it takes the *maximum* of the three lookups (not the sum — because every twist moves four edges and four corners at once, so the three estimates aren't independent and summing them would overestimate, breaking admissibility). The median optimal solution length this produces is 18 moves.

| Pros | Cons |
|---|---|
| **Provably optimal** — the true shortest possible solution for each cube | **Violates R4's 3-second budget by orders of magnitude.** Hard scrambles at depth 18–20 can take minutes to hours to solve |
| Directly satisfies the bonus item "an algorithm that goes lower than the most optimised solutions" | Pattern databases are 85 MB+ and take minutes to generate; caching them to disk becomes close to mandatory, which then invites its own defence question |
| Reuses the same IDA* driver you already need for Kociemba | R6 risk: a search that legitimately takes minutes looks indistinguishable from an infinite loop to an impatient evaluator |

**Verdict: not the shipped solver.** Worth adding *later*, optionally, behind a flag like `./rubik -optimal "<mix>"`, with a hard wall-clock (real elapsed time, as opposed to CPU time) timeout that falls back to the Kociemba answer if it doesn't finish. The subject itself says bonuses that take more than a few seconds aren't considered reasonable — so gate this behind an explicit flag and never let it be the default path.

---

## 3.E Multi-algorithm selection (bonus item, nearly free)

The subject's bonus list explicitly names "a choice between several algorithms, or a selection of the best solution between several algorithms." If the shared substrate in §3.0 was built generically, running both Thistlethwaite and Kociemba and printing whichever result is shorter is maybe 40 lines of glue code.

| Pros | Cons |
|---|---|
| Named bonus item, very low marginal cost | Only free if §3.0 was actually built generically from day one — retrofitting this later is expensive |
| Makes the defence noticeably stronger — you can compare move counts live, on the spot | Two algorithms running = two things that can independently be wrong |

---

## 3.F Algorithm decision matrix

| | Avg moves | Solve time | Table RAM | Table gen time | Implementation effort | Passes R4 |
|---|---|---|---|---|---|---|
| **A** Layer-by-layer | 100–210 | <1 ms | 0 | — | Medium | ❌ |
| **B** Thistlethwaite | ~40–45 | <10 ms | ~2 MB | <1 s | Medium-high | ✅ (thin margin) |
| **C** Kociemba | **~20–23** | **~1–50 ms** | **<10 MB** | **<1 s** | High | ✅✅ |
| **D** Korf (optimal) | 18 (optimal) | seconds–hours | ~85 MB | minutes | High | ❌ |
| **E** B + C, pick best | ~20–23 | <60 ms | ~12 MB | <1 s | C + small addition | ✅✅ |
