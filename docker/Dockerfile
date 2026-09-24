# rubik build environment — matches the 42 evaluation platform (Ubuntu + glibc).
#
# Unlike libasm, this project is plain C, so there is NO architecture constraint:
# on an Apple Silicon Mac the native arm64 image is the daily driver (fast, and
# valgrind works), and amd64 is the pre-defense parity check, since the 42
# machines are x86_64. run.sh picks native by default; --platform amd64 flips it.
#
# The bonus renderer is raylib (vendored in lib/raylib, built by `make bonus`).
# On Linux raylib uses GLFW's X11 backend and asks for OpenGL 3.3, so the image
# carries the X11 + GL development headers it compiles against.
#
# DISPLAY STORY (macOS): nothing in this container talks to your Mac's GPU.
# Mesa renders in software (llvmpipe) inside the container and the finished
# frames are drawn on XQuartz over the network. That is deliberate: XQuartz's
# own GLX path tops out at old OpenGL, and raylib wants 3.3 core. See README.md.
#
# Do NOT swap this for Alpine. musl is not the libc you are graded against, and
# valgrind and raylib's Linux path are both glibc-shaped.
#
# Pinned, not :latest — a floating base tag means your environment silently
# changes under you mid-project.
FROM ubuntu:22.04
ARG TARGETARCH
LABEL org.opencontainers.image.title="rubik build environment" \
      org.opencontainers.image.description="glibc Linux for 42 rubik (C + raylib bonus)" \
      rubik.arch="${TARGETARCH}"

ARG DEBIAN_FRONTEND=noninteractive

# Toolchain and the diagnostics the mandatory part is graded with.
#   build-essential  gcc/make/ar/ld — the Makefile needs a compiler on PATH
#   clang            second opinion on warnings; -fsanitize=address fallback
#   valgrind         subject R6: no leak, no segfault, no double free
#   gdb              step through solver bugs; needs --cap-add=SYS_PTRACE
#   strace / cppcheck / binutils / file / xxd   inspection and linting
#   git              the Makefile runs `git submodule update` for raylib
#
# raylib (PLATFORM_DESKTOP -> GLFW -> X11) compile-time dependencies. These map to
# the headers GLFW's X11 backend actually includes:
#   libx11-dev libxrandr-dev libxinerama-dev libxcursor-dev libxi-dev
#   libxext-dev (X11/extensions/shape.h)   libxkbcommon-dev
#   libgl1-mesa-dev (GL/gl.h, -lGL)        libglu1-mesa-dev
#
# Runtime rendering + display tools:
#   libgl1-mesa-dri  the software rasteriser (llvmpipe) — without it, no GL at all
#   mesa-utils       glxinfo / glxgears, used by check-env to prove GL works
#   x11-utils        xdpyinfo, to prove the X connection works
RUN apt-get update && apt-get install -y --no-install-recommends \
        build-essential \
        make \
        binutils \
        libc6-dev \
        clang \
        gdb \
        valgrind \
        strace \
        cppcheck \
        pkg-config \
        file \
        xxd \
        git \
        ca-certificates \
        curl \
        libx11-dev \
        libxrandr-dev \
        libxinerama-dev \
        libxcursor-dev \
        libxi-dev \
        libxext-dev \
        libxkbcommon-dev \
        libgl1-mesa-dev \
        libglu1-mesa-dev \
        libgl1-mesa-dri \
        mesa-utils \
        x11-utils \
        python3 \
        ripgrep \
        tree \
        vim \
        nano \
        less \
        man-db \
        fish \
    && rm -rf /var/lib/apt/lists/*

# The repo is a bind mount owned by a different uid than root, so git refuses to
# touch it ("dubious ownership") and `git submodule update` would die. This is a
# single-user throwaway dev container, so trusting every path is fine.
RUN git config --system --add safe.directory '*'

WORKDIR /workspace

COPY check-env.sh /usr/local/bin/check-env
RUN chmod +x /usr/local/bin/check-env

# Interactive niceties for fish. Deliberately not the CMD: scripts and
# `run.sh -- cmd` expect a POSIX shell, so bash stays the default and fish is opt-in.
COPY fish/config.fish /root/.config/fish/config.fish

# Empty on purpose — a leak you suppressed is a leak you still have at defense.
COPY valgrind.supp /etc/valgrind/rubik.supp
ENV VALGRIND_OPTS="--suppressions=/etc/valgrind/rubik.supp"

# Software OpenGL. Set here (not only in run.sh) so a hand-started container
# behaves the same. GALLIUM_DRIVER pins llvmpipe rather than letting Mesa guess.
ENV LIBGL_ALWAYS_SOFTWARE=1 \
    GALLIUM_DRIVER=llvmpipe

CMD ["/bin/bash"]
