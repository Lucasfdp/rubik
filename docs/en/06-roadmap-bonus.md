# Roadmap — Bonus (3D) Part

*(Part of the Rubik prep docs — see `00-overview.md` for the index and `GLOSSARY.md` for term definitions.)*

Same principle as the mandatory roadmap: both of you touch the 3D work. The seam here is **animation engine vs. scene/appearance**, which is a genuine interface — `anim_push(move)`, `anim_update(dt)`, `anim_current_transform(cubie_id)` — not an arbitrary split.

## Sprint 5 — Renderer skeleton (both, pair-programmed, ~2 days)

Together again, for the same reason as Sprint 0: this sets the shared contract for everything after it.

- The build spike: get the chosen stack (raylib, per `03-graphics.md`, or MLX as fallback) actually compiling and linking on a cluster machine. **Do this before writing any real rendering code.** If raylib fails here, switch to MLX *now*, in week one — not discovered in week three when there's no time left to pivot.
- Window, camera, the 26 static cubies placed on the lattice, orbital mouse control, and sticker colours read correctly from a solved logical state.
- Agree the `anim_*` interface signatures precisely, and write them into `DECISIONS.md`.

## Sprint 6 — Split the bonus (~5 days)

| | Dev A | Dev B |
|---|---|---|
| **Owns** | `render/anim.c` — the animation engine | `render/geometry.c` + `render/hud.c` — scene & appearance |
| **Builds** | Face selection by lattice coordinate; 0→±90° interpolation with easing; **commit to logical state on completion**; the move queue; play / pause / step / speed controls | Cubie mesh + sticker-colour mapping driven from the live cube state; flat shading or basic lighting; the HUD (move counter, current move shown in notation, solution progress bar); camera presets |
| **Gate** | Play a 1,000-move random sequence at maximum speed; the final rendered state must equal the logical state **exactly** — zero floating-point drift | Scramble the cube via the renderer's manual-turn UI, dump the resulting cube state, feed it to the solver, and verify it actually solves. This proves the render state and the logical state never diverge |

## Sprint 7 — Named bonus items (~3 days)

Pick from the subject's own bonus list. Ordered here worst-value-first-removed, i.e. do the highest-value items first:

1. **Real-time animated solve** — the headline bonus item. Already done as of Sprint 6.
2. **Integrated scramble generator**, with configurable length/count (e.g. `-g LENGTH`, `-n COUNT` flags). Cheap, explicitly named in the subject's bonus list, and it also doubles as the engine for your own benchmark harness from Sprint 3.
3. **Multi-algorithm selection** (see `02-algorithms.md` §3.E) — add Thistlethwaite as a second algorithm and compare/print both. Dev A takes the algorithm implementation, Dev B takes the comparison UI.
4. **"Humanly understandable" substeps** — annotate the printed solution with phase labels, e.g. `[Phase 1: orient everything] R U2 F' ... [Phase 2: finish in G1] U D2 R2 ...`. Nearly free with Kociemba, since the phase boundary is already known internally. This lands very well at defence.
5. **2×2×2 support** — the cubie model generalises to this fairly directly; the coordinate encoders don't (they'd need reworking). Only attempt this if there's real time left over.
6. **Optimal solver behind an `-optimal` flag with a timeout** (see `02-algorithms.md` §3.D) — the highest-cost item on this list. Do it last, or not at all.

Per the subject, every option flag must be `-`-prefixed, and the mix/scramble input must always remain a valid move sequence regardless of which flags are combined.

## Sprint 8 — Defence rehearsal (both, 1 day)

Swap modules: each of you presents *the other person's* code, from memory, on the whiteboard. Whatever can't be explained on the spot gets fixed or removed before the actual defence. This is also the final checkpoint for R11 — if anything in the mandatory part regressed while building the bonus, the entire bonus is worth zero, so this is the last chance to catch that.
