# Defence Preparation

*(Part of the Rubik prep docs — see `00-overview.md` for the index and `GLOSSARY.md` for term definitions.)*

The subject says: *"you must be able to explain your algorithm with simple words and visual concepts."* So the answers below are written to work on a whiteboard, not in code — practice saying each one out loud, not just reading it.

**1. "How do we know you're not just inverting the scramble?"**
→ Point to the solver's function signature (see the anti-cheat rule in `01-requirements.md`): it only ever receives a cube state, never the scramble string, so there's nothing to invert. Offer to solve a state the evaluator builds by hand in the 3D viewer — since the solver works from *any* state, not just ones produced by parsing a scramble string, this is a live, on-the-spot proof.

**2. "Why two phases?"**
→ Draw the funnel on the whiteboard: 4.3×10¹⁹ possible states is far too many to search directly, by any computer. Phase 1 doesn't solve anything by itself — it makes the cube *well-behaved*: every edge and corner oriented correctly, the middle-slice edges home in the middle layer. That single restriction shrinks the remaining problem to about 20 billion states, which — unlike 4.3×10¹⁹ — actually *is* small enough to search directly. Phase 2 then finishes the job using only moves that are guaranteed not to undo what phase 1 achieved.

**3. "What is a coordinate?"**
→ A lossy fingerprint. `twist = 2187` just means "how are the 8 corners twisted," written as a 7-digit base-3 number (because the 8th corner's twist is mathematically forced by the other seven). Millions of genuinely different cube arrangements share the same fingerprint — and that's the whole point: you're searching over fingerprints, which is small, instead of over actual cubes, which is astronomically large.

**4. "What's a pruning table?"**
→ A precomputed answer to "how many moves, at minimum, from this fingerprint to the goal?" — for every possible fingerprint, computed once in advance. If the search is 10 moves deep, looking for a solution of length 12, and the table says "at least 5 more moves are needed from here," the search can immediately abandon that branch — it can't possibly reach length 12 with only 2 moves left to try. The one hard rule: the table must never *overestimate*, or the search can return a wrong answer.

**5. "Is it optimal?"**
→ No, and deliberately not. The truly optimal solver (Korf's, see `02-algorithms.md` §3.D) takes minutes to hours per cube. This solver produces solutions of roughly 21 moves, against a proven worst-case optimum of 20 and a subject requirement of 50 — all in milliseconds. Show the benchmark output from `05-roadmap-mandatory.md` Sprint 3 as evidence.

**6. "Justify raylib."** *(or: "justify MiniLibX," if you went that route)*
→ raylib opens a window, hands you an OpenGL graphics context, and draws coloured quads given a camera. It has no idea what a Rubik's cube is. The lattice layout, the face-selection logic, the animation and easing, the commit-back-to-logical-state step, and every line of the actual solver are entirely yours. (If you went the MLX route instead: "we wrote the entire rasterizer ourselves, too" — an even stronger answer.)
