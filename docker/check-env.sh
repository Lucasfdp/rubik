#!/usr/bin/env bash
# Smoke test for the rubik container. Run this FIRST, before blaming your code.
#
#   check-env            everything, no window
#   check-env --window   also open a real GL window for a few seconds
#
# Exits non-zero if anything that would stop the MANDATORY part working is broken.
# Graphics problems are warnings only: the mandatory binary links no graphics code,
# so a missing display must never make the whole environment look broken.
set -uo pipefail

pass() { printf '  \033[32mok\033[0m   %s\n' "$1"; }
fail() { printf '  \033[31mFAIL\033[0m %s\n' "$1"; FAILED=1; }
warn() { printf '  \033[33mwarn\033[0m %s\n' "$1"; }
note() { printf '       %s\n' "$1"; }
FAILED=0
WINDOW=0
[ "${1:-}" = "--window" ] && WINDOW=1
T=/tmp/_rubik_check
mkdir -p "$T"

echo "rubik environment check"
echo

# --- platform ----------------------------------------------------------------
echo "platform"
ARCH="$(uname -m)"
printf '  container arch: %s   ' "$ARCH"
case "$ARCH" in
    aarch64) echo "(arm64 — native on an Apple Silicon Mac)" ;;
    x86_64)  echo "(x86_64 — what the 42 machines run)" ;;
    *)       echo "(unexpected)" ;;
esac

EMULATED="no"
if [ "${HOST_ARCH:-}" = "arm64" ] && [ "$ARCH" = "x86_64" ]; then EMULATED="yes"; fi
if [ "${HOST_ARCH:-}" = "amd64" ] && [ "$ARCH" = "aarch64" ]; then EMULATED="yes"; fi
[ -e /proc/sys/fs/binfmt_misc/rosetta ] && EMULATED="rosetta"
grep -qsi qemu /proc/cpuinfo && EMULATED="qemu"
if [ "$EMULATED" != "no" ]; then
    note "running under emulation ($EMULATED) — valgrind and software GL may misbehave"
    note "here; that is why arm64 is the daily driver and amd64 the parity check"
fi

LIBC_VER="$(ldd --version 2>/dev/null | sed -n '1p')"
printf '  libc: %s\n' "${LIBC_VER:-unknown}"
if printf '%s' "$LIBC_VER" | grep -qi musl; then
    fail "musl libc detected — this image must be glibc-based (Debian/Ubuntu, like 42)"
else
    pass "glibc (matches the evaluation platform)"
fi
echo

# --- toolchain ---------------------------------------------------------------
echo "toolchain"
for t in gcc cc make ar ld gdb valgrind git pkg-config; do
    command -v "$t" >/dev/null && pass "$t" || fail "$t missing"
done
command -v gcc >/dev/null && note "$(gcc --version | head -1)"

# Compile with the project's own warning flags, so a toolchain that only works
# with warnings off is caught here rather than in the middle of `make`.
cat > "$T/hello.c" <<'EOF'
#include <stdio.h>
#include <stdlib.h>
int main(void)
{
	char *p = malloc(16);
	if (!p)
		return 1;
	p[0] = 'x';
	free(p);
	return 0;
}
EOF
if gcc -Wall -Wextra -Werror "$T/hello.c" -o "$T/hello" 2>"$T/err"; then
    pass "gcc -Wall -Wextra -Werror compiles"
    "$T/hello" && pass "the binary runs" || fail "binary ran but failed"
else
    fail "compiling failed"; sed 's/^/       /' "$T/err"
fi
echo

# --- raylib build dependencies -----------------------------------------------
# Every header GLFW's X11 backend includes, compiled for real rather than
# stat()ed, so a header that exists but is broken is still caught.
echo "raylib build dependencies (needed for 'make bonus')"
for h in X11/Xlib.h X11/XKBlib.h X11/Xcursor/Xcursor.h X11/extensions/XInput2.h \
         X11/extensions/Xinerama.h X11/extensions/Xrandr.h X11/extensions/shape.h \
         GL/gl.h; do
    if printf '#include <%s>\nint main(void){return 0;}\n' "$h" \
        | gcc -fsyntax-only -x c - 2>/dev/null; then
        pass "$h"
    else
        fail "$h missing — the raylib build will fail"
    fi
done
printf 'int main(void){return 0;}\n' > "$T/link.c"
if gcc "$T/link.c" -o "$T/link" -lGL -lX11 -lm -lpthread -ldl -lrt 2>"$T/err"; then
    pass "links against -lGL -lX11 -lm -lpthread -ldl -lrt (the Makefile's bonus libs)"
