# =============================================================================
#  rubik
#
#  Source layout (see docs/en/04-architecture.md for the full rationale):
#    include/        one header per module, plus the rubik.h umbrella
#    src/parse/      argv/notation -> t_cube, and legality validation
#    src/cube/       cubie + facelet models, the 18 move tables
#    src/coord/      coordinate encoders, move-table & pruning-table builders
#    src/solve/      IDA*, Kociemba two-phase, Thistlethwaite (bonus only —
#                     that one .c compiles only under `make bonus`, see below)
#    src/render/     3D bonus ONLY — raylib. Never linked into the mandatory
#                     binary; this is structural, not a promise (see below).
#    tests/          standalone unit tests, one binary each, module-scoped
#    lib/raylib/     vendored as a git submodule, built locally, never installed
#
#  Sources are DISCOVERED, not listed: adding a .c anywhere under src/ picks
#  it up with no edit here — src/solve/thistlethwaite.c is the one deliberate
#  exception (THISTLE_SRC below). Object files mirror the source tree under
#  obj/. src/main.c is also special: it is compiled TWICE, once plain for
#  $(NAME) and once with -DBONUS_ALGO for $(NAME_BONUS) (MAIN_BONUS_OBJ
#  below), so the "-a kociemba|thistlethwaite" flag exists only in the bonus
#  binary without a second near-duplicate entry-point file.
#
#  Two binaries, on purpose (R11: bonus only counts if mandatory is perfect):
#    make        ->  $(NAME)        mandatory only: src/render/ and
#                                    Thistlethwaite excluded, no -a flag
#    make bonus  ->  $(NAME_BONUS)  mandatory + src/render/ (raylib) +
#                                    Thistlethwaite, choose it with -a
#  `make` never touches raylib and never fails because of it.
# =============================================================================

NAME		:=	rubik
NAME_BONUS	:=	rubik_bonus
TESTS		:=	test_moves test_cubie test_parse test_coord test_movetable test_prune test_ida test_solve test_thistlethwaite
# ^ One binary per module, all built from tests/. test_moves / test_cubie /
#   test_parse / test_coord / test_movetable / test_prune / test_ida / test_solve / test_thistlethwaite are C unit tests; tests/test_cli.sh is the end-to-end check on
#   ./rubik itself (exit codes, messages, stdout). `make test` runs all of
#   them. Add one TESTS entry + one *_SRCS/*_OBJS pair below per new module
#   (coord, solve, ...) — same pattern, not a rewrite.

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

CFLAGS		:=	-Wall -Wextra -Werror -MMD -MP -O2
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
# Thistlethwaite is bonus-only (R11 + the subject's multi-algorithm-selection
# bonus item): excluded from the mandatory SRCS the same way src/render/ is,
# and linked into $(NAME_BONUS) explicitly via THISTLE_OBJ below.
THISTLE_SRC	:=	$(SRC_DIR)/solve/thistlethwaite.c
THISTLE_OBJ	:=	$(THISTLE_SRC:%.c=$(OBJ_DIR)/%.o)

SRCS		:=	$(shell find $(SRC_DIR) -name '*.c' -not -path '$(RENDER_DIR)/*' \
					-not -path '$(THISTLE_SRC)')
OBJS		:=	$(SRCS:%.c=$(OBJ_DIR)/%.o)

# main.c is compiled twice — see the header comment above. MAIN_OBJ is the
# plain one, already inside OBJS/$(NAME); CORE_OBJS is everything else
# mandatory (library code, no entry point), reused by $(NAME_BONUS) instead
# of MAIN_OBJ so the bonus binary links MAIN_BONUS_OBJ in its place.
MAIN_SRC		:=	$(SRC_DIR)/main.c
MAIN_OBJ		:=	$(MAIN_SRC:%.c=$(OBJ_DIR)/%.o)
MAIN_BONUS_OBJ	:=	$(OBJ_DIR)/$(SRC_DIR)/main_bonus.o
CORE_OBJS		:=	$(filter-out $(MAIN_OBJ),$(OBJS))

RENDER_SRCS	:=	$(shell find $(RENDER_DIR) -name '*.c' 2>/dev/null)
RENDER_OBJS	:=	$(RENDER_SRCS:%.c=$(OBJ_DIR)/%.o)

