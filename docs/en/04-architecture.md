# Target Architecture

*(Part of the Rubik prep docs — see `00-overview.md` for the index and `GLOSSARY.md` for term definitions.)*

The module boundaries below are chosen so that (a) the anti-cheat rule (see `01-requirements.md`) is enforced *structurally*, by the code's shape, not just by convention — and (b) two people can work on separate pieces at the same time without constantly stepping on each other's changes.

```
src/
  main.c            CLI: arg parsing, option dispatch, output
  parse/
    notation.c      "R2 D' B'" -> t_move[]   (rejects M/E/S/x/y/z)
    validate.c      cube legality: permutation parity, twist %3, flip %2
  cube/
    cubie.c         corner/edge perm + orientation; the authoritative model
    facelet.c       54-sticker <-> cubie conversion (needed by I/O and renderer)
    moves.c         the 18 moves as permutation tables
  coord/
    encode.c        cubie -> twist / flip / slice / cperm / eperm / sperm
    movetable.c     generic: coord x move -> coord
    prune.c         generic: BFS pruning-table builder
  solve/
    ida.c           generic IDA* driver over (coords, prune tables, move set)
    kociemba.c      phase 1 + phase 2 + sub-optimal iteration loop
    thistle.c       (optional) the four-phase variant
  render/           <-- bonus only, never linked into the mandatory binary
    geometry.c      26 cubies on the lattice
    anim.c          move queue, easing, commit-on-complete
    draw.c          stack-specific: raylib OR mlx rasterizer
    hud.c           move count, current move, controls
```

## Two rules that keep this honest

- **`solve/` must not include anything from `render/`, and `render/` must not include anything from `parse/`.** The renderer's only interface to the rest of the program is: receive a cube state, receive a move list. That's the entire contract. This is what makes it structurally impossible for the renderer to somehow leak the original scramble string into the solving process, or vice versa.
- **`make` builds the mandatory binary with zero graphics code linked into it.** `make bonus` builds a *second*, separate binary that includes the renderer. This matters because of R11 (bonus only counts if the mandatory part is perfect): if the graphics half has a bug or fails to build on the evaluator's machine, the mandatory binary is completely unaffected and still compiles and runs. The small duplication in build setup is worth it for that protection.

## Why the split is at the coordinate layer, not somewhere else

The coordinate layer (`coord/encode.c`, specifically the function signature `uint16_t encode_x(const t_cube *)`) is a clean seam for splitting work between two people because it can be agreed on paper *before* either of you writes code, and it's independently testable on both sides: whoever owns the coordinate encoders can write and verify them without needing the search driver to exist yet, and whoever owns the search driver can write and test it against a stub encoder before the real one is finished. See `05-roadmap-mandatory.md` for how the actual sprint-by-sprint split uses this seam.