else
    fail "cannot link the bonus libraries"; sed 's/^/       /' "$T/err"
fi
if [ -f /workspace/lib/raylib/src/raylib.h ]; then
    pass "lib/raylib sources present"
else
    warn "lib/raylib/src/raylib.h not found — 'make bonus' will try git submodule update"
fi
echo

# --- memory checking ---------------------------------------------------------
echo "memory checking"
if command -v valgrind >/dev/null; then
    printf '       %s\n' "$(valgrind --version)"
    if valgrind -q --leak-check=full --error-exitcode=99 "$T/hello" >"$T/vg" 2>&1; then
        pass "valgrind runs a real binary and finds no leaks"
    elif valgrind --error-exitcode=99 /bin/true >/dev/null 2>&1; then
        warn "valgrind runs trivial programs but failed on the probe"
        sed 's/^/       /' "$T/vg" | head -8
    else
        if [ "$EMULATED" != "no" ]; then
            warn "valgrind cannot run under emulation (expected)"
            note "use the native image for leak checks: ./run.sh --platform native"
        else
            fail "valgrind cannot run here"
        fi
    fi
else
    fail "valgrind missing"
fi
echo 'int main(void){return 0;}' > "$T/asan.c"
if gcc -fsanitize=address "$T/asan.c" -o "$T/asan" 2>/dev/null && "$T/asan" 2>/dev/null; then
    pass "AddressSanitizer available (make debug)"
else
    warn "AddressSanitizer unavailable"
fi
echo

# --- debugger ----------------------------------------------------------------
echo "debugger"
if [ -x "$T/hello" ] && gdb -batch -ex run -ex quit "$T/hello" >"$T/gdb" 2>&1; then
    pass "gdb can run a process (ptrace works)"
else
    if grep -qi 'operation not permitted\|ptrace' "$T/gdb" 2>/dev/null; then
        fail "gdb cannot ptrace — add --cap-add=SYS_PTRACE to docker run"
    else
        warn "gdb did not complete a batch run"
        sed 's/^/       /' "$T/gdb" 2>/dev/null | head -5
    fi
fi
echo

# --- graphics (warnings only) ------------------------------------------------
# Path of a frame: raylib -> GLFW -> Mesa llvmpipe (software GL, in this container)
# -> X11 protocol over TCP -> XQuartz -> your screen. Nothing uses your Mac's GPU.
echo "graphics (optional — only the bonus needs this)"
if [ -z "${DISPLAY:-}" ]; then
    warn "DISPLAY is not set — no window can open"
    if [ "${HOST_OS:-}" = "Darwin" ]; then
        note "on macOS run, once:  ./run.sh --gui-setup   (then re-run this check)"
    else
        note "start the container from a desktop session, or set DISPLAY"
    fi
else
    printf '  DISPLAY=%s\n' "$DISPLAY"
    if timeout 8 xdpyinfo >"$T/xdpy" 2>&1; then
        pass "connected to the X server"
        if grep -q 'GLX' "$T/xdpy"; then
            pass "server advertises GLX"
        else
            warn "server does not advertise GLX (software GL may still work)"
        fi
    else
        warn "cannot connect to the X server"
        sed 's/^/       /' "$T/xdpy" | head -3
        if grep -qi 'authoriz\|No protocol' "$T/xdpy"; then
            note "the server refused us: on the Mac run  ./run.sh --gui-setup"
            note "(it redoes  xhost +localhost, which XQuartz forgets on restart)"
        else
            note "is XQuartz running, and 'Allow connections from network clients' on?"
        fi
    fi

    if [ -r "$T/xdpy" ] && grep -q 'version number' "$T/xdpy"; then
        if timeout 30 glxinfo -B >"$T/glx" 2>&1; then
            REND="$(sed -n 's/^OpenGL renderer string: //p' "$T/glx")"
            CORE="$(sed -n 's/^OpenGL core profile version string: //p' "$T/glx")"
            printf '  renderer: %s\n' "${REND:-unknown}"
            printf '  core profile: %s\n' "${CORE:-none}"
            # raylib on desktop asks for OpenGL 3.3 core.
            MAJ="$(printf '%s' "$CORE" | sed -n 's/^\([0-9]*\)\.\([0-9]*\).*/\1/p')"
            MIN="$(printf '%s' "$CORE" | sed -n 's/^\([0-9]*\)\.\([0-9]*\).*/\2/p')"
            if [ -n "$MAJ" ] && { [ "$MAJ" -gt 3 ] || { [ "$MAJ" -eq 3 ] && [ "${MIN:-0}" -ge 3 ]; }; }; then
                pass "OpenGL core profile >= 3.3 (what raylib requests)"
            else
                warn "no OpenGL 3.3 core profile — raylib's window creation will fail"
                note "try  GALLIUM_DRIVER=softpipe  (slow, but no JIT) or see README.md"
            fi
            printf '%s' "$REND" | grep -qi 'llvmpipe\|softpipe' \
                && pass "rendering in software inside the container (as designed)" \
                || warn "renderer is not Mesa software — unexpected, but not necessarily wrong"
        else
            warn "glxinfo failed"
            sed 's/^/       /' "$T/glx" | head -5
        fi
    fi

    if [ "$WINDOW" -eq 1 ]; then
        echo
        echo "  opening a GL window for 4 seconds — look at your screen"
        timeout 4 glxgears -info >"$T/gears" 2>&1
        RC=$?
        # timeout exits 124 when it had to kill a healthy, still-running program.
        if [ "$RC" -eq 124 ]; then
            pass "glxgears ran for 4s without crashing (the window opened)"
        else
            warn "glxgears exited early (code $RC)"
            sed 's/^/       /' "$T/gears" | head -6
        fi
    else
        note "for a visual test:  check-env --window   (or ./run.sh --window)"
    fi
