#!/usr/bin/env bash
# End-to-end tests of the ./rubik binary: exit codes, stderr messages, and
# stdout cleanliness. Usage: bash tests/test_cli.sh [path-to-rubik]
#
# If valgrind is installed, every single run is wrapped in it (subject R6:
# no leak, no segfault, no double free, on every path including errors).
# A valgrind finding shows up here as exit code 99.
#
# A valid scramble must exit 0 and print ONE line of moves. The solver is
# checked with the binary itself: scramble + printed solution, fed back in,
# must be a solved cube (one empty line, zero moves).

BIN="${1:-.././rubik}"
pass=0
fail=0
WRAP=()
if command -v valgrind > /dev/null 2>&1; then
	WRAP=(valgrind -q --leak-check=full --show-leak-kinds=all
		--errors-for-leak-kinds=all --error-exitcode=99)
	echo "  (running every case under valgrind)"
fi

OUT=$(mktemp)
ERR=$(mktemp)
trap 'rm -f "$OUT" "$ERR"' EXIT

# run <args...>: sets RC, and fills $OUT / $ERR
run()
{
	"${WRAP[@]}" "$BIN" "$@" > "$OUT" 2> "$ERR"
	RC=$?
}

ok()
{
	pass=$((pass + 1))
}

bad()
{
	fail=$((fail + 1))
	echo "  FAIL: $1"
	echo "        exit=$RC stdout=[$(cat "$OUT")] stderr=[$(cat "$ERR")]"
}

# expect_error <description> <stderr substring> <args...>
# Must exit 1, print the substring on stderr, and print nothing on stdout.
expect_error()
{
	local desc="$1" want="$2"
	shift 2
	run "$@"
	if [ "$RC" -eq 1 ] && grep -qF -- "$want" "$ERR" && [ ! -s "$OUT" ]; then
		ok
	else
		bad "$desc"
	fi
}

# expect_accepted <description> <args...>
# Must NOT be rejected as a bad scramble (exit 0, nothing on stderr).
expect_accepted()
{
	local desc="$1"
	shift
	run "$@"
	if [ "$RC" -eq 0 ] && [ ! -s "$ERR" ]; then
		ok
	else
		bad "$desc"
	fi
}

# expect_solved_output <description> <args...>: exit 0, stdout is one empty line.
expect_solved_output()
{
	local desc="$1"
	shift
	run "$@"
	if [ "$RC" -eq 0 ] && [ "$(cat "$OUT")" = "" ] && [ "$(wc -c < "$OUT")" -eq 1 ] \
		&& [ ! -s "$ERR" ]; then
		ok
	else
		bad "$desc"
	fi
}

# expect_solves <description> <scramble>: exit 0, stdout is ONE non-empty line
# of at most <max> words, and "<scramble> <that line>" is a solved cube.
expect_solves()
{
	local desc="$1" scramble="$2" max="${3:-30}" sol
	run "$scramble"
	sol=$(cat "$OUT")
	if [ "$RC" -ne 0 ] || [ -z "$sol" ] || [ "$(wc -l < "$OUT")" -ne 1 ] \
		|| [ -s "$ERR" ] || [ "$(wc -w < "$OUT")" -gt "$max" ]; then
		bad "$desc"
		return
	fi
	run "$scramble $sol"
	if [ "$RC" -eq 0 ] && [ "$(wc -c < "$OUT")" -eq 1 ] && [ ! -s "$ERR" ]; then
		ok
	else
		bad "$desc (solution does not solve it)"
	fi
}

if [ ! -x "$BIN" ]; then
	echo "  $BIN not found: run make first"
	exit 1
fi

# --- argument count ---------------------------------------------------------
expect_error "no argument"          "usage"
expect_error "two arguments"        "usage" "R" "U"
expect_error "three arguments"      "usage" "R" "U" "F"

