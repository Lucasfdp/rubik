# 3D Bonus — Stack & Rendering Decision

*(Part of the Rubik prep docs — see `00-overview.md` for the index and `GLOSSARY.md` for term definitions.)*

**Recommendation: raylib**, falling back to MiniLibX + a hand-rolled rasterizer if raylib won't build on the school machines.

## What "a real graphic" has to mean

The subject is explicit and it constrains everything below: *"a series of figures or letters is considered a debug, not a real bonus,"* and *"you can also try to graphically show the cube spin in real time (this would be real nice!)."*

So the bar is: **an actual 3D cube with animated face turns**, not a coloured 2D net, not an ncurses grid of letters. Every option below is judged against that bar.

## 4.0 The rendering model (identical regardless of which stack you pick)

This design is stack-agnostic on purpose, so the stack decision can be made late (or even changed) without rewriting the logic.

1. **Geometry.** 26 visible cubies (or 27 if you include a hidden centre piece for simplicity) sitting at integer lattice positions — a "lattice" here just means a fixed 3D grid, in this case the 3×3×3 arrangement of coordinates `(x, y, z)` where each of x, y, z is one of `{-1, 0, 1}`. Each cubie is a small cube with 6 faces, each face being a flat 4-sided shape called a **quad**. Total: ~162 quads — trivial for any renderer, hand-rolled or GPU-based.
2. **Colour.** Sticker colours are read **from the logical cube state** (the same cubie/facelet model the solver uses), never stored separately inside the renderer. This is deliberate: if the *solver* has a bug, it becomes visually obvious on screen instead of being silently masked by a renderer that's tracking its own separate copy of the colours.
3. **State authority.** The logical cube (the solver's data structure) is the single source of truth. The renderer only ever holds "logical state + one animation currently in progress."
4. **Animating a move, e.g. `R` (turn the right face):** select the 9 cubies whose `x` coordinate is `+1`; rotate that group of 9 as a unit about the X axis, from 0° to −90°, spread smoothly over several frames using **easing** (a curve that makes the motion accelerate/decelerate naturally instead of moving at a constant robotic speed). Once the rotation finishes, **apply the actual permutation to the logical lattice positions and reset the animation's rotation back to zero.** Never let the in-between rotated (interpolated) positions accumulate into the logical state — if you do, tiny floating-point rounding errors build up over hundreds of moves and the visible cube gradually drifts out of sync with the real one.
5. **Move queue.** The solver produces a list of moves; the renderer plays them back one at a time at a configurable speed (milliseconds per move), with pause / step / speed controls.

This is the standard approach used by essentially every mature cube-visualiser: represent the cube as individual cubies with per-face sticker colours, animate turns in the 3D scene, then commit the result back into the logical state once the animation finishes. Selecting which cubies to turn is just a coordinate test (e.g. "is this cubie's x equal to +1?"), then rotating only those.

---

## 4.A raylib ← **recommended**

**raylib** is a cross-platform graphics library, written in C99 (the same C standard you're already using), built on top of OpenGL (the standard graphics API most GPUs support). It has no external dependencies to install separately — everything it needs is bundled — and it's hardware-accelerated (i.e., it uses the graphics card, not just the CPU, so it's fast).

It gives you, ready to use: a `Camera3D` object and orbit-camera controls (so the user can spin the view with the mouse), `BeginMode3D`/`EndMode3D` blocks for drawing in 3D, `DrawCube`/`DrawCubeWires` functions, and a small math library (`raymath`) for the vector/matrix/rotation math you'd otherwise have to write by hand. That's exactly and only what this bonus needs.

| Pros | Cons |
|---|---|
| **Fastest path to a spinning, animated cube** — realistically a day for a working prototype | Not pre-installed on 42 machines; you have to vendor it (bundle a copy in your repo) and build it yourself — see build story below |
| Free, open-source, permissively licensed (zlib/libpng licence) — no licensing argument possible at defence | An evaluator may ask "isn't that a library doing the work for you?" — the answer: it draws quads and manages a window/camera; the cube logic, the lattice, the animation, and every permutation are all yours |
| Pure C99, matches the rest of the project — no C++ or separate build-system split needed | It bundles two other libraries (GLFW for windowing, glad for OpenGL function loading) internally, so the real dependency tree is bigger than "just raylib" |
| `raymath` saves you writing 4×4 matrix / rotation math you don't need to *prove* you can write from scratch | Somewhat less "we built this from nothing" credit than options B or C below |
| Camera, mouse/keyboard input, and on-screen text (for a move-counter HUD) are all included out of the box | |

**Build story for 42 machines (no admin/sudo access).** Clone the raylib repo, then from inside `raylib/src` run `make PLATFORM=PLATFORM_DESKTOP` to build the static library (a `.a` file you link directly into your binary, no system install needed). Vendor it as a git submodule under `lib/raylib` in your repo, link `libraylib.a` locally, and add a Makefile rule so `make bonus` builds it automatically on a fresh checkout. It needs Mesa (the open-source OpenGL implementation) and X11 (the windowing system) development headers to build — specifically `libx11-dev`, `libgl1-mesa-dev`, and `xorg-dev` on a Debian-family Linux box. **Verify these are present on the cluster machines before committing to this option** — this is a 10-minute check, do it first, before writing any render code.

## 4.B MiniLibX + our own software rasterizer ← **the purist alternative**

Project the 162 quads yourselves, entirely by hand: transform each point through model → world → view → projection space (the standard 3D graphics pipeline, done with your own matrix math instead of a library's), cull back-facing quads (skip drawing faces pointing away from the camera), sort the quads by depth (nearest-to-camera last, called the **painter's algorithm** — paint the far things first, then paint over them with closer things, like a real painter would), and fill each quad with a scanline **rasterizer** (a routine that converts a 2D shape into the actual pixels that make it up) writing directly into an MLX image buffer.

| Pros | Cons |
|---|---|
| **Zero justification risk** — MiniLibX (MLX) is the school's own library and does nothing on your behalf here | You write the *entire* 3D pipeline yourselves: matrices, projection, clipping, culling, polygon fill |
| Maximum defence credit — "we wrote the renderer ourselves" is a strong sentence in a 42 evaluation | No GPU acceleration, no free lighting — shading has to be hand-rolled (flat per-face shading is enough and looks fine for this) |
| You've likely done similar groundwork in earlier 42 graphics projects (fdf, cub3d); the mental model should be familiar | MLX behaves slightly differently on macOS vs. Linux, and its event handling is bare-bones |
| 162 quads is few enough that the painter's algorithm — normally a flawed shortcut for complex scenes — has no pathological case here: the cubies never overlap or interpenetrate, so sorting by centre-distance from the camera is genuinely correct | Real risk of running out of time and shipping *no* bonus at all if this proves harder than expected |
| No external dependency, no build spike, no submodule to manage | Anti-aliasing (smoothing jagged edges), smooth camera motion, and general polish all cost real, non-trivial time |

**This is a real contender, not a consolation prize.** MLX is deliberately minimal — that's *why* writing a proper 3D pipeline on top of it scores well at defence. Choose this option if you want the graphics work itself to be part of what you're learning, and you're willing to accept a slower start in exchange.

## 4.C Raw OpenGL 3.3 core + GLFW + glad

OpenGL is the graphics API itself (what raylib and MLX both ultimately sit on top of, one way or another). GLFW handles window creation and input; glad loads the actual OpenGL function pointers at runtime (a necessary bit of plumbing on most platforms). Using them directly means writing your own vertex/fragment shaders (small programs that run on the GPU to control how things are drawn), and manually managing VAOs/VBOs/EBOs — buffers that hold your 3D geometry data on the GPU (see glossary for the full breakdown of these three).

| Pros | Cons |
|---|---|
| Most control of any option; a real shader pipeline; the most transferable graphics-programming skill | **3–5× the setup work of raylib** before you even see your first triangle on screen |
| Genuinely impressive to a graphics-minded evaluator | Shader compilation and error handling, depth buffering, and your own matrix math all need to be built from scratch |
| GLFW and glad are both small, well-documented dependencies | Highest risk of the bonus being unfinished when the mandatory deadline arrives |

**Verdict:** only worth it if the mandatory part finishes early and you specifically want the OpenGL learning experience. Worth knowing: raylib already *is* GLFW + glad underneath, wrapped in a much friendlier layer — so choosing this option is essentially "raylib, minus the convenience."

## 4.D SDL2 + OpenGL

SDL2 is another windowing/input library, similar in role to GLFW.

| Pros | Cons |
|---|---|
| SDL2 is somewhat more likely to already be installed than raylib | Still requires all of option C's OpenGL work — SDL2 only replaces the windowing part, not the rendering pipeline |
| Better input/audio/gamepad support than GLFW (irrelevant for this project) | Heavier API than GLFW for what's actually needed here |

**Verdict:** no real advantage over raylib for this project. Skip it.

## 4.E ncurses / ASCII / coloured net

**Ruled out directly by the subject text**: *"a series of figures or letters is considered a debug, not a real bonus."* You can still build a coloured 2D net (the cube "unfolded" flat) — just treat it purely as a **debug tool** during development, never present it as the bonus itself.

## 4.F C solver piping to a web viewer (three.js)

Have the C solver output move lists that a separate JavaScript/three.js (a popular browser 3D library) web page consumes and animates.

| Pros | Cons |
|---|---|
| Prettiest result per hour invested | Two separate languages and codebases; reads as dodging the actual bonus requirement (which implies the graphics should be part of the C project) |
| Trivially shareable as a README gif | Needs a browser running at defence time; a more brittle demo setup |

**Verdict:** not a substitute for the actual bonus. Possibly worth doing *afterward*, purely as a nice README demo, once everything else is finished.

## 4.G Graphics decision matrix

| | Time to first spinning cube | Justification risk | Defence credit | External deps | Risk of not shipping |
|---|---|---|---|---|---|
| **A** raylib | ~1 day | Low | Medium | raylib (vendored) | **Low** |
| **B** MLX + own rasterizer | ~4–6 days | **None** | **High** | none | Medium-high |
| **C** OpenGL + GLFW | ~3–4 days | Low | High | GLFW, glad | High |
| **D** SDL2 + OpenGL | ~3–4 days | Low | High | SDL2 | High |
| **E** ncurses | ~1 day | — | **Fails the bar** | ncurses | — |
| **F** three.js viewer | ~1 day | Medium | Low | node/browser | Low |

**Decision rule:** if the raylib build spike on a cluster machine succeeds in under an hour, take **A**. If it fights you (missing X11/Mesa headers, no write access anywhere useful), take **B** instead — you lose some visual polish, but you gain a stronger "we built this ourselves" story.