fi
echo

# --- workspace ---------------------------------------------------------------
echo "workspace"
if [ -d /workspace ]; then
    if touch /workspace/.rubik_write_test 2>/dev/null; then
        rm -f /workspace/.rubik_write_test
        pass "/workspace is writable (bind mount is live)"
    else
        fail "/workspace is not writable — check the -v bind mount"
    fi

    # The cross-platform artifact trap. The bind mount is shared with macOS (Mach-O)
    # and with the *other* container architecture (arm64 vs x86-64). make compares
    # timestamps, not architectures, so it happily tries to link foreign objects.
    artifact_bad() {
        local f="$1" desc members first
        case "$f" in
            *.a)
                members="$(ar t "$f" 2>/dev/null)" || { echo "unreadable archive"; return 0; }
                if grep -q '^__\.SYMDEF' <<<"$members"; then echo "macOS archive"; return 0; fi
                first="$(grep -v '^/' <<<"$members" | head -1)"
                [ -n "$first" ] || return 1
                desc="$(ar p "$f" "$first" 2>/dev/null | file -b -)"
                ;;
            *) desc="$(file -b "$f")" ;;
        esac
        case "$desc" in
            *ELF*)
                case "$ARCH:$desc" in
                    x86_64:*x86-64*|aarch64:*aarch64*) return 1 ;;
                    *) echo "$desc" | cut -c1-60; return 0 ;;
                esac ;;
            *Mach-O*) echo "$desc" | cut -c1-60; return 0 ;;
            *) return 1 ;;
        esac
    }

    BAD=""
    CAND="$(find /workspace/obj /workspace/lib/raylib/src -maxdepth 6 \
                \( -name '*.o' -o -name '*.a' \) 2>/dev/null | head -400)"
    for b in rubik rubik_bonus test_moves test_cubie test_facelet test_coord; do
        [ -f "/workspace/$b" ] && CAND="$CAND
/workspace/$b"
    done
    while IFS= read -r f; do
        [ -n "$f" ] || continue
        if why="$(artifact_bad "$f")"; then
            BAD="$BAD
       ${f#/workspace/}  ($why)"
        fi
    done <<<"$CAND"
    if [ -n "$BAD" ]; then
        fail "build artifacts from another platform/architecture in the bind mount:"
        printf '%s\n' "$BAD" | sed '/^$/d' | head -12
        note "make would try to link these and fail confusingly. Clear them with:"
        note "    make -C docker purge        (from the project root, on the Mac)"
    else
        pass "no foreign object files or binaries left in the tree"
    fi
else
    warn "/workspace missing — started without the bind mount?"
fi
echo

# --- extras ------------------------------------------------------------------
echo "extras"
for t in fish rg strace cppcheck clang xxd tree; do
    command -v "$t" >/dev/null && pass "$t" || warn "$t missing"
done
echo

rm -rf "$T"

if [ "$FAILED" -eq 0 ]; then
    echo "environment looks good."
else
    echo "environment has problems — fix these before writing code."
    exit 1
fi
