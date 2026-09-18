# Rubik (42, v5) — Preparation & Decision Docs

**Goal: solve a 3×3×3 cube in ≤50 moves average, ≤3s, in C. Team of 2.**

This is the index. Each linked file covers one decision area in full, with plain-English explanations of the technical terms as they come up. Anything you don't recognize on sight is almost certainly in `GLOSSARY.md` — that file is meant to be opened in a second tab while you read the others.

## Reading order

1. **`01-requirements.md`** — what the subject actually forces us to do, and the numbers that explain *why* (state space size, God's Number, why layer-by-layer fails).
2. **`02-algorithms.md`** — the solver decision. Five options compared, Kociemba's two-phase algorithm recommended.
3. **`03-graphics.md`** — the 3D bonus decision. raylib vs. hand-rolled rasterizer vs. raw OpenGL.
4. **`04-architecture.md`** — how the code is laid out so the anti-cheat rule is structural and two people can work in parallel.
5. **`05-roadmap-mandatory.md`** — sprint-by-sprint plan for the required part.
6. **`06-roadmap-bonus.md`** — sprint-by-sprint plan for the 3D bonus.
7. **`07-defence-prep.md`** — the questions you'll get asked at evaluation, with whiteboard-ready answers.
8. **`08-risks-and-open-decisions.md`** — what could go wrong, and the decisions still left for the two of you to make.
9. **`09-sources.md`** — every claim above that isn't "we decided this" traces back to one of these.
10. **`GLOSSARY.md`** — every technical term used across all the files above, defined from scratch. No prior group-theory or graphics knowledge assumed.

## Recommendation, up front

| Decision | Recommendation | Confidence |
|---|---|---|
| Solver algorithm | **Kociemba two-phase**, on a generic coordinate + IDA* framework | High |
| Fallback / second algorithm | **Thistlethwaite four-phase** (~70% shared code, cheap bonus point) | Medium |
| 3D stack | **raylib** (C99, permissive licence, no system dependencies to install) | High |
| 3D purist alternative | **MiniLibX + our own software rasterizer** | Medium |
| Ruled out | Korf IDA*+PDB as the *shipped* solver; ncurses/ASCII as the bonus | High |

The reasoning behind each row is in `02-algorithms.md` and `03-graphics.md`. If you disagree with either, the trade-off tables there are what to argue from — that's what they're for.

## The one rule that matters most

Before anything else: the solver function must never see the scramble string. It only ever receives a cube *state*. This single design choice is what makes "prove you're not cheating" a one-line answer instead of a defence risk. Full explanation in `01-requirements.md` under "the anti-cheat rule."
