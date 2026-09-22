# Glossary

*(Part of the Rubik prep docs — see `00-overview.md` for the index. Every term below is used somewhere in the other files; this is meant to be self-contained, so no prior group-theory or graphics background is assumed.)*

Organised alphabetically. Cross-references point to other glossary entries in *italics*.

---

**Admissible heuristic** — A heuristic (an estimate used to guide a search) is *admissible* if it never overestimates the true cost/distance to the goal. In this project, a *pruning table* must be admissible: it can say "at least 5 more moves," but never "at least 5" when the true answer is 3. Search algorithms like *IDA\** rely on admissibility to guarantee they find a correct answer; a non-admissible table can silently cause wrong or missing solutions.

**Backtracking** — see *IDA\**.

**BFS (Breadth-First Search)** — A search strategy that explores a space level by level: first everything reachable in 1 step, then everything reachable in 2 steps, and so on. Because it expands in order of distance, it's the natural way to build a table of "exact minimum distance to goal" for every state in a space — which is exactly what the pruning-table generators in this project do (starting from the solved state and working outward).

**Cartesian product** — Given two sets, their Cartesian product is the set of *every possible pairing* of one item from the first set with one item from the second. Example: if set A = {1, 2} and set B = {x, y}, the Cartesian product A × B = {(1,x), (1,y), (2,x), (2,y)} — 4 pairs from 2×2 inputs. In this project, phase 1's full state space is described as the Cartesian product of three coordinates (`twist × flip × slice`): every possible combination of a `twist` value, a `flip` value, and a `slice` value. Its size is just the product of the three sizes: 2,187 × 2,048 × 495 ≈ 2.2 billion — which is why it's too large to store as a single table and gets split into smaller pairwise tables instead.

**Centroid** — The geometric centre point of a shape (average position of all its points). Used in `03-graphics.md`'s description of the *painter's algorithm*: quads are sorted by the distance from the camera to their centroid, to decide drawing order.

**Coordinate** — In this project's specific sense: a small integer that captures *one particular property* of the cube's state while discarding everything else — a "lossy fingerprint." For example, the `twist` coordinate (0–2186) only encodes how the 8 corners are individually twisted; it says nothing about where they are or how the edges look. Because it's lossy, many different actual cubes share the same coordinate value — which is exactly what makes coordinates useful: you can search over a small space of coordinate values instead of the astronomically larger space of actual cube states.

**Coset** — A term from group theory. If you have a group (a set of things, like moves, that combine in a well-behaved way) and a subgroup within it (a smaller, self-contained set of moves), a coset is one "copy" of that subgroup, shifted by some outside element. In practice for this project, you don't need the formal definition — what matters is: Thistlethwaite's four phases each work within a *coset space* (an intermediate space between two subgroups), and these coset spaces are what get searched exhaustively to determine the tight worst-case move bounds (7, 10, 13, 15) quoted in `02-algorithms.md`.

**Cubie** — One physical piece of the Rubik's Cube — either a corner piece (touches 3 faces) or an edge piece (touches 2 faces). The centre pieces are usually ignored since they never move relative to each other. The "cubie model" tracks each cubie's *permutation* (which slot it's currently in) and *orientation* (how it's twisted/flipped in that slot) — see `04-architecture.md`. Every corner and edge's actual name: `02a-cube-notation.md`.

**Depth-first search** — A search strategy that goes as deep as possible down one path before backtracking to try alternatives, as opposed to *BFS* which explores level by level. *IDA\** is built on repeated depth-first searches.

**Easing** — In animation, a function that controls how a value changes over time — typically making motion start and end more gradually (accelerate, then decelerate) rather than moving at a constant speed throughout. Used in `03-graphics.md` to make face-turn animations look natural instead of robotic.

**Facelet** — One individual coloured sticker on the cube's surface. There are 54 total (9 per face × 6 faces). The "facelet model" represents the cube as this flat list of 54 colours — as opposed to the *cubie* model, which represents it as 20 physical pieces. Facelets are what you read from input and what you display to the user; cubies are what the solver actually reasons about internally. Facelet index numbering and the 54-character state string: `02a-cube-notation.md`.

**God's Number** — The proven worst-case number of moves needed to solve *any* reachable cube position, using the *half-turn metric*. It's 20. "Proven" means this isn't just the best result anyone has found — it's been shown, exhaustively, that no position needs more than 20 moves, and that some position needs exactly 20. Established in 2010 using roughly 35 CPU-years of donated computing time.

**Half-Turn Metric (HTM)** — A way of counting "how long is this solution." Under HTM, turning any face 90° (a quarter turn) or 180° (a half turn) both count as exactly one move. This is the metric the 42 subject uses (verified by `wc -w` on the output), and it's the metric God's Number (20) is proven under.

**Heuristic** — see *Admissible heuristic*.

**IDA\* (Iterative Deepening A\*)** — A search algorithm that combines *depth-first search* with a depth limit that's gradually increased ("iterative deepening"), guided by a *heuristic* estimate (from a *pruning table*) of remaining distance to prune away branches that can't possibly reach the goal within the current depth limit. It uses far less memory than storing every state it visits, which is why it's the standard choice for a search space as large as the Rubik's Cube's.

**Interpolation** — Computing intermediate values smoothly between a start and end value. In `03-graphics.md`, a face turn is interpolated from 0° to ±90° across several animation frames, rather than jumping instantly.

