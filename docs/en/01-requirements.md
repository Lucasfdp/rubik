# Requirements, Constraints & Background Numbers

*(Part of the Rubik prep docs — see `00-overview.md` for the index and `GLOSSARY.md` for term definitions.)*

## 1. What the subject actually requires

| # | Requirement | Where it bites |
|---|---|---|
| R1 | Program takes a **mix (scramble) as a single argument**, outputs solution on stdout | CLI + parser |
| R2 | Notation is **F R U B L D** with `'` and `2` modifiers only. **No M/E/S slices, no x/y/z rotations** — in input *or* output | Parser + move set |
| R3 | Metric is **HTM** (half-turn metric — see glossary): any quarter- or half-turn of one face = 1 move. Score = `wc -w` on our output | Output format: single line, space-separated, no extra words |
| R4 | **Average ≤ 50 moves**, **max response time 3 seconds** | Algorithm choice (`02-algorithms.md`) |
| R5 | **Returning the inverse of the scramble is cheating.** Also no "insert a no-op sequence to pad it" | Architecture — see "the anti-cheat rule" below |
| R6 | No segfault, no leak, no double free, no infinite loop. "No tolerance." | Valgrind gate in CI/Makefile |
| R7 | Errors must be handled properly | Invalid tokens, invalid cube, empty input, no arg, too many args |
| R8 | Makefile with the usual rules | `all clean fclean re` (+ `bonus`) |
| R9 | Any library is allowed **if we can justify it**. A library that does the work for us is not | raylib = windowing/graphics abstraction, OK. A cube-*solving* library = not OK |
| R10 | We must be able to **explain the algorithm in simple words and visual concepts** | `07-defence-prep.md` |
| R11 | Bonus is only graded if the mandatory part is **perfect** | Sequencing: mandatory ships first, no exceptions |

A quick note on jargon in that table: "the mandatory part" just means the required, non-bonus part of the project — the part you must finish and get perfect before any bonus work counts for anything (see R11).

## The anti-cheat rule (the single most important architectural decision)

R5 is the easiest way to fail this defence, and it's an architecture decision, not a coding one. Enforce it at the type level — i.e., make it impossible to get wrong by how the function signatures are written, not just something you promise to remember:

```
solve(const t_cube *state) -> char *
```

The solver **never receives the scramble string**. It receives a cube *state* (the internal representation of what colour sticker is where — see "cubie" and "facelet" in the glossary). `main` parses the scramble, applies it to a solved cube, throws the string away, and hands only the resulting state to the solver.

Why this matters: any evaluator who asks "how do we know you're not just inverting the mix?" gets a one-line, verifiable answer — read the function signature. It physically cannot receive the string it would need to invert.

Secondary benefit: because the solver only ever sees a state, you can feed it a state from *any* source — a random-state generator, a file, moves made by hand in the 3D viewer — and it still works. That gets you two bonus items (scramble generator, and solving a manually-turned cube in the viewer) almost for free.

## 2. Background numbers everyone on the team should know

These aren't trivia — each one directly explains a design decision later.

- The cube has **43,252,003,274,489,856,000** reachable states (about 4.3×10¹⁹). This is *why* you can't just search the whole problem directly — no computer can hold or search a space that size. It's the number that justifies every "coordinate" and "pruning table" trick in `02-algorithms.md`.
- **God's Number is 20 in HTM.** This means: every one of those 4.3×10¹⁹ positions can be solved in 20 moves or fewer, and this was *proven* (not just observed) in 2010, using about 35 CPU-years of computation donated by Google. It's the theoretical floor — no solver, however clever, can beat "20 moves worst case" because 20 is provably enough and some positions provably need it.
- HTM (half-turn metric) is the same metric the subject uses: any twist of any face — 90° or 180° — counts as one move. This is the "move" that R3's `wc -w` is counting.
- For calibration, human solving methods: **CFOP** (a common competitive method) averages ~56 moves; the **beginner's layer-by-layer method** averages roughly 110, and a larger sample (10,000 scrambles) puts the beginner-method average nearer 210 quarter-turns, ranging from 80 to 321.

**Read that last bullet again.** A layer-by-layer solver — the intuitive, "solve it like a human" approach — does *not* pass R4 (≤50 moves average). This is the trap most first attempts fall into: it's the easiest algorithm to write and explain, and it's also the one guaranteed to fail the move-count requirement. See `02-algorithms.md` §A for the full verdict on why it's still useful, just not as the shipped solver.
