#!/usr/bin/env bash
# Platform-aware launcher for the rubik build container.
#
# Written for bash 3.2 — the version macOS still ships. No associative arrays,
# no ${var,,}, no mapfile, and the empty-array idiom ${a[@]+"${a[@]}"} because
# bash 3.2 treats an empty array as unset under `set -u`.
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

HOST_OS="$(uname -s)"
HOST_ARCH="$(uname -m)"

die() { echo "$*" >&2; exit 1; }

# Normalise: macOS says arm64, Linux says aarch64, both mean the same thing.
case "$HOST_ARCH" in
    arm64|aarch64) NATIVE_ARCH="arm64" ;;
    x86_64|amd64)  NATIVE_ARCH="amd64" ;;
    *) die "unsupported host architecture: $HOST_ARCH" ;;
esac

PLATFORM="native"
DO_BUILD=0
FORCE_BUILD=0
RUN_CHECK=0
RUN_TEST=""
GUI="auto"
GUI_SETUP=0
CMD=""
START_CMD="/bin/bash"
CHECK_CMD=(check-env)

usage() {
    cat <<EOF
Usage: ./run.sh [options] [-- command...]

Options:
  --build           build the image if it is missing
  --rebuild         force a rebuild even if the image exists
  --check           run check-env inside the container and exit
  --window          --check, plus open a real GL window for a few seconds
  --gui-setup       macOS: one-time XQuartz setup (install check, network
                    clients on, start it, allow the container in)
  --no-gui          don't wire up a display, even if one is available
  --test            make test (unit tests), then exit
  --valgrind        make valgrind (mandatory binary), then exit
  --fish            start fish instead of bash
  --platform ARCH   native (default) | arm64 | amd64
  -h, --help        this message

Platforms:
  native   your own architecture — arm64 on an M-series Mac. Fast, valgrind works.
  amd64    what the 42 machines are. Emulated on a Mac; use it as the pre-defense
           parity check, not as the daily driver.

Examples:
  ./run.sh --build --check          # first-time setup and verification
  ./run.sh --gui-setup              # macOS, once: get XQuartz ready
  ./run.sh --window                 # prove a 3D window can open
  ./run.sh                          # interactive shell in /workspace
  ./run.sh -- make bonus && ./rubik_bonus "R U R' U'"
  ./run.sh --platform amd64 --test  # parity run on the 42 architecture
EOF
}

while [ $# -gt 0 ]; do
    case "$1" in
        --platform) [ $# -ge 2 ] || die "--platform needs a value"; PLATFORM="$2"; shift 2 ;;
        --build)    DO_BUILD=1; shift ;;
        --rebuild)  DO_BUILD=1; FORCE_BUILD=1; shift ;;
        --check)    RUN_CHECK=1; DO_BUILD=1; shift ;;
        --window)   RUN_CHECK=1; DO_BUILD=1; CHECK_CMD=(check-env --window); shift ;;
        --gui-setup) GUI_SETUP=1; shift ;;
        --no-gui)   GUI="no"; shift ;;
        --test)     RUN_TEST="test"; DO_BUILD=1; shift ;;
        --valgrind) RUN_TEST="valgrind"; DO_BUILD=1; shift ;;
        --fish)     START_CMD="/usr/bin/fish"; shift ;;
        -h|--help)  usage; exit 0 ;;
        --)         shift; CMD="$*"; break ;;
        *) echo "unknown option: $1" >&2; usage >&2; exit 1 ;;
    esac
done

# ------------------------------------------------------------------------------
# XQuartz helpers (macOS). Kept above everything else so --gui-setup can run
# without Docker being up.
# ------------------------------------------------------------------------------
# The container connects to XQuartz over TCP (host.docker.internal:6000), so:
#   - XQuartz must be running,
#   - it must accept network clients (defaults key nolisten_tcp = false),
#   - and `xhost +localhost` must have whitelisted the connection.
# You do NOT need to launch anything from an XQuartz terminal — any terminal works.
find_xhost() {
    if [ -n "${XHOST:-}" ]; then echo "$XHOST"; return; fi
    if [ -x /opt/X11/bin/xhost ]; then echo /opt/X11/bin/xhost; return; fi
    command -v xhost 2>/dev/null || true
}
xquartz_installed() { [ -d /Applications/Utilities/XQuartz.app ] || [ -d /opt/X11 ]; }
xquartz_running()   { pgrep -xi xquartz >/dev/null 2>&1; }
x_tcp_open()        { nc -z -w 2 127.0.0.1 6000 >/dev/null 2>&1; }