# Each test links against the module(s) it exercises, never against main.c.
MV_SRCS		:=	$(TEST_DIR)/test_moves.c $(SRC_DIR)/cube/cubie.c $(SRC_DIR)/cube/moves.c
MV_OBJS		:=	$(MV_SRCS:%.c=$(OBJ_DIR)/%.o)

CB_SRCS		:=	$(TEST_DIR)/test_cubie.c $(SRC_DIR)/cube/cubie.c $(SRC_DIR)/cube/moves.c
CB_OBJS		:=	$(CB_SRCS:%.c=$(OBJ_DIR)/%.o)

PR_SRCS		:=	$(TEST_DIR)/test_parse.c $(SRC_DIR)/parse/notation.c \
				$(SRC_DIR)/parse/validate.c $(SRC_DIR)/cube/cubie.c \
				$(SRC_DIR)/cube/moves.c
PR_OBJS		:=	$(PR_SRCS:%.c=$(OBJ_DIR)/%.o)

CO_SRCS		:=	$(TEST_DIR)/test_coord.c $(SRC_DIR)/coord/encode.c \
				$(SRC_DIR)/coord/decode.c $(SRC_DIR)/coord/tables.c \
				$(SRC_DIR)/cube/cubie.c $(SRC_DIR)/cube/moves.c
CO_OBJS		:=	$(CO_SRCS:%.c=$(OBJ_DIR)/%.o)

MT_SRCS		:=	$(TEST_DIR)/test_movetable.c $(SRC_DIR)/coord/movetable.c \
				$(SRC_DIR)/coord/encode.c $(SRC_DIR)/coord/decode.c \
				$(SRC_DIR)/coord/tables.c $(SRC_DIR)/cube/cubie.c \
				$(SRC_DIR)/cube/moves.c
MT_OBJS		:=	$(MT_SRCS:%.c=$(OBJ_DIR)/%.o)

PU_SRCS		:=	$(TEST_DIR)/test_prune.c $(SRC_DIR)/coord/prune.c \
				$(SRC_DIR)/coord/movetable.c $(SRC_DIR)/coord/encode.c \
				$(SRC_DIR)/coord/decode.c $(SRC_DIR)/coord/tables.c \
				$(SRC_DIR)/cube/cubie.c $(SRC_DIR)/cube/moves.c
PU_OBJS		:=	$(PU_SRCS:%.c=$(OBJ_DIR)/%.o)

ID_SRCS		:=	$(TEST_DIR)/test_ida.c $(SRC_DIR)/solve/ida.c \
				$(SRC_DIR)/coord/prune.c $(SRC_DIR)/coord/movetable.c \
				$(SRC_DIR)/coord/encode.c $(SRC_DIR)/coord/decode.c \
				$(SRC_DIR)/coord/tables.c $(SRC_DIR)/cube/cubie.c \
				$(SRC_DIR)/cube/moves.c
ID_OBJS		:=	$(ID_SRCS:%.c=$(OBJ_DIR)/%.o)

SO_SRCS		:=	$(TEST_DIR)/test_solve.c $(SRC_DIR)/solve/solve.c \
				$(SRC_DIR)/solve/ida.c $(SRC_DIR)/coord/prune.c \
				$(SRC_DIR)/coord/movetable.c $(SRC_DIR)/coord/encode.c \
				$(SRC_DIR)/coord/decode.c $(SRC_DIR)/coord/tables.c \
				$(SRC_DIR)/parse/notation.c $(SRC_DIR)/parse/validate.c \
				$(SRC_DIR)/cube/cubie.c $(SRC_DIR)/cube/moves.c
SO_OBJS		:=	$(SO_SRCS:%.c=$(OBJ_DIR)/%.o)

TH_SRCS		:=	$(TEST_DIR)/test_thistlethwaite.c $(SRC_DIR)/solve/thistlethwaite.c \
				$(SRC_DIR)/solve/solve.c $(SRC_DIR)/solve/ida.c \
				$(SRC_DIR)/coord/prune.c $(SRC_DIR)/coord/movetable.c \
				$(SRC_DIR)/coord/encode.c $(SRC_DIR)/coord/decode.c \
				$(SRC_DIR)/coord/tables.c $(SRC_DIR)/parse/notation.c \
				$(SRC_DIR)/parse/validate.c $(SRC_DIR)/cube/cubie.c \
				$(SRC_DIR)/cube/moves.c
