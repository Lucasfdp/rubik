#include "rubik.h"

/// @brief Checks that a cube is physically reachable, as a t_parse_status.
///
/// Thin wrapper on purpose. The real maths (twist sum, flip sum,
/// permutation parity) lives in cube_is_valid() in cube/cubie.c, so
/// exactly one place can get it wrong. This function only translates that
/// bool into the same status type parse_notation() returns, so main.c
/// handles "bad notation" and "impossible cube" through one error path.
///
/// Run it before any search: an unreachable cube makes the solver hunt
/// for a solution that does not exist, which looks exactly like an
/// infinite loop (docs/en/08-risks-and-open-decisions.md).
///
/// @param cube Cube to check.
/// @return PARSE_OK, or PARSE_INVALID_CUBE.
t_parse_status	validate_cube(const t_cube *cube)
{
	if (!cube_is_valid(cube))
		return (PARSE_INVALID_CUBE);
	return (PARSE_OK);
}
