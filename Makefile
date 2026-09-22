# =============================================================================
#  rubik
#
#  Source layout (see docs/en/04-architecture.md for the full rationale):
#    include/        one header per module, plus the rubik.h umbrella
#    src/parse/      argv/notation -> t_cube, and legality validation
#    src/cube/       cubie + facelet models, the 18 move tables
#    src/coord/      coordinate encoders, move-table & pruning-table builders
#    src/solve/      IDA*, Kociemba two-phase, (optional) Thistlethwaite
#    src/render/     3D bonus ONLY — raylib. Never linked into the mandatory
#                     binary; this is structural, not a promise (see below).
#    tests/          standalone unit tests, one binary each, module-scoped
#    lib/raylib/     vendored as a git submodule, built locally, never installed
#
#  Sources are DISCOVERED, not listed: adding a .c anywhere under src/ picks
#  it up with no edit here. Object files mirror the source tree under obj/.
#
#  Two binaries, on purpose (R11: bonus only counts if mandatory is perfect):
#    make        ->  $(NAME)        mandatory only, src/render/ excluded
#    make bonus  ->  $(NAME_BONUS)  mandatory + src/render/, linked against raylib
#  `make` never touches raylib and never fails because of it.
# =============================================================================

NAME		:=	rubik
NAME_BONUS	:=	rubik_bonus
TESTS		:=	test_moves
# ^ Sprint 0 (05-roadmap-mandatory.md) is the only module that exists yet:
#   t_cube + the 18 move tables + apply_move(). test_moves.c is that sprint's
#   test harness (4x any move = identity, (R U R' U')x6 = identity, scramble
#   then its exact inverse = identity). Add one TESTS entry + one *_SRCS/*_OBJS
#   pair below per module as Sprint 1+ lands — same pattern, not a rewrite.

# ==========================
# Compiler detection
# ==========================
# `cc` is not guaranteed to exist. It is a traditional alias, and a minimal
# Debian/Ubuntu VM - a 42 image, a container, a fresh cloud box - can ship
# gcc or clang without ever creating it.
#
# When it is missing, the compile rule below dies with a bare
#
#     make: *** [Makefile:NN: obj/src/cube/cubie.o] Error 127
#
# 127 is the shell's code for "command not found", but make reports the
# TARGET rather than the missing command, so the message reads as though the
# object file were the problem. It is not: the compiler is.
#
# So find one, and if there is genuinely none, say something actionable
# instead of failing halfway through a build. A command-line override always
# wins over everything here:  make CC=clang
CC			:=	$(firstword $(foreach c,cc gcc clang,\
					$(if $(shell command -v $(c) 2>/dev/null),$(c))))
ifeq ($(strip $(CC)),)
$(error No C compiler found - looked for cc, gcc and clang on your PATH. \
Install one with `sudo apt install build-essential` (Debian/Ubuntu) or \
`xcode-select --install` (macOS), or point this at yours: make CC=/path/to/gcc)
endif

CFLAGS		:=	-Wall -Wextra -Werror -MMD -MP
CPPFLAGS	:=	-Iinclude
LDLIBS		:=
# No -lm here: the mandatory build is permutation/index arithmetic (moves,
# coordinates, IDA*, pruning tables) — plain integers throughout, nothing
# calls into libm. If that stops being true, add -lm here, not in LDLIBS_BONUS.

SRC_DIR		:=	src
TEST_DIR	:=	tests
OBJ_DIR		:=	obj
RENDER_DIR	:=	$(SRC_DIR)/render

# ==========================
# Source discovery
# ==========================
# Mandatory sources explicitly exclude src/render/ — this is the actual
# enforcement of "zero graphics code linked into the mandatory binary"
# (04-architecture.md), not just a naming convention. $(NAME) is built from
# $(OBJS) alone; render objects never enter that list.
SRCS		:=	$(shell find $(SRC_DIR) -name '*.c' -not -path '$(RENDER_DIR)/*')
OBJS		:=	$(SRCS:%.c=$(OBJ_DIR)/%.o)

RENDER_SRCS	:=	$(shell find $(RENDER_DIR) -name '*.c' 2>/dev/null)
RENDER_OBJS	:=	$(RENDER_SRCS:%.c=$(OBJ_DIR)/%.o)