# A DISPLAY value that reaches the Mac's own X server from *this* shell. A terminal
# opened before XQuartz was installed has no DISPLAY; launchd still knows it.
host_display() {
    local d="${DISPLAY:-}"
    [ -n "$d" ] || d="$(launchctl getenv DISPLAY 2>/dev/null || true)"
    [ -n "$d" ] || d=":0"
    printf '%s' "$d"
}

allow_localhost() {
    local xh
    xh="$(find_xhost)"
    [ -n "$xh" ] || return 1
    DISPLAY="$(host_display)" "$xh" +localhost >/dev/null 2>&1
}

gui_setup() {
    local cur need_restart=0 i=0
    if [ "$HOST_OS" != "Darwin" ]; then
        echo "--gui-setup is for macOS/XQuartz. On Linux your own X server is used"
        echo "directly; try ./run.sh --window."
        exit 0
    fi
    echo "XQuartz setup"
    echo
    if ! xquartz_installed; then
        echo "  XQuartz is not installed. Run:"
        echo "      brew install --cask xquartz"
        echo "  then LOG OUT of macOS and back in (XQuartz needs this once), and rerun"
        echo "  ./run.sh --gui-setup"
        exit 1
    fi
    echo "  ok   XQuartz installed"

    # This is what Settings -> Security -> "Allow connections from network clients"
    # writes. XQuartz ships with it off.
    cur="$(defaults read org.xquartz.X11 nolisten_tcp 2>/dev/null || echo unset)"
    if [ "$cur" != "0" ]; then
        defaults write org.xquartz.X11 nolisten_tcp -bool false
        echo "  set  'Allow connections from network clients' on (was: $cur)"
        need_restart=1
    else
        echo "  ok   network clients already allowed"
    fi

    if xquartz_running && [ "$need_restart" -eq 1 ]; then
        echo
        echo "  XQuartz is running with the OLD setting. Quit it (Cmd-Q while XQuartz is"
        echo "  focused), then rerun ./run.sh --gui-setup"
        exit 1
    fi

    if ! xquartz_running; then
        echo "  ...  starting XQuartz"
        open -a XQuartz
        while [ "$i" -lt 20 ]; do
            if xquartz_running && x_tcp_open; then break; fi
            sleep 1
            i=$((i + 1))
        done
    fi
    if ! xquartz_running; then
        echo "  FAIL XQuartz did not start — open it from Applications/Utilities by hand"
        exit 1
    fi
    if ! x_tcp_open; then
        echo "  FAIL XQuartz is up but not listening on TCP 6000."
        echo "       Quit it completely (Cmd-Q), start it again, then rerun this."
        exit 1
    fi
    echo "  ok   XQuartz is listening on TCP 6000"

    if allow_localhost; then
        echo "  ok   xhost +localhost (the container is allowed in)"
    else
        echo "  FAIL could not run xhost. Try, from the XQuartz terminal:  xhost +localhost"
        exit 1
    fi
    echo
    echo "Ready. Containers will use DISPLAY=host.docker.internal:0."
    echo "Prove it end to end:  ./run.sh --window"
}

if [ "$GUI_SETUP" -eq 1 ]; then
    gui_setup
    exit 0
fi

# ------------------------------------------------------------------------------
# Platform + Docker
# ------------------------------------------------------------------------------
case "$PLATFORM" in
    native)        PLATFORM="$NATIVE_ARCH" ;;
    arm64|aarch64) PLATFORM="arm64" ;;
    amd64|x86_64)  PLATFORM="amd64" ;;
    *) die "--platform must be native, arm64 or amd64 (got: $PLATFORM)" ;;
esac
IMAGE="rubik-env:$PLATFORM"

command -v docker >/dev/null || die "docker not found on PATH"
docker info >/dev/null 2>&1 || die "docker daemon not reachable — is Docker Desktop running?"

EMULATED=0
[ "$PLATFORM" != "$NATIVE_ARCH" ] && EMULATED=1

