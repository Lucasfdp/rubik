#ifndef SCRAMBLE_H
# define SCRAMBLE_H

# include <stddef.h>
# include "cube.h"

/// @brief Fills out[0..count) with a random legal move sequence, with no
///        two consecutive moves on the same face (docs/en/03b-3d-
///        implementation-plan.md section 6.2) — the one quality rule
///        that matters for "does this look like a real scramble". This
///        is intentionally the simple version, not a WCA-grade scrambler.
///
/// @param out   Buffer of at least count moves.
/// @param count How many moves to generate.
/// @param seed  In/out PRNG state — caller owns it (seed once from
///              time(NULL) at program start, not once per call).
void	scramble_generate(t_move *out, size_t count, unsigned int *seed);

#endif