# Each test links against the module(s) it exercises, never against main.c.
MV_SRCS		:=	$(TEST_DIR)/test_moves.c $(SRC_DIR)/cube/cubie.c $(SRC_DIR)/cube/moves.c
MV_OBJS		:=	$(MV_SRCS:%.c=$(OBJ_DIR)/%.o)

ALL_OBJS	:=	$(sort $(OBJS) $(RENDER_OBJS) $(MV_OBJS))

# Progress-bar denominator: `make bonus` also compiles src/render/, `make`
# alone never does — count accordingly so the bar actually reaches 100%
# either way instead of stalling or overshooting.
ifneq ($(filter bonus,$(MAKECMDGOALS)),)
TOTAL		:=	$(words $(SRCS) $(RENDER_SRCS))
else
TOTAL		:=	$(words $(SRCS))
endif
COMPILED	:=	0
MAKEFLAGS	+=	--no-print-directory

# Make's default goal is the FIRST rule in the file, not whichever target is
# named "all" — and $(RAYLIB_LIB)'s rule appears earlier in this file than
# the "all:" target does. Without this, bare `make` would silently try to
# build raylib instead of the mandatory binary. Pin it explicitly.
.DEFAULT_GOAL := all

# ==========================
# raylib (bonus only — vendored, never installed system-wide)
# ==========================
# Build story from 03-graphics.md: clone as a submodule, build the static
# lib locally with PLATFORM_DESKTOP, link it directly. No admin/sudo access
# needed on school machines, and nothing here touches the mandatory build.
RAYLIB_DIR		:=	lib/raylib
RAYLIB_SRC_DIR	:=	$(RAYLIB_DIR)/src
RAYLIB_LIB		:=	$(RAYLIB_SRC_DIR)/libraylib.a

UNAME_S		:=	$(shell uname -s)
ifeq ($(UNAME_S),Darwin)
LDLIBS_BONUS	:=	-L$(RAYLIB_SRC_DIR) -lraylib \
					-framework OpenGL -framework Cocoa \
					-framework IOKit -framework CoreVideo -framework CoreAudio
else
LDLIBS_BONUS	:=	-L$(RAYLIB_SRC_DIR) -lraylib -lGL -lm -lpthread -ldl -lrt -lX11
endif

$(RAYLIB_LIB):
	@if [ ! -f $(RAYLIB_DIR)/CMakeLists.txt ] && [ ! -f $(RAYLIB_DIR)/src/Makefile ]; then \
		printf "$(YELLOW)  raylib submodule not initialised — fetching it...$(RESET)\n"; \
		git submodule update --init --recursive; \
	fi
	@printf "$(CYAN)$(BOLD)\n  Building raylib (PLATFORM_DESKTOP, first time only)...$(RESET)\n\n"
	@$(MAKE) -C $(RAYLIB_SRC_DIR) PLATFORM=PLATFORM_DESKTOP

# render/ objects additionally need raylib's headers. Pattern-specific
# variable — only applies to objects built from under src/render/, leaves
# every other compile rule untouched.
$(OBJ_DIR)/$(RENDER_DIR)/%.o: CPPFLAGS += -I$(RAYLIB_SRC_DIR)

