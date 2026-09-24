# rubik build container

A glibc Linux environment matching the 42 evaluation platform, with the C
toolchain, valgrind/gdb, and everything raylib needs for the 3D bonus. Adapted
from the libasm container.

## Quick start

```sh
cd docker
./run.sh --build --check      # build the image and verify it
./run.sh                      # interactive shell in /workspace
./run.sh --test               # unit tests
./run.sh --valgrind           # valgrind the mandatory binary
```

Or `make build && make check && make shell` from this folder. `run.sh` finds
the project root itself, so it works from anywhere.

Inside the shell, work exactly as on a 42 machine: `make`, `make bonus`,
`make valgrind`, `./rubik "R U R' U'"`.

---

## Which platform?

Unlike libasm, rubik is plain C, so there is a real choice — and it is the same
one as ft_ping:

| | `native` (arm64 on your M2) | `amd64` |
|---|---|---|
| Speed | full speed | emulated, slow |
| valgrind | works | may not run |
| Software OpenGL (bonus) | fast enough | slow, JIT can misbehave |
| Matches 42 machines | same glibc, different CPU | same CPU |

Default is native. Before you defend, do one parity run on the 42 architecture:

```sh
./run.sh --platform amd64 --build --test
```

Docker Desktop -> Settings -> General -> **Use Rosetta for x86_64/amd64
emulation** makes the amd64 image noticeably faster.

---

## Seeing the 3D window on your Mac (bonus only)

The **mandatory** part has no graphics code and needs none of this. The bonus
(`make bonus` -> `rubik_bonus`) opens a raylib window, and a Linux container
has no screen, so it draws on XQuartz on your Mac.

### One-time setup

```sh
brew install --cask xquartz     # then log out of macOS and back in, once
./run.sh --gui-setup            # network clients on, start XQuartz, allow the container in
./run.sh --window               # opens a spinning-gears window for 4 seconds
```

After that, `run.sh` re-checks XQuartz and re-runs `xhost +localhost` for you on
every launch (XQuartz forgets it whenever it restarts), and passes
`DISPLAY=host.docker.internal:0` into the container. It prints a one-line
`display:` status so you can see whether the window will work.

### You do NOT need an XQuartz terminal

Run `./run.sh` from Terminal, iTerm or VS Code as normal. The container connects
to XQuartz over the network; it only needs XQuartz to be *running*. Nothing has
to be launched from inside XQuartz's own xterm.

### How a frame gets to your screen

```
raylib -> GLFW -> Mesa llvmpipe (software OpenGL, inside the container)
       -> X11 protocol over TCP -> XQuartz -> your display
```

Your Mac's GPU is not involved, and that is deliberate. XQuartz's own OpenGL
support (GLX) is limited to old OpenGL, and raylib asks for OpenGL 3.3 core.
Rendering in software inside the container sidesteps that completely. The cost
is speed: a cube of a few hundred quads is fine, but keep the window modest
(around 800x600) and don't expect 4K at 60 fps.

### If something goes wrong

| Symptom | Fix |
|---|---|
| `display: off — XQuartz is not running` | `./run.sh --gui-setup` |
| `Authorization required` / `cannot open display` | `./run.sh --gui-setup` (redoes `xhost +localhost`) |
| Still refused after that | quit XQuartz fully (Cmd-Q), reopen, `--gui-setup` again |
| `no OpenGL 3.3 core profile` in check-env | inside the shell try `GALLIUM_DRIVER=softpipe ./rubik_bonus ...` — slow, no JIT |
| Window opens black or frozen | check-env's `glxinfo` section; on the amd64 image, switch to native |
| `GLFW: Failed to create X11 window` | DISPLAY is unset in the container: check `echo $DISPLAY`, rerun without `--no-gui` |

Don't fix a refused connection with plain `xhost +` — that lets any machine on
your network draw on your screen. `xhost +localhost` is the scoped version and is
what the script uses.

---

## First thing, every session

```sh
./run.sh --check
```

Verifies: architecture and emulation; glibc not musl; gcc compiles with the
project's `-Wall -Wextra -Werror`; every X11/GL header raylib compiles against
(`Xlib`, `XKBlib`, `Xcursor`, `XInput2`, `Xinerama`, `Xrandr`, `shape`, `gl`);
that the bonus link libs resolve; valgrind runs a real binary and finds no leaks;
gdb can ptrace; the display connection, GLX and an OpenGL 3.3 core profile; and
that no object files from another platform are lying in the tree.

Graphics problems are **warnings**, not failures — a missing display must not
make the mandatory part look broken. It exits non-zero only for things that stop
the mandatory build.

---

## Gotchas

**Build artifacts collide across platforms.** The project folder is shared
between macOS, the arm64 container and the amd64 container. `make` compares
timestamps, not architectures, so it will happily try to link a macOS (Mach-O)
object against Linux ones and fail confusingly. This bites in *both* directions,
and also whenever you switch `--platform`. `check-env` names the offending files;
the fix is:

```sh
make -C docker purge
```

It deletes build outputs only (`obj/`, the binaries, raylib's `.o` files and
`libraylib.a`) — all gitignored and regenerated by the next `make`.

**raylib is built inside the container, into `lib/raylib/src`.** First `make
bonus` takes a minute; after that it's cached until you purge or switch platform.

**Don't add `--user`.** It costs you the ptrace capability gdb and valgrind need.
If root-owned files on a Linux host bother you, use `make fix-perms`.

**Don't swap to Alpine.** musl is not the libc you are graded against.

**`--rm` is intentional.** Containers are disposable; anything worth keeping
belongs in the Dockerfile.

---

## Files

| File | Purpose |
|---|---|
| `Dockerfile` | The image: ubuntu:22.04, C toolchain, valgrind/gdb, raylib's X11 + GL headers, Mesa software GL. |
| `run.sh` | Launcher. Picks the platform, wires up XQuartz (or a Linux X server), passes ptrace caps. |
| `check-env.sh` | Smoke test, baked into the image as `check-env`. |
| `Makefile` | Optional convenience targets around `run.sh`. |
| `fish/config.fish` | `m`, `mb`, `t`, `vg`, `gl` helpers for the fish shell. |
| `valgrind.supp` | Deliberately empty suppression file. |
| `.dockerignore` | Keeps the build context to what the image copies. |
