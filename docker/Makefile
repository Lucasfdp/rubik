# Convenience wrapper around run.sh. Everything here is optional — run.sh works alone.
#
# Pick the architecture with PLATFORM:
#   make shell                     native (arm64 on an M-series Mac) — daily driver
#   make shell PLATFORM=amd64      x86_64, emulated — pre-defense parity check

PLATFORM ?= native
RUN      := ./run.sh --platform $(PLATFORM)
UNAME_M  := $(shell uname -m)
ifeq ($(filter arm64 aarch64,$(UNAME_M)),)
HOSTNOTE := x86_64 host
else
HOSTNOTE := arm64 host
endif

.PHONY: help build rebuild shell fish check gui-setup gui-test vnc vnc-fish \
        vnc-window mandatory bonus test valgrind purge clean fix-perms

help:
	@echo "rubik docker environment  ($(HOSTNOTE), PLATFORM=$(PLATFORM))"
	@echo
	@echo "  make build         build the image"
	@echo "  make rebuild       force a rebuild"
	@echo "  make check         verify the environment    <- run this first"
	@echo "  make shell         interactive bash shell    <- daily driver"
	@echo "  make fish          interactive fish shell"
	@echo "  make gui-setup     macOS, once: get XQuartz ready for the 3D window"
	@echo "  make gui-test      open a real GL window for a few seconds"
	@echo "  make vnc           shell with Xvfb+VNC instead of XQuartz  <- if gui-test fails"
	@echo "  make vnc-fish      same, fish shell"
	@echo "  make vnc-window    Xvfb+VNC, open a real GL window for a few seconds"
	@echo "  make mandatory     build the mandatory binary (no graphics linked)"
	@echo "  make bonus         build rubik_bonus (raylib) inside the container"
	@echo "  make test          unit tests"
	@echo "  make valgrind      valgrind the mandatory binary"
	@echo "  make purge         delete build outputs (fixes cross-platform clashes)"
	@echo "  make clean         remove the image"
	@echo "  make fix-perms     chown build artifacts back to you (Linux hosts)"
	@echo
	@echo "  add PLATFORM=amd64 to any of these for the 42 architecture"

build:
	@$(RUN) --build -- true

rebuild:
	@$(RUN) --rebuild -- true

shell:
	@$(RUN)

fish:
	@$(RUN) --fish

check:
	@$(RUN) --check

gui-setup:
	@./run.sh --gui-setup

gui-test:
	@$(RUN) --window

# Xvfb + VNC instead of XQuartz — see README.md's "Xvfb + VNC" section. Use
# this when gui-setup/gui-test still fail: it is a known modern-Mesa vs
# XQuartz incompatibility (indirect/software GLX support Mesa dropped), not
# something --gui-setup can fix by retrying.
vnc:
	@$(RUN) --vnc

vnc-fish:
	@$(RUN) --vnc --fish

vnc-window:
	@$(RUN) --vnc --window

mandatory:
	@$(RUN) -- make

bonus:
	@$(RUN) -- make bonus

test:
	@$(RUN) --test

valgrind:
	@$(RUN) --valgrind

# Build outputs only — everything listed is gitignored and regenerable. Run it
# whenever the tree holds objects from another platform: macOS (Mach-O), or the
# other container architecture (arm64 vs x86-64). Sources are never touched.
purge:
	@$(RUN) -- 'rm -rf obj && rm -f rubik rubik_bonus test_moves test_cubie test_facelet test_coord && find lib/raylib/src -maxdepth 1 \( -name "*.o" -o -name "libraylib.a" \) -delete && echo "build outputs removed"'

clean:
	-docker image rm rubik-env:arm64 rubik-env:amd64 2>/dev/null || true

# On a Linux host the container runs as root, so anything it writes into the bind
# mount ends up root-owned on your machine. macOS/Docker Desktop maps ownership for
# you, so this is a no-op there.
fix-perms:
	@if [ "$$(uname -s)" = "Linux" ]; then \
		sudo chown -R "$$(id -u):$$(id -g)" .. && echo "ownership restored"; \
	else \
		echo "not needed on $$(uname -s)"; \
	fi