TH_OBJS		:=	$(TH_SRCS:%.c=$(OBJ_DIR)/%.o)

# Bonus-only, opt-in test (never part of $(TESTS)/`make test`): the
# drag-turning math (docs/en/11-drag-review.md §6.1) lives in
# src/render/, so this pulls in real raylib the same way `make bonus`
# does, and the mandatory build/test invariant ("make test never
# touches raylib and never fails because of it") stays intact by
# keeping this out of TESTS entirely — see `test_bonus` below.
DR_SRCS		:=	$(TEST_DIR)/test_drag.c $(RENDER_DIR)/input.c \
				$(RENDER_DIR)/anim.c $(RENDER_DIR)/geometry.c \
				$(RENDER_DIR)/fx.c $(SRC_DIR)/cube/cubie.c $(SRC_DIR)/cube/moves.c
DR_OBJS		:=	$(DR_SRCS:%.c=$(OBJ_DIR)/%.o)

ALL_OBJS	:=	$(sort $(OBJS) $(MAIN_BONUS_OBJ) $(RENDER_OBJS) $(MV_OBJS) $(CB_OBJS) $(PR_OBJS) $(CO_OBJS) $(MT_OBJS) $(PU_OBJS) $(ID_OBJS) $(SO_OBJS) $(TH_OBJS) $(DR_OBJS))

# Progress-bar denominator: `make bonus` also compiles src/render/, `make`
# alone never does — count accordingly so the bar actually reaches 100%
# either way instead of stalling or overshooting.
ifneq ($(filter bonus,$(MAKECMDGOALS)),)
# CORE_OBJS (len(SRCS)-1, main.c swapped out) + MAIN_BONUS_OBJ (1, swapped
# in) + THISTLE_OBJ (1, extra) + RENDER_OBJS -> len(SRCS) + len(RENDER_SRCS)
# + 1; adding THISTLE_SRC to this word-count union is that "+1", same trick
# the rest of this file uses instead of raw arithmetic.
TOTAL		:=	$(words $(SRCS) $(RENDER_SRCS) $(THISTLE_SRC))
else ifneq ($(filter test,$(MAKECMDGOALS)),)
TOTAL		:=	$(words $(sort $(OBJS) $(MV_OBJS) $(CB_OBJS) $(PR_OBJS) $(CO_OBJS) $(MT_OBJS) $(PU_OBJS) $(ID_OBJS) $(SO_OBJS) $(TH_OBJS)))
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

# Two ways to get raylib, picked once at parse time:
#   1. pkg-config knows it (native macOS: `brew install raylib pkg-config`)
#      -> flags come from `pkg-config --cflags/--libs raylib`, the vendored
#         submodule is never built.
#   2. it does not (Docker/XQuartz, 42 Linux boxes: no raylib.pc anywhere —
#      the submodule's plain `make` never generates one) -> fall back to the
#      vendored static lib in lib/raylib, exactly as before.
# Only the cheap `--exists` probe runs on every make (stderr silenced, so a
# missing pkg-config stays invisible to the mandatory build). The actual
# --cflags/--libs calls use `=`, so they expand only inside bonus recipes.
RAYLIB_PC		:=	$(shell pkg-config --exists raylib 2>/dev/null && echo 1)

UNAME_S		:=	$(shell uname -s)
ifeq ($(RAYLIB_PC),1)
RAYLIB_CFLAGS	=	$(shell pkg-config --cflags raylib)
LDLIBS_BONUS	=	$(shell pkg-config --libs raylib)
RAYLIB_DEP		:=
else
RAYLIB_CFLAGS	:=	-I$(RAYLIB_SRC_DIR)
RAYLIB_DEP		:=	$(RAYLIB_LIB)
ifeq ($(UNAME_S),Darwin)
LDLIBS_BONUS	:=	-L$(RAYLIB_SRC_DIR) -lraylib \
					-framework OpenGL -framework Cocoa \
					-framework IOKit -framework CoreVideo -framework CoreAudio
else
LDLIBS_BONUS	:=	-L$(RAYLIB_SRC_DIR) -lraylib -lGL -lm -lpthread -ldl -lrt -lX11
endif
endif