# ==========================
# Colours
# ==========================
GREEN		= \033[0;32m
BLUE		= \033[0;34m
CYAN		= \033[0;36m
YELLOW		= \033[0;33m
RED			= \033[0;31m
MAGENTA		= \033[0;35m
BOLD		= \033[1m
RESET		= \033[0m

# ==========================
# Targets
# ==========================
all: $(NAME)

$(NAME): $(OBJS)
	@$(CC) $(CFLAGS) $(OBJS) $(LDLIBS) -o $(NAME)
	@$(MAKE) banner
	@printf "$(GREEN)$(BOLD) [rubik compiled successfully — mandatory, no graphics code linked]$(RESET)\n\n"

bonus: $(RAYLIB_LIB) $(NAME_BONUS)

$(NAME_BONUS): $(OBJS) $(RENDER_OBJS)
	@$(CC) $(CFLAGS) $(OBJS) $(RENDER_OBJS) $(LDLIBS_BONUS) -o $(NAME_BONUS)
	@$(MAKE) banner
	@printf "$(GREEN)$(BOLD) [rubik_bonus compiled successfully — 3D renderer linked]$(RESET)\n\n"

# Compile with progress bar. The mkdir handles the mirrored obj/ subtree,
# so a new src/<module>/ directory needs no rule of its own.
$(OBJ_DIR)/%.o: %.c
	@mkdir -p $(dir $@)
	@$(eval COMPILED := $(shell echo $$(($(COMPILED) + 1))))
	@$(eval PCT := $(shell echo $$(($(COMPILED) * 100 / $(TOTAL)))))
	@$(eval FILLED := $(shell if [ $(COMPILED) -ge $(TOTAL) ]; then echo 20; else echo $$(($(COMPILED) * 20 / $(TOTAL))); fi))
	@$(eval EMPTY := $(shell echo $$(( 20 - $(FILLED)))))
	@printf "\r$(CYAN)  Compiling $(BOLD)%-30s$(RESET)$(CYAN) [" "$(notdir $<)"
	@i=0; while [ $$i -lt $(FILLED) ]; do printf "$(GREEN)█$(RESET)"; i=$$((i+1)); done
	@i=0; while [ $$i -lt $(EMPTY) ]; do printf "░"; i=$$((i+1)); done
	@printf "$(CYAN)] $(BOLD)%3d%%$(RESET)" $(PCT)
	@$(CC) $(CFLAGS) $(CPPFLAGS) -c $< -o $@

# ==========================
# Unit tests (C)
# ==========================
test: $(TESTS)
	@printf "$(YELLOW)$(BOLD)\n  Running unit tests...$(RESET)\n\n"
	@for t in $(TESTS); do printf "$(CYAN)  == %s ==$(RESET)\n" "$$t"; ./$$t || exit 1; done
	@printf "$(GREEN)$(BOLD)\n  [All tests passed]$(RESET)\n\n"

test_moves: $(MV_OBJS)
	@$(CC) $(CFLAGS) $(MV_OBJS) $(LDLIBS) -o $@

# ==========================
# Valgrind
# ==========================
# No sudo, no raw sockets — the entire point of the anti-cheat rule
# (01-requirements.md) is that solve() only ever sees a t_cube, so this
# needs nothing more than a scramble string on argv. Pass one through ARGS,
# e.g. `make valgrind ARGS="R U R' U'"` — defaults to a fixed scramble so
# `make valgrind` alone works from a clean checkout.
ARGS		?=	R2 U F' L2 D B R U2 L' F2
valgrind: $(NAME)
	@printf "$(YELLOW)$(BOLD)\n  Running valgrind on $(NAME)...$(RESET)\n\n"
	valgrind \
		--leak-check=full \
		--show-leak-kinds=all \
		--track-origins=yes \
		--error-exitcode=1 \
		./$(NAME) $(ARGS)

# ==========================
# Debug build
# ==========================
debug: CFLAGS += -g3 -fsanitize=address
debug: re
	@printf "$(RED)$(BOLD)  [Debug build with ASan ready]$(RESET)\n\n"

# ==========================
# Run
# ==========================
run: $(NAME)
	@printf "$(GREEN)$(BOLD)  Starting rubik...$(RESET)\n\n"
	./$(NAME) $(ARGS)

# ==========================
# Check style / warnings
# ==========================
check: CFLAGS += -Wpedantic -Wshadow -Wconversion
check: re
	@printf "$(MAGENTA)$(BOLD)  [Strict warning check complete]$(RESET)\n\n"

# ==========================
# Toolchain report
# ==========================
# Run this first on any machine that will not build. It answers the questions
# a bare "Error 127" cannot, and — specific to this project — checks the two
# raylib build dependencies 03-graphics.md says to verify BEFORE committing
# to the raylib bonus path: X11 and Mesa/OpenGL development headers.
env:
	@printf "$(CYAN)$(BOLD)\n  Toolchain$(RESET)\n\n"
	@printf "    %-14s %s\n" "compiler"  "$(CC)"
	@printf "    %-14s %s\n" "path"      "$$(command -v $(CC) 2>/dev/null || echo '(not found)')"
	@printf "    %-14s %s\n" "version"   "$$($(CC) --version 2>/dev/null | head -1 || echo '(unknown)')"
	@printf "    %-14s %s\n" "make"      "$$($(MAKE) --version 2>/dev/null | head -1)"
	@printf "    %-14s %s\n" "uname"     "$$(uname -srm)"
	@printf "$(CYAN)$(BOLD)\n  Optional tools$(RESET)\n\n"
	@for t in valgrind git clear; do \
		p=$$(command -v $$t 2>/dev/null); \
		if [ -n "$$p" ]; then \
			printf "    $(GREEN)✓$(RESET) %-12s %s\n" "$$t" "$$p"; \
		else \
			printf "    $(YELLOW)-$(RESET) %-12s (not installed)\n" "$$t"; \
		fi; \
	done
	@printf "$(CYAN)$(BOLD)\n  raylib bonus build dependencies (see docs/en/03-graphics.md)$(RESET)\n\n"
	@if [ -f /usr/include/X11/Xlib.h ] || [ -f /opt/X11/include/X11/Xlib.h ]; then \
		printf "    $(GREEN)✓$(RESET) %-12s found\n" "X11 headers"; \
	else \
		printf "    $(YELLOW)-$(RESET) %-12s not found — install libx11-dev / xorg-dev (Linux); Xcode tools usually cover macOS\n" "X11 headers"; \
	fi
	@if [ -f /usr/include/GL/gl.h ] || [ -d /System/Library/Frameworks/OpenGL.framework ]; then \
		printf "    $(GREEN)✓$(RESET) %-12s found\n" "GL headers"; \
	else \
		printf "    $(YELLOW)-$(RESET) %-12s not found — install libgl1-mesa-dev (Linux)\n" "GL headers"; \
	fi
	@if [ -f $(RAYLIB_SRC_DIR)/raylib.h ]; then \
		printf "    $(GREEN)✓$(RESET) %-12s submodule present\n" "raylib"; \
	else \
		printf "    $(YELLOW)-$(RESET) %-12s not yet fetched — run: git submodule update --init --recursive\n" "raylib"; \
	fi
	@printf "$(CYAN)$(BOLD)\n  Sources found$(RESET)\n\n"
	@printf "    %-14s %s\n" "mandatory (.c)" "$(words $(SRCS))"
	@printf "    %-14s %s\n" "render (.c)"    "$(words $(RENDER_SRCS))"
	@if [ "$(words $(SRCS))" -eq 0 ]; then \
		printf "    $(RED)no sources found - are you running make from the repo root?$(RESET)\n"; \
	fi
	@printf "\n"

# ==========================
# Count lines of code
# ==========================
cloc:
	@printf "$(CYAN)$(BOLD)\n  Lines of code:$(RESET)\n\n"
	@find $(SRC_DIR) include $(TEST_DIR) -name "*.c" -o -name "*.h" | \
		xargs wc -l | sort -rn | head -30
	@printf "\n"

# ==========================
# Show the source tree
# ==========================
list:
	@printf "$(CYAN)$(BOLD)\n  Mandatory sources ($(words $(SRCS)) files):$(RESET)\n"
	@for f in $(SRCS); do printf "    $(GREEN)→$(RESET) $$f\n"; done
	@printf "$(CYAN)$(BOLD)\n  Render sources — bonus only ($(words $(RENDER_SRCS)) files):$(RESET)\n"
	@for f in $(RENDER_SRCS); do printf "    $(MAGENTA)→$(RESET) $$f\n"; done
	@printf "$(CYAN)$(BOLD)\n  Headers:$(RESET)\n"
	@for f in include/*.h; do printf "    $(BLUE)→$(RESET) $$f\n"; done
	@printf "$(CYAN)$(BOLD)\n  Tests:$(RESET)\n"
	@for f in $(TEST_DIR)/*.c; do printf "    $(YELLOW)→$(RESET) $$f\n"; done
	@printf "\n"

# ==========================
# Push to GitHub
# ==========================
# Usage: make push MSG="commit message"
MSG			?=	update: $(shell date "+%Y-%m-%d %H:%M")
BRANCH		:=	$(shell git rev-parse --abbrev-ref HEAD 2>/dev/null)

push:
	@printf "$(YELLOW)$(BOLD)\n  Pushing to GitHub ($(BRANCH))...$(RESET)\n\n"
	@git add -A
	@if git diff --cached --quiet; then \
		printf "$(CYAN)  Nothing to commit, pushing anyway...$(RESET)\n"; \
	else \
		git commit -m "$(MSG)"; \
	fi
	@git push origin $(BRANCH)
	@printf "$(GREEN)$(BOLD)\n  [Pushed to $(BRANCH)]$(RESET)\n\n"

# ==========================
# Banner
# ==========================
banner:
	@if [ -t 1 ] && [ -n "$$TERM" ] && command -v clear >/dev/null 2>&1; then clear; fi
	@printf "\n$(CYAN)"
	@sleep 0.05
	@printf "%s\n" " ____  _   _ ____ ___ _  __"
	@sleep 0.15
	@printf "%s\n" "|  _ \\| | | | __ )_ _| |/ /"
	@sleep 0.15
	@printf "%s\n" "| |_) | | | |  _ \\| || ' / "
	@sleep 0.15
	@printf "%s\n" "|  _ <| |_| | |_) | || . \\ "
	@sleep 0.15
	@printf "%s\n" "|_| \\_\\\\___/|____/___|_|\\_\\"
	@printf "$(RESET)\n"

# ==========================
# Clean
# ==========================
# fclean deliberately does NOT touch lib/raylib's own build output — that's
# a vendored submodule, not project output, and rebuilding it from scratch
# costs real minutes you don't want to pay on every `make re`. Use
# `make fclean-raylib` on the rare occasion you actually need that.
clean:
	@rm -rf $(OBJ_DIR)
	@printf "$(BLUE)\n  Cleansed$(RESET)\n"

fclean: clean
	@rm -f $(NAME) $(NAME_BONUS) $(TESTS)
	@printf "$(BLUE)  More Cleansed$(RESET)\n\n"

fclean-raylib:
	@$(MAKE) -C $(RAYLIB_SRC_DIR) clean 2>/dev/null || true
	@printf "$(BLUE)  raylib build artefacts removed (submodule itself untouched)$(RESET)\n\n"

re: fclean all

-include $(ALL_OBJS:.o=.d)

# ==========================
# Help
# ==========================
help:
	@printf "\n$(CYAN)$(BOLD)  rubik — available commands$(RESET)\n\n"
	@printf "  $(GREEN)make$(RESET)                  — build the mandatory binary ($(NAME))\n"
	@printf "  $(GREEN)make bonus$(RESET)            — fetch/build raylib if needed, build $(NAME_BONUS)\n"
	@printf "  $(GREEN)make re$(RESET)               — clean and rebuild (mandatory)\n"
	@printf "  $(GREEN)make clean$(RESET)            — remove object files\n"
	@printf "  $(GREEN)make fclean$(RESET)           — remove objects and both binaries\n"
	@printf "  $(GREEN)make fclean-raylib$(RESET)    — also clean raylib's own build output\n"
	@printf "  $(GREEN)make test$(RESET)             — build and run the C unit tests\n"
	@printf "  $(GREEN)make run$(RESET)              — build and run rubik (pass ARGS=\"R U R'\")\n"
	@printf "  $(GREEN)make debug$(RESET)            — build with AddressSanitizer and debug symbols\n"
	@printf "  $(GREEN)make valgrind$(RESET)         — run rubik under valgrind (pass ARGS=\"...\")\n"
	@printf "  $(GREEN)make check$(RESET)            — rebuild with extra pedantic warnings\n"
	@printf "  $(GREEN)make env$(RESET)              — report the toolchain + raylib build deps\n"
	@printf "  $(GREEN)make cloc$(RESET)             — count lines of code per file\n"
	@printf "  $(GREEN)make list$(RESET)             — list the source tree (mandatory + render)\n"
	@printf "  $(GREEN)make push$(RESET)             — commit and push to GitHub (pass MSG=\"...\")\n"
	@printf "  $(GREEN)make help$(RESET)             — show this message\n"
	@printf "\n$(CYAN)  Usage:$(RESET)\n"
	@printf "  $(YELLOW)./rubik \"R2 U F' L2 D B R U2 L' F2\"$(RESET)   (mandatory)\n"
	@printf "  $(YELLOW)./rubik_bonus \"...\"$(RESET)                    (3D bonus, same scramble syntax)\n\n"

.PHONY: all bonus test clean fclean fclean-raylib re valgrind debug run check env cloc list banner push help