**Invariant** — A property of a system that stays constant no matter what valid operations are performed on it. Example used in this project: the total corner twist across the whole cube is always a multiple of 3, no matter what sequence of legal moves you apply — this invariant is *why* the 8th corner's orientation is always mathematically determined by the other 7, and doesn't need its own coordinate.

**Lattice** — A regular, evenly-spaced grid of points in space. In `03-graphics.md`, the 26 visible cubies sit at lattice positions — every combination of x, y, z each being −1, 0, or +1.

**Norminette** — 42's automated code-style checker, referenced only indirectly in this project (via the "personal coding norm" area file) — not part of the Rubik subject itself, but relevant if you're applying similar style discipline here.

**Orientation** — How a piece is twisted or flipped *within* its current slot, independent of which slot it's in. Corners have 3 possible orientations (0, 1, 2 — think of it as "not twisted," "twisted one way," "twisted the other way"); edges have 2 (flipped or not). Contrast with *permutation*, which is about *which slot* a piece occupies, not how it sits within that slot.

**Painter's algorithm** — A simple way to draw overlapping 3D shapes correctly without a GPU depth buffer: draw everything farthest-from-camera first, then progressively draw nearer things on top, the same way a painter working on canvas would paint the background before the foreground. It has known failure cases for shapes that overlap in complex ways, but with the cube's 162 non-overlapping quads there's no such case, so simply sorting by distance from the camera and drawing back-to-front works correctly.

**Parity** — Whether a permutation (rearrangement) is "even" or "odd," based on how many pairwise swaps it takes to produce it from the identity (unchanged) arrangement. The Rubik's Cube has parity constraints baked into which states are physically reachable — e.g., you can't have exactly one pair of edges swapped and nothing else, because that would be an odd permutation with no matching orientation change, which never arises from an actual physical scramble. Checking these constraints on input is what catches an "impossible" cube before it's handed to the search (see the traps list in `02-algorithms.md`).

**Permutation** — An arrangement, or rearrangement, of a set of items — specifically here, *which slot* each cubie currently occupies (as opposed to *orientation*, which is how it sits within that slot). "8!" (8 factorial = 40,320) is the number of ways to arrange 8 distinct corner pieces into 8 slots, ignoring orientation — this is the size of the `cperm` coordinate in `02-algorithms.md`.

**Pruning table** — A precomputed lookup table that stores, for every value of some *coordinate*, the minimum number of moves needed to reach the goal from there. Built once (via *BFS*) before the actual search runs, so that during the search, looking up "how far away am I, minimum?" is instant instead of requiring its own search. Must be *admissible* (never overestimate) to guarantee correct results.

**Quad** — Short for "quadrilateral": a flat, four-sided 2D shape, used here as the basic building block for 3D geometry — each face of a cubie is drawn as one quad.

**Quaternion** — A mathematical object used to represent 3D rotations, as an alternative to rotation matrices — mentioned in `03-graphics.md` as something raylib's math library (`raymath`) provides ready-made, so you don't have to implement rotation math from scratch. Not strictly necessary to understand deeply for this project; raylib's simpler rotation functions (`rlRotatef` and friends) are usually enough for axis-aligned cube-face turns.

**Rasterization / Rasterizer** — The process (and the code that performs it) of converting a geometric shape — like a quad, described by its corner points — into the actual pixels on screen that represent it. A "software rasterizer" does this with hand-written code running on the CPU, as opposed to letting a GPU/OpenGL do it. This is the core of the work required by the MiniLibX option in `03-graphics.md` §4.B.

**Subgroup** — A smaller set of moves (or states reachable using only those moves) that's self-contained — meaning, combining any moves from within the subgroup only ever produces other results also within that subgroup. Thistlethwaite's algorithm (`02-algorithms.md` §3.B) works by descending through a chain of increasingly restrictive subgroups, each one only allowing moves that can't undo the property locked in by the previous phase.

**Symmetry reduction** — An optimisation technique that exploits the fact that the cube has physical symmetries (e.g., rotating your view of the whole cube doesn't change the underlying puzzle) to make tables smaller or searches faster, by treating symmetric states as equivalent instead of storing/searching each one separately. Mentioned in `02-algorithms.md` as optional for this project — the time budget (3 seconds) is generous enough that Kociemba's algorithm works well within it even without this optimisation.

**Valgrind** — A tool that runs your compiled program and reports memory errors: leaks (memory allocated but never freed), invalid reads/writes, double-frees, and use of uninitialised memory. R6 in `01-requirements.md` requires the program to be completely clean under Valgrind, on every code path, including error paths.

**VAO / VBO / EBO** — Three types of GPU buffer objects used in raw OpenGL programming (relevant to `03-graphics.md` §4.C if you go the raw-OpenGL route): a **VBO** (Vertex Buffer Object) holds raw vertex data (positions, colours, etc.) on the GPU; an **EBO** (Element Buffer Object, a.k.a. Index Buffer Object) holds a list of indices describing how those vertices connect into shapes, avoiding duplicate vertex data; a **VAO** (Vertex Array Object) bundles the configuration of one or more VBOs/EBOs together so you can switch between different pieces of geometry with one call instead of many. raylib manages all of this internally, which is a large part of why it's faster to get started with than raw OpenGL.

**Wall-clock time** — Actual real-world elapsed time (as if measured by a clock on the wall), as opposed to CPU time (which can be smaller if a process is waiting, or larger if it uses multiple threads). R4's 3-second limit is a wall-clock limit — what an evaluator actually experiences waiting for a result — which is why it matters that table generation happens fast in real terms, not just in CPU cycles.