# GRAPHICS=GRAPHICS_API_OPENGL_21, not raylib's own default (_33 / GL 3.3 core):
# XQuartz's GLX implementation cannot create a core-profile context at all —
# confirmed on a real run: X_GLXCreateNewContext comes back BadValue the
# instant GLFW asks for GL 3.3 core, with no env var on either side of the
# connection able to work around it (it is the X SERVER, i.e. XQuartz, that
# refuses the request — nothing Mesa/llvmpipe does in the container changes
# that). GL 2.1 (compatibility profile) is something XQuartz's GLX CAN hand
# out, and this project's renderer only ever calls rlgl's fixed-function-era
# API (DrawCube, DrawText, rlPushMatrix/rlRotatef, ...) — nothing here needs
# a core profile or a shader, so the older backend is a fully compatible
# swap, not a downgrade in what phases 0-3 (or 5's planned lighting, which
# does need a real shader) can do. If you ever run this on a real Linux
# desktop or a Wayland/XWayland setup with modern GLX instead of XQuartz,
# GRAPHICS_API_OPENGL_33 would work fine there too — this pin is specifically
# for the XQuartz bridge docker/README.md documents.
$(RAYLIB_LIB):
	@if [ ! -f $(RAYLIB_DIR)/CMakeLists.txt ] && [ ! -f $(RAYLIB_DIR)/src/Makefile ]; then \
		printf "$(YELLOW)  raylib submodule not initialised — fetching it...$(RESET)\n"; \
		git submodule update --init --recursive; \
	fi
	@printf "$(CYAN)$(BOLD)\n  Building raylib (PLATFORM_DESKTOP, GL 2.1 — see comment above, first time only)...$(RESET)\n\n"
	@$(MAKE) -C $(RAYLIB_SRC_DIR) PLATFORM=PLATFORM_DESKTOP GRAPHICS=GRAPHICS_API_OPENGL_21

