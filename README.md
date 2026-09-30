# rubik

A 3×3×3 Rubik's Cube solver in C — a 42 school project, built by a team of
two. The mandatory part solves any valid scramble with Kociemba's two-phase
algorithm and prints the solution; the bonus part adds a second solver
(Thistlethwaite's four-phase algorithm) and a full interactive 3D renderer
(raylib) you can scramble, turn by hand, and watch solve itself.

## The rule that shapes everything

The solver never sees the scramble string — only the resulting cube state.
Parsing the scramble, applying it, and validating the cube all happen before
the solver is ever called, so "prove you're not cheating" is a structural
fact about the code, not a promise. See `docs/en/01-requirements.md` for the
full reasoning.

## Build

```sh
make                  # mandatory binary: rubik
make bonus            # fetches/builds raylib if needed, builds rubik_bonus
make re               # clean and rebuild (mandatory)
make clean            # remove object files
make fclean           # remove objects and both binaries
make fclean-raylib    # also clean raylib's own build output
make help             # full command list, from the Makefile itself
```

No system-wide dependencies to install for the bonus — raylib is pulled in
and built locally as a git submodule (`lib/raylib`) the first time you run
`make bonus`.

On a machine without a matching toolchain (e.g. not on 42's own Linux
image), `docker/` has a container that reproduces the 42 evaluation
environment exactly — see `docker/README.md`.

## Usage

```sh
./rubik "R2 U F' L2 D B R U2 L' F2"           # mandatory — Kociemba only
./rubik_bonus "R2 U F' L2 D B R U2 L' F2"     # bonus — Kociemba by default
./rubik_bonus "..." -a thistlethwaite         # bonus — Thistlethwaite instead
./rubik_bonus -r                              # bonus — open the 3D view on a solved cube
./rubik_bonus "..." -r                        # bonus — open the 3D view, scrambled and animating in
```

The scramble is standard cube notation (`U D L R F B`, `'` for
counter-clockwise, `2` for a double turn), space-separated — see
`docs/en/02a-cube-notation.md` for the full notation and facelet reference.
Both binaries print the solution as a move list; `-r` (bonus only) opens the
3D window instead of printing.

## The 3D view (`-r`)

Opens on the scrambled cube playing the solution back automatically
(**Autoplay**), or on the solved cube ready for you to scramble and turn by
hand (**Manual**) if no scramble was given.

**Autoplay** — the cube plays back a queued scramble or solve on its own:

| Key | Action |
|---|---|
| Space | Pause / resume |
| Right arrow | Step one move (while paused) |
| Up / Down | Speed up / slow down |
| Esc | Stop and drop into Manual |
| A | Toggle the auto-loop demo (scramble → solve → wait → repeat, forever) |

The HUD shows which phase is running (SCRAMBLING / SOLVING) and a
done/total progress bar for whichever one is currently queued.

**Manual** — turn the cube yourself:

| Key | Action |
|---|---|
| `U R F D L B` | Turn that face (hold **Shift** for counter-clockwise, hold **2** for a double turn) |
| Left-drag a sticker | Turn the layer it's on |
| Circle-drag a centre piece | Turn that face |
| `S` | Scramble |
| `Z` / `Y` | Undo / redo |
| Enter | Solve for me (queues the solution and switches to Autoplay) |
| `[` / `]` | Scrub back / forward through the tracked solve or scramble |
| `\` | Reverse the tracked solve/scramble from wherever you are |
| Right-drag or arrow keys | Orbit the camera |
| Mouse wheel | Zoom |
| `5`–`8` | Jump to a preset camera view |
| `P` | Toggle the sticker colour palette (Classic / Vivid) |
| `C` | Toggle rounded corners on the cubies |

## Algorithms

- **Kociemba, two-phase** (default) — the primary solver, built on a
  coordinate-cube + IDA* framework. Reasoning and comparison against the
  alternatives considered: `docs/en/02-algorithms.md`.
- **Thistlethwaite, four-phase** (`-a thistlethwaite`, bonus only) — a
  second, independent solver sharing most of its supporting code with
  Kociemba's. Spec: `docs/en/10-thistlethwaite-spec.md`.

Both take only a cube state as input (see "the rule that shapes everything"
above) and print a move list; neither is faster or "more correct" than the
other, they're just different searches over the same state space.

## Testing

```sh
make test          # mandatory-part unit tests (no raylib dependency)
make test_bonus    # bonus-only tests (currently: the drag-turning math)
make check         # rebuild with -Wpedantic -Wshadow -Wconversion
make valgrind      # run the mandatory binary under valgrind (pass ARGS="...")
make debug         # build with AddressSanitizer + debug symbols
```

`make test` never touches raylib, so it always runs — including on a
grading box that only has the mandatory part's dependencies.

## Project layout

```
src/          mandatory + bonus source (cube/, coord/, parse/, solve/, render/)
include/      matching headers, one directory per module above
tests/        unit tests, one file per module
docs/en/      design docs and decision records (start at 00-overview.md)
docs/es/      the same docs in Spanish
docker/       a container reproducing the 42 evaluation environment
assets/       shaders, sound effects used by the 3D renderer
lib/raylib/   raylib, as a git submodule (built by `make bonus`, not committed)
```

## Further reading

`docs/en/00-overview.md` is the index into every design decision behind
this project — algorithm choice, 3D stack choice, architecture, the full
sprint-by-sprint roadmap, and a glossary of every technical term used. Start
there for the "why", not just the "how" above.
