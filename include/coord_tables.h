#ifndef COORD_TABLES_H
# define COORD_TABLES_H

# include <stdint.h>
# include "cube.h"

/// Internal to src/coord/: the small constant tables and sizes that both
/// encode.c and decode.c need. Defined once in coord/tables.c so the two
/// files can never disagree. Nothing outside src/coord/ should include this
/// (the public interface is coord.h).

/// How many top/bottom edges (slots 0..7) and middle-layer edges (slots
/// 8..11) there are.
# define UD_EDGE_COUNT 8
# define SLICE_EDGE_COUNT 4

/// C(n, k) for n = 0..11 and k = 0..4 (used by the slice coordinate).
extern const uint16_t	g_binom[EDGE_COUNT][5];

/// Factorials 0! .. 7! (used by cperm, eperm and sperm).
extern const uint16_t	g_fact[CORNER_COUNT];

#endif
