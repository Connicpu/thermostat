#include "hvac_interlock.h"

/** Drop W and Y together if both are requested. See hvac_interlock.h. */
HvacState HvacInterlock(HvacState req, bool *pFault)
{
    HvacState out = req;
    bool bad = req.w && req.y;
    if (bad) {
        out.w = false;
        out.y = false;
    }
    if (pFault) *pFault = bad;
    return out;
}