# --- empty / whitespace-only ------------------------------------------------
expect_error "empty string"         "empty scramble" ""
expect_error "spaces only"          "empty scramble" "   "
expect_error "tab only"             "empty scramble" $'\t'
expect_error "newline only"         "empty scramble" $'\n'
expect_error "CRLF only"            "empty scramble" $'\r\n'

# --- bad faces (slices and whole-cube rotations are forbidden) --------------
for t in M E S x y z X r u f b l d w W 1 0 "'" 2 - '*' '@' "R X" "R2 X" "U R f" "R U m"; do
	expect_error "bad face [$t]"    "unknown face" "$t"
done

# --- bad modifiers ----------------------------------------------------------
for t in R3 R1 R0 RR RU 'R"' 'R`' R- R+ Rw "U R3" "U R2 D9"; do
	expect_error "bad modifier [$t]" "bad move modifier" "$t"
done

# --- tokens too long --------------------------------------------------------
for t in "R''" "R2'" "R'2" R22 RRR R2X "R U2X" $'R\xe2\x80\x99' UUUUUUUUUU; do
	expect_error "token too long [$t]" "token too long" "$t"
done

# --- size limits ------------------------------------------------------------
many=$(printf 'R %.0s' $(seq 1 257))
expect_error "257 moves"            "too many moves" "$many"
expect_error "5000 chars"           "too many moves" "$(printf 'R%.0s' $(seq 1 5000))"
expect_error "padded past buffer"   "too many moves" "$(printf ' %.0s' $(seq 1 800))R"
pairs=$(printf "R R' %.0s" $(seq 1 128))
expect_solved_output "256 moves (128 x R R') is accepted and solved" "$pairs"

# --- valid scrambles are accepted -------------------------------------------
expect_accepted "single move"        "R"
expect_accepted "subject example"    "F R U2 B' L' D'"
expect_accepted "subject long example" "R2 D' B' D F2 R F2 R2 U L' F2 U' B' L2 R D B' R' B2 L2 F2 L2 R2 U2 D2"
expect_accepted "multiple spaces"    "R   U    F"
expect_accepted "tabs"               $'R\tU\tF'
expect_accepted "leading/trailing"   "   R U F   "
expect_accepted "trailing newline"   $'R U F\n'
expect_accepted "trailing CRLF"      $'R U F\r\n'
expect_accepted "newline separated"  $'R\nU\nF'

# --- already solved: correct answer is zero moves = one empty line ----------
expect_solved_output "R R'"          "R R'"
expect_solved_output "R2 R2"         "R2 R2"
expect_solved_output "R R R R"       "R R R R"
expect_solved_output "sexy x6"       "R U R' U' R U R' U' R U R' U' R U R' U' R U R' U' R U R' U'"
expect_solved_output "solved w/ newline" $'R R\'\n'

# --- unsolved cubes get a real solution -------------------------------------
expect_solves "single move (one move back)" "R" 1
expect_solves "two moves"            "R U" 2
expect_solves "subject example"      "F R U2 B' L' D'"
expect_solves "subject long example" "R2 D' B' D F2 R F2 R2 U L' F2 U' B' L2 R D B' R' B2 L2 F2 L2 R2 U2 D2"
expect_solves "superflip"                   "U R2 F B R B2 R U2 L B2 R U' D' R2 F R' L B2 U2 F2"
expect_solves "sexy x5 (order 6)"    "R U R' U' R U R' U' R U R' U' R U R' U' R U R' U'"
expect_solves "only F turns"         "F F' F2 F" 1
expect_solves "newline separated"    $'R\nU\nF'
expect_solves "226 moves (room left to append the answer)" "$(printf 'R U2 F %.0s' $(seq 1 75)) R"

# --- the output is one line of plain notation, nothing else ------------------
run "F R U2 B' L' D'"
if grep -qE "^([URFDLB][2']? )*[URFDLB][2']?$" "$OUT" && [ "$(wc -l < "$OUT")" -eq 1 ]; then
	ok
else
	bad "solution is one line of space-separated moves"
fi

echo "  cli: $((pass + fail)) checks, $fail failed"
[ "$fail" -eq 0 ]