if [ "$DO_BUILD" -eq 1 ]; then
    if [ "$FORCE_BUILD" -eq 1 ] || ! docker image inspect "$IMAGE" >/dev/null 2>&1; then
        echo ">> building $IMAGE (linux/$PLATFORM)"
        [ "$EMULATED" -eq 1 ] && echo "   emulated build — this will take a while"
        docker build --platform "linux/$PLATFORM" -t "$IMAGE" "$SCRIPT_DIR"
    fi
fi

if ! docker image inspect "$IMAGE" >/dev/null 2>&1; then
    die "image $IMAGE not found. Run: ./run.sh --build"
fi

# ------------------------------------------------------------------------------
# Display wiring
# ------------------------------------------------------------------------------
GUI_ARGS=()
GUI_STATE="off"
GUI_NOTE=""

if [ "$GUI" = "no" ]; then
    GUI_NOTE="disabled with --no-gui"
elif [ "$HOST_OS" = "Darwin" ]; then
    if ! xquartz_installed; then
        GUI_NOTE="XQuartz is not installed — run ./run.sh --gui-setup"
    elif ! xquartz_running; then
        GUI_NOTE="XQuartz is not running — run ./run.sh --gui-setup"
    elif ! x_tcp_open; then
        GUI_NOTE="XQuartz is not accepting network clients — run ./run.sh --gui-setup"
    else
        # xhost state is lost whenever XQuartz restarts, so redo it every launch.
        allow_localhost || GUI_NOTE="xhost +localhost failed; the window may be refused"
        GUI_ARGS=(-e "DISPLAY=host.docker.internal:0")
        GUI_STATE="XQuartz via host.docker.internal:0"
    fi
elif [ "$HOST_OS" = "Linux" ]; then
    if [ -n "${DISPLAY:-}" ] && [ -d /tmp/.X11-unix ]; then
        # The container is root; let it onto this user's X server without opening
        # the server to everyone.
        if command -v xhost >/dev/null 2>&1; then
            xhost +SI:localuser:root >/dev/null 2>&1 || true
        fi
        GUI_ARGS=(-e "DISPLAY=$DISPLAY" -v /tmp/.X11-unix:/tmp/.X11-unix)
        GUI_STATE="host X11 socket ($DISPLAY)"
    else
        GUI_NOTE="no DISPLAY on this host"
    fi
fi

echo ">> host: $HOST_OS/$NATIVE_ARCH   container: linux/$PLATFORM$([ "$EMULATED" -eq 1 ] && echo "  (emulated)")"
if [ "$EMULATED" -eq 1 ]; then
    echo ">> note: emulated amd64. valgrind and Mesa's JIT can be unreliable here —"
    echo "         arm64 (native) is the daily driver, amd64 is the parity check."
fi
if [ "$GUI_STATE" != "off" ]; then
    echo ">> display: $GUI_STATE"
elif [ -n "$GUI_NOTE" ]; then
    echo ">> display: off — $GUI_NOTE  (fine unless you're running the bonus)"
fi

# --cap-add=SYS_PTRACE               gdb and valgrind both need to trace the process
# --security-opt seccomp=unconfined  the default profile blocks some of what
#                                    valgrind and ptrace want; this is a local dev
#                                    box on your own code, not a sandbox
# HOST_ARCH / HOST_OS                let check-env report emulation accurately
DOCKER_ARGS=(--rm --platform "linux/$PLATFORM"
    --cap-add=SYS_PTRACE --security-opt seccomp=unconfined
    -e "HOST_ARCH=$NATIVE_ARCH" -e "HOST_OS=$HOST_OS"
    -v "$PROJECT_ROOT:/workspace" -w /workspace)
DOCKER_ARGS+=(${GUI_ARGS[@]+"${GUI_ARGS[@]}"})
if [ -t 0 ] && [ -t 1 ]; then DOCKER_ARGS+=(-it); else DOCKER_ARGS+=(-i); fi

if [ "$RUN_CHECK" -eq 1 ]; then
    exec docker run "${DOCKER_ARGS[@]}" "$IMAGE" "${CHECK_CMD[@]}"
elif [ -n "$RUN_TEST" ]; then
    exec docker run "${DOCKER_ARGS[@]}" "$IMAGE" make "$RUN_TEST"
elif [ -n "$CMD" ]; then
    exec docker run "${DOCKER_ARGS[@]}" "$IMAGE" /bin/bash -lc "$CMD"
else
    exec docker run "${DOCKER_ARGS[@]}" "$IMAGE" "$START_CMD"
fi
