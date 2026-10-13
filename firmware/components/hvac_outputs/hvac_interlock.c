#include "hvac_interlock.h"

hvac_state_t hvac_interlock(hvac_state_t req, bool *fault)
{
    hvac_state_t out = req;
    bool bad = req.w && req.y;
    if (bad) {
        out.w = false;
        out.y = false;
    }
    if (fault) *fault = bad;
    return out;
}
