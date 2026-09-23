/* ========================================================================
 * rubik.h — umbrella header
 *
 * One include for the whole project. As each module in
 * docs/en/04-architecture.md lands, its header joins the list below —
 * coord.h, solve.h, render.h (render.h only ever included by files under
 * src/render/, and src/render/ is never linked into the mandatory binary
 * — see the Makefile).
 * ======================================================================== */

#ifndef RUBIK_H
# define RUBIK_H

# include <stddef.h>
# include <stdint.h>
# include <stdbool.h>
# include <stdlib.h>
# include <stdio.h>
# include <string.h>

# include "cube.h"
# include "parse.h"

#endif
