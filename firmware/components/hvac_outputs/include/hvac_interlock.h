/* HvacInterlock.h - the single place where output combinations are vetted.
 * Pure C so it is unit tested on the host together with the control logic. */
#pragma once
#include <stdbool.h>
#include <stdint.h>

typedef struct { bool w, y, g; } HvacState;

/* Returns the state that may actually be driven. Rule: W and Y are never
 * energised together - if both are requested, BOTH are dropped (the request
 * is a firmware bug; failing to "nothing running" is the safe side) and
 * *fault (if non-NULL) is set. G is passed through. */
HvacState HvacInterlock(HvacState req, bool *pFault);
