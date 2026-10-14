/* HvacInterlock.h - the single place where output combinations are vetted.
 * Pure C so it is unit tested on the host together with the control logic. */
#pragma once
#include <stdbool.h>
#include <stdint.h>

/**
 * Requested or permitted state of the three 24 VAC thermostat outputs.
 * w = W (heat call), y = Y (cooling compressor call), g = G (indoor blower fan).
 */
typedef struct { bool w, y, g; } HvacState;

/**
 * Vet a requested output combination; the single place where output combinations are checked.
 *
 * Rule: W and Y are never energised together. If both are requested, BOTH are dropped (the
 * request is a firmware bug, and "nothing running" is the safe side). G is passed through.
 * Pure function, no side effects apart from *pFault.
 *
 * @param req     requested outputs
 * @param pFault  if non-NULL, set to true when the W+Y rule fired, false otherwise
 * @return the state that may actually be driven
 */
HvacState HvacInterlock(HvacState req, bool *pFault);
