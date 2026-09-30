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


Invalid read of size 8
==786633==    at 0x40286A8: strncmp (strcmp.S:172)
==786633==    by 0x400668D: is_dst (dl-load.c:216)
==786633==    by 0x400810E: _dl_dst_count (dl-load.c:253)
==786633==    by 0x400810E: expand_dynamic_string_token (dl-load.c:395)
==786633==    by 0x40082B7: fillin_rpath.isra.0 (dl-load.c:483)
==786633==    by 0x4008602: decompose_rpath (dl-load.c:654)
==786633==    by 0x400ABF5: cache_rpath (dl-load.c:696)
==786633==    by 0x400ABF5: cache_rpath (dl-load.c:677)
==786633==    by 0x400ABF5: _dl_map_object (dl-load.c:2165)
==786633==    by 0x4003494: openaux (dl-deps.c:64)
==786633==    by 0x4C93B57: _dl_catch_exception (dl-error-skeleton.c:208)
==786633==    by 0x4003C7B: _dl_map_object_deps (dl-deps.c:248)
==786633==    by 0x400EA0E: dl_open_worker_begin (dl-open.c:592)
==786633==    by 0x4C93B57: _dl_catch_exception (dl-error-skeleton.c:208)
==786633==    by 0x400DF99: dl_open_worker (dl-open.c:782)
==786633==  Address 0x5075349 is 9 bytes inside a block of size 15 alloc'd
==786633==    at 0x4848899: malloc (in /usr/libexec/valgrind/vgpreload_memcheck-amd64-linux.so)
==786633==    by 0x40271DF: malloc (rtld-malloc.h:56)
==786633==    by 0x40271DF: strdup (strdup.c:42)
==786633==    by 0x4008594: decompose_rpath (dl-load.c:629)
==786633==    by 0x400ABF5: cache_rpath (dl-load.c:696)
==786633==    by 0x400ABF5: cache_rpath (dl-load.c:677)
==786633==    by 0x400ABF5: _dl_map_object (dl-load.c:2165)
==786633==    by 0x4003494: openaux (dl-deps.c:64)
==786633==    by 0x4C93B57: _dl_catch_exception (dl-error-skeleton.c:208)
==786633==    by 0x4003C7B: _dl_map_object_deps (dl-deps.c:248)
==786633==    by 0x400EA0E: dl_open_worker_begin (dl-open.c:592)
==786633==    by 0x4C93B57: _dl_catch_exception (dl-error-skeleton.c:208)
==786633==    by 0x400DF99: dl_open_worker (dl-open.c:782)
==786633==    by 0x4C93B57: _dl_catch_exception (dl-error-skeleton.c:208)
==786633==    by 0x400E34D: _dl_open (dl-open.c:883)
==786633== 
==786633== Invalid read of size 8
==786633==    at 0x40286A8: strncmp (strcmp.S:172)
==786633==    by 0x400668D: is_dst (dl-load.c:216)
==786633==    by 0x4007F79: _dl_dst_substitute (dl-load.c:295)
==786633==    by 0x40082B7: fillin_rpath.isra.0 (dl-load.c:483)
==786633==    by 0x4008602: decompose_rpath (dl-load.c:654)
==786633==    by 0x400ABF5: cache_rpath (dl-load.c:696)
==786633==    by 0x400ABF5: cache_rpath (dl-load.c:677)
==786633==    by 0x400ABF5: _dl_map_object (dl-load.c:2165)
==786633==    by 0x4003494: openaux (dl-deps.c:64)
==786633==    by 0x4C93B57: _dl_catch_exception (dl-error-skeleton.c:208)
==786633==    by 0x4003C7B: _dl_map_object_deps (dl-deps.c:248)
==786633==    by 0x400EA0E: dl_open_worker_begin (dl-open.c:592)
==786633==    by 0x4C93B57: _dl_catch_exception (dl-error-skeleton.c:208)
==786633==    by 0x400DF99: dl_open_worker (dl-open.c:782)
==786633==  Address 0x5075349 is 9 bytes inside a block of size 15 alloc'd
==786633==    at 0x4848899: malloc (in /usr/libexec/valgrind/vgpreload_memcheck-amd64-linux.so)
==786633==    by 0x40271DF: malloc (rtld-malloc.h:56)
==786633==    by 0x40271DF: strdup (strdup.c:42)
==786633==    by 0x4008594: decompose_rpath (dl-load.c:629)
==786633==    by 0x400ABF5: cache_rpath (dl-load.c:696)
==786633==    by 0x400ABF5: cache_rpath (dl-load.c:677)
==786633==    by 0x400ABF5: _dl_map_object (dl-load.c:2165)
==786633==    by 0x4003494: openaux (dl-deps.c:64)
==786633==    by 0x4C93B57: _dl_catch_exception (dl-error-skeleton.c:208)
==786633==    by 0x4003C7B: _dl_map_object_deps (dl-deps.c:248)
==786633==    by 0x400EA0E: dl_open_worker_begin (dl-open.c:592)
==786633==    by 0x4C93B57: _dl_catch_exception (dl-error-skeleton.c:208)
==786633==    by 0x400DF99: dl_open_worker (dl-open.c:782)
==786633==    by 0x4C93B57: _dl_catch_exception (dl-error-skeleton.c:208)
==786633==    by 0x400E34D: _dl_open (dl-open.c:883)
==786633== 
INFO: DISPLAY: Trying to enable VSYNC
INFO: DISPLAY: Device initialized successfully 
INFO:     > Display size: 3840 x 2160
INFO:     > Screen size:  1280 x 720
INFO:     > Render size:  1280 x 720
INFO:     > Viewport offsets: 0, 0
INFO: GLAD: OpenGL extensions loaded successfully
INFO: GL: Supported extensions count: 310
INFO: GL: OpenGL device information:
INFO:     > Vendor:   AMD
INFO:     > Renderer: AMD Radeon RX 470 Graphics (polaris10, LLVM 15.0.7, DRM 3.42, 5.15.0-191-generic)
INFO:     > Version:  4.6 (Compatibility Profile) Mesa 23.2.1-1ubuntu3.1~22.04.4
INFO:     > GLSL:     4.60
INFO: GL: VAO extension detected, VAO functions loaded successfully
INFO: GL: NPOT textures extension detected, full NPOT textures supported
INFO: GL: DXT compressed textures supported
INFO: GL: ETC2/EAC compressed textures supported
INFO: PLATFORM: DESKTOP (GLFW - X11): Initialized successfully
INFO: TEXTURE: [ID 1] Texture loaded successfully (1x1 | R8G8B8A8 | 1 mipmaps)
INFO: TEXTURE: [ID 1] Default texture loaded successfully
INFO: SHADER: [ID 1] Vertex shader compiled successfully
INFO: SHADER: [ID 2] Fragment shader compiled successfully
INFO: SHADER: [ID 3] Program shader loaded successfully
INFO: SHADER: [ID 3] Default shader loaded successfully
INFO: RLGL: Render batch vertex buffers loaded successfully in RAM (CPU)
INFO: RLGL: Render batch vertex buffers loaded successfully in VRAM (GPU)
INFO: RLGL: Default OpenGL state initialized successfully
INFO: TEXTURE: [ID 2] Texture loaded successfully (128x128 | GRAY_ALPHA | 1 mipmaps)
INFO: FONT: Default font loaded successfully (224 glyphs)
INFO: SYSTEM: Working Directory: /goinfre/luferna3/rubik
INFO: TIMER: Target time per frame: 16.667 milliseconds
INFO: FILEIO: [assets/shaders/glsl120/lighting.vs] Text file loaded successfully
INFO: FILEIO: [assets/shaders/glsl120/lighting.fs] Text file loaded successfully
INFO: SHADER: [ID 4] Vertex shader compiled successfully
INFO: SHADER: [ID 5] Fragment shader compiled successfully
INFO: SHADER: [ID 6] Program shader loaded successfully
INFO: AUDIO: Device initialized successfully
INFO:     > Backend:       miniaudio | PulseAudio
INFO:     > Format:        32-bit IEEE Floating Point -> 16-bit Signed Integer
INFO:     > Channels:      2 -> 2
INFO:     > Sample rate:   44100 -> 44100
INFO:     > Periods size:  3304
INFO: FILEIO: [assets/sfx/turn.wav] File loaded successfully
INFO: WAVE: Data loaded successfully (44100 Hz, 16 bit, 1 channels)
INFO: AUDIO: Device closed successfully
INFO: SHADER: [ID 6] Unloaded shader program data from VRAM (GPU)
INFO: TEXTURE: [ID 2] Unloaded texture data from VRAM (GPU)
INFO: SHADER: [ID 3] Default shader unloaded successfully
INFO: TEXTURE: [ID 1] Default texture unloaded successfully
INFO: Window closed successfully
==786633== 
==786633== HEAP SUMMARY:
==786633==     in use at exit: 404,184 bytes in 3,318 blocks
==786633==   total heap usage: 102,398 allocs, 99,080 frees, 43,028,147 bytes allocated
==786633== 
==786633== 0 bytes in 3 blocks are definitely lost in loss record 1 of 2,349
==786633==    at 0x4848899: malloc (in /usr/libexec/valgrind/vgpreload_memcheck-amd64-linux.so)
==786633==    by 0x4005A22: malloc (rtld-malloc.h:56)
==786633==    by 0x4005A22: _dl_find_object_update (dl-find_object.c:791)
==786633==    by 0x400ECCF: dl_open_worker_begin (dl-open.c:735)
==786633==    by 0x4C93B57: _dl_catch_exception (dl-error-skeleton.c:208)
==786633==    by 0x400DF99: dl_open_worker (dl-open.c:782)
==786633==    by 0x4C93B57: _dl_catch_exception (dl-error-skeleton.c:208)
==786633==    by 0x400E34D: _dl_open (dl-open.c:883)
==786633==    by 0x4BAF5FB: dlopen_doit (dlopen.c:56)
==786633==    by 0x4C93B57: _dl_catch_exception (dl-error-skeleton.c:208)
==786633==    by 0x4C93C22: _dl_catch_error (dl-error-skeleton.c:227)
==786633==    by 0x4BAF0ED: _dlerror_run (dlerror.c:138)
==786633==    by 0x4BAF687: dlopen_implementation (dlopen.c:71)
==786633==    by 0x4BAF687: dlopen@@GLIBC_2.34 (dlopen.c:81)
==786633== 
==786633== 408 bytes in 1 blocks are definitely lost in loss record 2,312 of 2,349
==786633==    at 0x484DA83: calloc (in /usr/libexec/valgrind/vgpreload_memcheck-amd64-linux.so)
==786633==    by 0x4A409CF: _XimOpenIM (in /usr/lib/x86_64-linux-gnu/libX11.so.6.4.0)
==786633==    by 0x4A4657C: _XimRegisterIMInstantiateCallback (in /usr/lib/x86_64-linux-gnu/libX11.so.6.4.0)
==786633==    by 0x4A32BAC: XRegisterIMInstantiateCallback (in /usr/lib/x86_64-linux-gnu/libX11.so.6.4.0)
==786633==    by 0x4973EA: _glfwInitX11 (in /goinfre/luferna3/rubik/rubik_bonus)
==786633==    by 0x493B82: glfwInit (in /goinfre/luferna3/rubik/rubik_bonus)
==786633==    by 0x436352: InitPlatform (in /goinfre/luferna3/rubik/rubik_bonus)
==786633==    by 0x43726F: InitWindow (in /goinfre/luferna3/rubik/rubik_bonus)
==786633==    by 0x40CF4D: render_run (in /goinfre/luferna3/rubik/rubik_bonus)
==786633==    by 0x407F53: main (in /goinfre/luferna3/rubik/rubik_bonus)
==786633== 
==786633== LEAK SUMMARY:
==786633==    definitely lost: 408 bytes in 4 blocks
==786633==    indirectly lost: 0 bytes in 0 blocks
==786633==      possibly lost: 0 bytes in 0 blocks
==786633==    still reachable: 403,776 bytes in 3,314 blocks
==786633==         suppressed: 0 bytes in 0 blocks
==786633== Reachable blocks (those to which a pointer was found) are not shown.
==786633== To see them, rerun with: --leak-check=full --show-leak-kinds=all