# render/ objects additionally need raylib's headers (pkg-config's, or the
# vendored ones — see RAYLIB_CFLAGS above). Pattern-specific
# variable — only applies to objects built from under src/render/, leaves
# every other compile rule untouched.
$(OBJ_DIR)/$(RENDER_DIR)/%.o: CPPFLAGS += $(RAYLIB_CFLAGS)
$(OBJ_DIR)/$(TEST_DIR)/test_drag.o: CPPFLAGS += $(RAYLIB_CFLAGS)

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
BG_RED		= \033[41m
BG_GRN		= \033[42m
BG_YEL		= \033[43m
BG_BLU		= \033[44m
BG_WHT		= \033[47m
BG_ORG		= \033[48;5;208m
WHITE		= \033[1;37m
ORANGE		= \033[38;5;208m
BG_TOP		= \033[48;5;250m
BG_SIDE		= \033[48;5;238m

# ==========================
# Targets
# ==========================
all: $(NAME)

$(NAME): $(OBJS)
	@$(CC) $(CFLAGS) $(OBJS) $(LDLIBS) -o $(NAME)
	@$(MAKE) banner
	@printf "$(GREEN)$(BOLD) [rubik compiled successfully — mandatory, no graphics code linked]$(RESET)\n\n"

bonus: $(RAYLIB_DEP) $(NAME_BONUS)

$(NAME_BONUS): $(CORE_OBJS) $(MAIN_BONUS_OBJ) $(THISTLE_OBJ) $(RENDER_OBJS)
	@$(CC) $(CFLAGS) $(CORE_OBJS) $(MAIN_BONUS_OBJ) $(THISTLE_OBJ) $(RENDER_OBJS) $(LDLIBS_BONUS) -o $(NAME_BONUS)
	@$(MAKE) banner
	@printf "$(GREEN)$(BOLD) [rubik_bonus compiled successfully — 3D renderer + Thistlethwaite linked, -a to choose]$(RESET)\n\n"

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

# main.c compiled a second time, with -DBONUS_ALGO, so the -a flag only
# exists in $(NAME_BONUS); $(NAME) links plain $(MAIN_OBJ) instead and its
# argv handling never sees this macro. Same progress-bar bookkeeping as the
# generic rule above, since this one bypasses it (different stem: main vs
# main_bonus, not a pattern rule).
$(MAIN_BONUS_OBJ): $(MAIN_SRC)
	@mkdir -p $(dir $@)
	@$(eval COMPILED := $(shell echo $$(($(COMPILED) + 1))))
	@$(eval PCT := $(shell echo $$(($(COMPILED) * 100 / $(TOTAL)))))
	@$(eval FILLED := $(shell if [ $(COMPILED) -ge $(TOTAL) ]; then echo 20; else echo $$(($(COMPILED) * 20 / $(TOTAL))); fi))
	@$(eval EMPTY := $(shell echo $$(( 20 - $(FILLED)))))
	@printf "\r$(CYAN)  Compiling $(BOLD)%-30s$(RESET)$(CYAN) [" "$(notdir $@)"
	@i=0; while [ $$i -lt $(FILLED) ]; do printf "$(GREEN)█$(RESET)"; i=$$((i+1)); done
	@i=0; while [ $$i -lt $(EMPTY) ]; do printf "░"; i=$$((i+1)); done
	@printf "$(CYAN)] $(BOLD)%3d%%$(RESET)" $(PCT)
	@$(CC) $(CFLAGS) $(CPPFLAGS) -DBONUS_ALGO -c $< -o $@

# ==========================
# Unit tests (C)
# ==========================
test: $(NAME) $(TESTS)
	@printf "$(YELLOW)$(BOLD)\n\n  Running unit tests...$(RESET)\n\n"
	@for t in $(TESTS); do printf "$(CYAN)  == %s ==$(RESET)\n" "$$t"; ./$$t || exit 1; done
	@printf "$(CYAN)  == test_cli.sh ==$(RESET)\n"
	@bash $(TEST_DIR)/test_cli.sh ./$(NAME) || exit 1
	@printf "$(GREEN)$(BOLD)\n  [All tests passed]$(RESET)\n\n"

test_moves: $(MV_OBJS)
	@$(CC) $(CFLAGS) $(MV_OBJS) $(LDLIBS) -o $@

test_cubie: $(CB_OBJS)
	@$(CC) $(CFLAGS) $(CB_OBJS) $(LDLIBS) -o $@

test_parse: $(PR_OBJS)
	@$(CC) $(CFLAGS) $(PR_OBJS) $(LDLIBS) -o $@

test_coord: $(CO_OBJS)
	@$(CC) $(CFLAGS) $(CO_OBJS) $(LDLIBS) -o $@

test_movetable: $(MT_OBJS)
	@$(CC) $(CFLAGS) $(MT_OBJS) $(LDLIBS) -o $@

test_prune: $(PU_OBJS)
	@$(CC) $(CFLAGS) $(PU_OBJS) $(LDLIBS) -o $@

test_ida: $(ID_OBJS)
	@$(CC) $(CFLAGS) $(ID_OBJS) $(LDLIBS) -o $@

test_solve: $(SO_OBJS)
	@$(CC) $(CFLAGS) $(SO_OBJS) $(LDLIBS) -o $@

test_thistlethwaite: $(TH_OBJS)
	@$(CC) $(CFLAGS) $(TH_OBJS) $(LDLIBS) -o $@

# Opt-in only (docs/en/11-drag-review.md §6.1) — NOT part of `test`/
# $(TESTS): `make test_bonus` builds raylib if needed, then this one
# binary. Kept separate so a raylib-less environment (a 42 eval box
# grading the mandatory part only) never has its plain `make test` fail
# over a bonus-only test.
test_drag: $(RAYLIB_DEP) $(DR_OBJS)
	@$(CC) $(CFLAGS) $(DR_OBJS) $(LDLIBS_BONUS) -o $@

test_bonus: test_drag
	@printf "$(YELLOW)$(BOLD)\n  Running bonus-only unit tests...$(RESET)\n\n"
	@printf "$(CYAN)  == test_drag ==$(RESET)\n"
	@./test_drag || exit 1
	@printf "$(GREEN)$(BOLD)\n  [All bonus tests passed]$(RESET)\n\n"

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
debug: CFLAGS += -g3 -fsanitize=address -00
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
	@printf "    %-14s %s\n" "bonus algo (.c)" "$(words $(THISTLE_SRC))"
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
	@printf "$(CYAN)$(BOLD)\n  Bonus-only sources — linked into $(NAME_BONUS) only:$(RESET)\n"
	@printf "    $(MAGENTA)→$(RESET) $(THISTLE_SRC)\n"
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
	@printf "\n"
	@sleep 0.03
	@printf "$(RED)████ $(RESET)  $(WHITE)█   █$(RESET)  $(BLUE)████ $(RESET)  $(ORANGE)█████$(RESET)  $(GREEN)█   █$(RESET)\n"
	@sleep 0.03
	@printf "$(RED)█   █$(RESET)  $(WHITE)█   █$(RESET)  $(BLUE)█   █$(RESET)  $(ORANGE)  █  $(RESET)  $(GREEN)█  █ $(RESET)\n"
	@sleep 0.03
	@printf "$(RED)████ $(RESET)  $(WHITE)█   █$(RESET)  $(BLUE)████ $(RESET)  $(ORANGE)  █  $(RESET)  $(GREEN)███  $(RESET)\n"
	@sleep 0.03
	@printf "$(RED)█  █ $(RESET)  $(WHITE)█   █$(RESET)  $(BLUE)█   █$(RESET)  $(ORANGE)  █  $(RESET)  $(GREEN)█  █ $(RESET)\n"
	@sleep 0.03
	@printf "$(RED)█   █$(RESET)  $(WHITE) ███ $(RESET)  $(BLUE)████ $(RESET)  $(ORANGE)█████$(RESET)  $(GREEN)█   █$(RESET)\n\n"
	@bash -c '\
		COLORS=("$(BG_RED)" "$(BG_GRN)" "$(BG_YEL)" "$(BG_BLU)" "$(BG_WHT)" "$(BG_ORG)"); \
		pick() { echo "$${COLORS[$$RANDOM % 6]}"; }; \
		TOP_COLOR="$$(pick)"; \
		SIDE_COLOR="$$(pick)"; \
		while [ "$$SIDE_COLOR" = "$$TOP_COLOR" ]; do SIDE_COLOR="$$(pick)"; done; \
		sleep 0.02; printf "      $(CYAN)____________|$(RESET)\n"; \
		sleep 0.02; printf "    $(CYAN)/$(RESET)%b           $(RESET)$(CYAN)/$(RESET)%b $(RESET)$(CYAN)|$(RESET)\n" "$$TOP_COLOR" "$$SIDE_COLOR"; \
		sleep 0.02; printf "  $(CYAN)/$(RESET)%b           $(RESET)$(CYAN)/$(RESET)%b   $(RESET)$(CYAN)|$(RESET)\n" "$$TOP_COLOR" "$$SIDE_COLOR"; \
		sleep 0.03; printf "$(CYAN)+---+---+---+$(RESET)%b     $(RESET)$(CYAN)|$(RESET)\n" "$$SIDE_COLOR"; \
		printf "\033[s"; \
		sleep 0.06; printf "$(CYAN)|$(RESET)%b   $(RESET)$(CYAN)|$(RESET)%b   $(RESET)$(CYAN)|$(RESET)%b   $(RESET)$(CYAN)|$(RESET)%b     $(RESET)$(CYAN)|$(RESET)\n" "$$(pick)" "$$(pick)" "$$(pick)" "$$SIDE_COLOR"; \
		sleep 0.03; printf "$(CYAN)+---+---+---+$(RESET)%b     $(RESET)$(CYAN)|$(RESET)\n" "$$SIDE_COLOR"; \
		sleep 0.06; printf "$(CYAN)|$(RESET)%b   $(RESET)$(CYAN)|$(RESET)%b   $(RESET)$(CYAN)|$(RESET)%b   $(RESET)$(CYAN)|$(RESET)%b     $(RESET)$(CYAN)|$(RESET)\n" "$$(pick)" "$$(pick)" "$$(pick)" "$$SIDE_COLOR"; \
		sleep 0.03; printf "$(CYAN)+---+---+---+$(RESET)%b   $(RESET)$(CYAN)/$(RESET)\n" "$$SIDE_COLOR"; \
		sleep 0.06; printf "$(CYAN)|$(RESET)%b   $(RESET)$(CYAN)|$(RESET)%b   $(RESET)$(CYAN)|$(RESET)%b   $(RESET)$(CYAN)|$(RESET)%b $(RESET)$(CYAN)/$(RESET)\n" "$$(pick)" "$$(pick)" "$$(pick)" "$$SIDE_COLOR"; \
		sleep 0.03; printf "$(CYAN)+---+---+---+$(RESET)\n"; \
		for _f in 1 2 3 4; do \
			printf "\033[u"; \
			printf "$(CYAN)|$(RESET)%b   $(RESET)$(CYAN)|$(RESET)%b   $(RESET)$(CYAN)|$(RESET)%b   $(RESET)$(CYAN)|$(RESET)%b     $(RESET)$(CYAN)|$(RESET)\n" "$$(pick)" "$$(pick)" "$$(pick)" "$$SIDE_COLOR"; \
			printf "$(CYAN)+---+---+---+$(RESET)%b     $(RESET)$(CYAN)|$(RESET)\n" "$$SIDE_COLOR"; \
			printf "$(CYAN)|$(RESET)%b   $(RESET)$(CYAN)|$(RESET)%b   $(RESET)$(CYAN)|$(RESET)%b   $(RESET)$(CYAN)|$(RESET)%b     $(RESET)$(CYAN)|$(RESET)\n" "$$(pick)" "$$(pick)" "$$(pick)" "$$SIDE_COLOR"; \
			printf "$(CYAN)+---+---+---+$(RESET)%b   $(RESET)$(CYAN)/$(RESET)\n" "$$SIDE_COLOR"; \
			printf "$(CYAN)|$(RESET)%b   $(RESET)$(CYAN)|$(RESET)%b   $(RESET)$(CYAN)|$(RESET)%b   $(RESET)$(CYAN)|$(RESET)%b $(RESET)$(CYAN)/$(RESET)\n" "$$(pick)" "$$(pick)" "$$(pick)" "$$SIDE_COLOR"; \
			printf "$(CYAN)+---+---+---+$(RESET)\n"; \
			sleep 0.1; \
		done; \
	'

flash:
	@if [ -t 1 ] && [ -n "$$TERM" ] && command -v clear >/dev/null 2>&1; then clear; fi
	@printf "\n"
	@printf "$(RED)████ $(RESET)  $(WHITE)█   █$(RESET)  $(BLUE)████ $(RESET)  $(ORANGE)█████$(RESET)  $(GREEN)█   █$(RESET)\n"
	@printf "$(RED)█   █$(RESET)  $(WHITE)█   █$(RESET)  $(BLUE)█   █$(RESET)  $(ORANGE)  █  $(RESET)  $(GREEN)█  █ $(RESET)\n"
	@printf "$(RED)████ $(RESET)  $(WHITE)█   █$(RESET)  $(BLUE)████ $(RESET)  $(ORANGE)  █  $(RESET)  $(GREEN)███  $(RESET)\n"
	@printf "$(RED)█  █ $(RESET)  $(WHITE)█   █$(RESET)  $(BLUE)█   █$(RESET)  $(ORANGE)  █  $(RESET)  $(GREEN)█  █ $(RESET)\n"
	@printf "$(RED)█   █$(RESET)  $(WHITE) ███ $(RESET)  $(BLUE)████ $(RESET)  $(ORANGE)█████$(RESET)  $(GREEN)█   █$(RESET)\n\n"
	@bash -c '\
		COLORS=("$(BG_RED)" "$(BG_GRN)" "$(BG_YEL)" "$(BG_BLU)" "$(BG_WHT)" "$(BG_ORG)"); \
		pick() { echo "$${COLORS[$$RANDOM % 6]}"; }; \
		TOP_COLOR="$$(pick)"; \
		SIDE_COLOR="$$(pick)"; \
		while [ "$$SIDE_COLOR" = "$$TOP_COLOR" ]; do SIDE_COLOR="$$(pick)"; done; \
		printf "      $(CYAN)____________|$(RESET)\n"; \
		printf "    $(CYAN)/$(RESET)%b           $(RESET)$(CYAN)/$(RESET)%b $(RESET)$(CYAN)|$(RESET)\n" "$$TOP_COLOR" "$$SIDE_COLOR"; \
		printf "  $(CYAN)/$(RESET)%b           $(RESET)$(CYAN)/$(RESET)%b   $(RESET)$(CYAN)|$(RESET)\n" "$$TOP_COLOR" "$$SIDE_COLOR"; \
		printf "$(CYAN)+---+---+---+$(RESET)%b     $(RESET)$(CYAN)|$(RESET)\n" "$$SIDE_COLOR"; \
		printf "\033[s"; \
		printf "$(CYAN)|$(RESET)%b   $(RESET)$(CYAN)|$(RESET)%b   $(RESET)$(CYAN)|$(RESET)%b   $(RESET)$(CYAN)|$(RESET)%b     $(RESET)$(CYAN)|$(RESET)\n" "$$(pick)" "$$(pick)" "$$(pick)" "$$SIDE_COLOR"; \
		printf "$(CYAN)+---+---+---+$(RESET)%b     $(RESET)$(CYAN)|$(RESET)\n" "$$SIDE_COLOR"; \
		printf "$(CYAN)|$(RESET)%b   $(RESET)$(CYAN)|$(RESET)%b   $(RESET)$(CYAN)|$(RESET)%b   $(RESET)$(CYAN)|$(RESET)%b     $(RESET)$(CYAN)|$(RESET)\n" "$$(pick)" "$$(pick)" "$$(pick)" "$$SIDE_COLOR"; \
		printf "$(CYAN)+---+---+---+$(RESET)%b   $(RESET)$(CYAN)/$(RESET)\n" "$$SIDE_COLOR"; \
		printf "$(CYAN)|$(RESET)%b   $(RESET)$(CYAN)|$(RESET)%b   $(RESET)$(CYAN)|$(RESET)%b   $(RESET)$(CYAN)|$(RESET)%b $(RESET)$(CYAN)/$(RESET)\n" "$$(pick)" "$$(pick)" "$$(pick)" "$$SIDE_COLOR"; \
		printf "$(CYAN)+---+---+---+$(RESET)\n"; \
		printf "$(YELLOW)  [flashing for 30s — Ctrl+C to stop early]$(RESET)\n"; \
		SECONDS=0; \
		while [ $$SECONDS -lt 30 ]; do \
			printf "\033[u"; \
			printf "$(CYAN)|$(RESET)%b   $(RESET)$(CYAN)|$(RESET)%b   $(RESET)$(CYAN)|$(RESET)%b   $(RESET)$(CYAN)|$(RESET)%b     $(RESET)$(CYAN)|$(RESET)\n" "$$(pick)" "$$(pick)" "$$(pick)" "$$SIDE_COLOR"; \
			printf "$(CYAN)+---+---+---+$(RESET)%b     $(RESET)$(CYAN)|$(RESET)\n" "$$SIDE_COLOR"; \
			printf "$(CYAN)|$(RESET)%b   $(RESET)$(CYAN)|$(RESET)%b   $(RESET)$(CYAN)|$(RESET)%b   $(RESET)$(CYAN)|$(RESET)%b     $(RESET)$(CYAN)|$(RESET)\n" "$$(pick)" "$$(pick)" "$$(pick)" "$$SIDE_COLOR"; \
			printf "$(CYAN)+---+---+---+$(RESET)%b   $(RESET)$(CYAN)/$(RESET)\n" "$$SIDE_COLOR"; \
			printf "$(CYAN)|$(RESET)%b   $(RESET)$(CYAN)|$(RESET)%b   $(RESET)$(CYAN)|$(RESET)%b   $(RESET)$(CYAN)|$(RESET)%b $(RESET)$(CYAN)/$(RESET)\n" "$$(pick)" "$$(pick)" "$$(pick)" "$$SIDE_COLOR"; \
			printf "$(CYAN)+---+---+---+$(RESET)\n"; \
			sleep 0.1; \
		done; \
		printf "\n"; \
	'

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
	@printf "  $(GREEN)make flash$(RESET)            — 30s flashing cube showcase (Ctrl+C to stop early)\n"
	@printf "  $(GREEN)make help$(RESET)             — show this message\n"
	@printf "\n$(CYAN)  Usage:$(RESET)\n"
	@printf "  $(YELLOW)./rubik \"R2 U F' L2 D B R U2 L' F2\"$(RESET)                (mandatory, Kociemba only)\n"
	@printf "  $(YELLOW)./rubik_bonus \"...\"$(RESET)                                 (bonus, Kociemba by default)\n"
	@printf "  $(YELLOW)./rubik_bonus \"...\" -a thistlethwaite$(RESET)               (bonus, Thistlethwaite instead)\n\n"

.PHONY: all bonus test clean fclean fclean-raylib re valgrind debug run check env cloc list banner flash push help