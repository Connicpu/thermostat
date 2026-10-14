/* control.c - see control.h. Pure C99, no platform dependencies. */
#include "core.h"
#include "control.h"
#include <string.h>

static I16 Clamp16(I32 v, I16 lo, I16 hi)
{
    return v < lo ? lo : (v > hi ? hi : (I16)v);
}

static bool Elapsed(U64 now, U64 since, U32 dur)
{
    return now >= since && (now - since) >= dur;
}

void CtrlInit(Ctrl *pC, const CtrlParams *pP, U64 nowMs)
{
    memset(pC, 0, sizeof(*pC));
    pC->p = *pP;
    pC->p.hysteresis = CtrlClampSwing(pP->hysteresis);
    /* Treat boot as "everything just turned off": after a power blip the
     * compressor gets its full minimum off-time before restarting. */
    pC->wOffAt = pC->yOffAt = nowMs;
    pC->lastValidAt = nowMs;
    /* Start in failsafe until the first good samples arrive. */
    pC->fault = kCtrlFaultSensorTimeout;
}

void CtrlSanitizeSettings(const CtrlParams *pP, CtrlSettings *pS, bool keepHeat)
{
    if (!CtrlModeValid(pS->mode)) pS->mode = kCtrlModeOff;
    if (pS->fan != kCtrlFanOn && pS->fan != kCtrlFanAuto) pS->fan = kCtrlFanAuto;
    (void)keepHeat;
    pS->heatSp = Clamp16(pS->heatSp, pP->minSp, pP->maxSp);
    pS->coolSp = Clamp16(pS->coolSp, pP->minSp, pP->maxSp);
}

void CtrlFeedSensor(Ctrl *pC, const CtrlInput *pIn, U64 nowMs)
{
    if (!pIn->valid) {
        pC->goodStreak = 0;
        return; /* timeout logic in CtrlStep handles a dead sensor */
    }
    if (pIn->temp < pC->p.tempMin || pIn->temp > pC->p.tempMax) {
        pC->fault = kCtrlFaultTempRange;
        pC->goodStreak = 0;
        pC->lastTemp = pIn->temp;
        pC->haveTemp = true;
        return;
    }
    pC->lastValidAt = nowMs;
    pC->lastTemp = pIn->temp;
    pC->haveTemp = true;
    if (pC->fault != kCtrlFaultNone) {
        if (++pC->goodStreak >= pC->p.recoverSamples) {
            pC->fault = kCtrlFaultNone;
            pC->goodStreak = 0;
        }
    }
}

static void SetW(Ctrl *pC, bool on, U64 now)
{
    if (on == pC->w) return;
    pC->w = on;
    if (on) { pC->wOnAt = now; pC->wRan = true; }
    else {
        pC->wOffAt = now;
        pC->fanRunonUntil = now + pC->p.fanRunonHeatMs;
    }
}

static void SetY(Ctrl *pC, bool on, U64 now)
{
    if (on == pC->y) return;
    pC->y = on;
    if (on) { pC->yOnAt = now; pC->yRan = true; }
    else {
        pC->yOffAt = now;
        pC->fanRunonUntil = now + pC->p.fanRunonCoolMs;
    }
}

CtrlOutput CtrlStep(Ctrl *pC, const CtrlSettings *pIn, U64 now)
{
    CtrlOutput o = {0};
    CtrlSettings s = *pIn;
    CtrlSanitizeSettings(&pC->p, &s, true);

    /* ---- failsafe: stale or implausible sensor => everything off, now. ---- */
    if (!Elapsed(now, pC->lastValidAt, 0) || (now - pC->lastValidAt) > pC->p.sensorTimeoutMs) {
        if (pC->fault == kCtrlFaultNone) pC->goodStreak = 0;
        if (pC->fault != kCtrlFaultTempRange) pC->fault = kCtrlFaultSensorTimeout;
    }
    if (pC->fault != kCtrlFaultNone || !pC->haveTemp) {
        SetW(pC, false, now);
        SetY(pC, false, now);
        pC->g = false;
        pC->fanRunonUntil = 0;      /* no run-on in failsafe */
        pC->heatCall = pC->coolCall = false;
        if (pC->boost.kind) { pC->boost.kind = kBoostNone; pC->boost.lastEnd = kBoostEndFault; }
        o.fault = pC->fault != kCtrlFaultNone ? pC->fault : kCtrlFaultSensorTimeout;
        return o;
    }

    const I16 t = pC->lastTemp, h = pC->p.hysteresis;

    /* ---- boost: end conditions, then override what we ask for ---- */
    bool forceHeat = false, forceCool = false;
    if (pC->boost.kind != kBoostNone) {
        CtrlBoost *pB = &pC->boost;
        CtrlBoostEnd end = kBoostEndNone;
        if (s.mode != pB->baseMode) end = kBoostEndModeChanged;
        else if (now >= pB->endAt) end = kBoostEndExpired;
        else if (pB->kind == kBoostToTarget &&
                 ((pB->dir == kCtrlModeHeat && t >= pB->sp) || (pB->dir == kCtrlModeCool && t <= pB->sp)))
            end = kBoostEndReached;
        else if (pB->kind == kBoostTimed &&
                 ((pB->dir == kCtrlModeHeat && t >= pC->p.maxSp) || (pB->dir == kCtrlModeCool && t <= pC->p.minSp)))
            end = kBoostEndLimit;            /* comfort cap on timed boosts */
        if (end != kBoostEndNone) {
            pB->kind = kBoostNone;
            pB->lastEnd = end;
            pC->softStop = true;
            /* drop the boost's latched call; normal hysteresis re-evaluates below */
            if (pB->dir == kCtrlModeHeat) pC->heatCall = false; else pC->coolCall = false;
        } else {
            s.mode = pB->dir;
            if (pB->dir == kCtrlModeHeat) { forceHeat = true; }
            else                          { forceCool = true; }
        }
    }
    const bool heatAllowed = (s.mode == kCtrlModeHeat);
    const bool coolAllowed = (s.mode == kCtrlModeCool);

    /* ---- hysteresis (latched calls) ---- */
    if (!heatAllowed)               pC->heatCall = false;
    else if (t <= s.heatSp - h)     pC->heatCall = true;
    else if (t >= s.heatSp + h)     pC->heatCall = false;

    if (!coolAllowed)               pC->coolCall = false;
    else if (t >= s.coolSp + h)     pC->coolCall = true;
    else if (t <= s.coolSp - h)     pC->coolCall = false;

    if (forceHeat) { pC->heatCall = true; pC->coolCall = false; }
    if (forceCool) { pC->coolCall = true; pC->heatCall = false; }

    /* One mode at a time makes both calls impossible, but be explicit: an active stage wins. */
    if (pC->heatCall && pC->coolCall) {
        if (pC->y) pC->heatCall = false; else pC->coolCall = false;
    }
    o.heatDemand = pC->heatCall;
    o.coolDemand = pC->coolCall;

    /* ---- heat (W) ---- */
    if (pC->w) {
        bool stop = (!heatAllowed && (!pC->softStop || Elapsed(now, pC->wOnAt, pC->p.heatMinOnMs))) ||
                    (!pC->heatCall && Elapsed(now, pC->wOnAt, pC->p.heatMinOnMs));
        if (stop) SetW(pC, false, now);
    } else if (pC->heatCall && !pC->y) {
        if (Elapsed(now, pC->wOffAt, pC->p.heatMinOffMs) &&
            (!pC->yRan || Elapsed(now, pC->yOffAt, pC->p.changeoverMs)))
            SetW(pC, true, now);
        else
            o.waiting = true;
    }

    /* ---- cool (Y, compressor) ---- */
    if (pC->y) {
        bool stop = (!coolAllowed && (!pC->softStop || Elapsed(now, pC->yOnAt, pC->p.compMinOnMs))) ||
                    (!pC->coolCall && Elapsed(now, pC->yOnAt, pC->p.compMinOnMs));
        if (stop) SetY(pC, false, now);
    } else if (pC->coolCall && !pC->w) {
        if (Elapsed(now, pC->yOffAt, pC->p.compMinOffMs) &&
            (!pC->wRan || Elapsed(now, pC->wOffAt, pC->p.changeoverMs)))
            SetY(pC, true, now);
        else
            o.waiting = true;
    }

    if (!pC->w && !pC->y) pC->softStop = false;

    /* ---- fan (G) ---- */
    bool g = (s.fan == kCtrlFanOn) || pC->y || (pC->w && pC->p.heatDrivesFan) ||
             (now < pC->fanRunonUntil);
    pC->g = g;

    o.w = pC->w;
    o.y = pC->y;
    o.g = pC->g;
    o.fault = kCtrlFaultNone;
    return o;
}

CtrlMode CtrlNextMode(CtrlMode m)
{
    switch (m) {
    case kCtrlModeOff:  return kCtrlModeHeat;
    case kCtrlModeHeat: return kCtrlModeCool;
    default:             return kCtrlModeOff;
    }
}

bool CtrlModeValid(int v)
{
    return v == kCtrlModeOff || v == kCtrlModeCool || v == kCtrlModeHeat;
}

const char *CtrlFaultStr(CtrlFault f)
{
    switch (f) {
    case kCtrlFaultNone:           return "OK";
    case kCtrlFaultSensorTimeout: return "SENSOR FAIL";
    case kCtrlFaultTempRange:     return "TEMP RANGE";
    default:                        return "?";
    }
}

CtrlBoostResult CtrlBoostStart(Ctrl *pC, const CtrlSettings *pS, CtrlBoostKind kind,
                                     CtrlBoostDir dir, I16 sp, U16 minutes, U64 now)
{
    if (pC->fault != kCtrlFaultNone || !pC->haveTemp) return kBoostErrFault;
    if (dir != kBoostDirAuto && dir != kBoostDirHeat && dir != kBoostDirCool) return kBoostErrArg;
    const I16 t = pC->lastTemp, h = pC->p.hysteresis;
    CtrlBoost b = {.kind = kind, .baseMode = pS->mode, .lastEnd = kBoostEndNone};

    if (kind == kBoostToTarget) {
        sp = Clamp16(sp, pC->p.minSp, pC->p.maxSp);
        if (dir == kBoostDirAuto) {
            if (t <= sp - h)      b.dir = kCtrlModeHeat;
            else if (t >= sp + h) b.dir = kCtrlModeCool;
            else return kBoostErrAtTarget;
        } else {
            b.dir = dir == kBoostDirHeat ? kCtrlModeHeat : kCtrlModeCool;
            if ((b.dir == kCtrlModeHeat && t >= sp) || (b.dir == kCtrlModeCool && t <= sp)) return kBoostErrAtTarget;
        }
        b.sp = sp;
        b.endAt = now + (U64)BOOST_TARGET_MAX_MIN * 60000u;
    } else if (kind == kBoostTimed) {
        if (minutes < BOOST_TIMED_MIN_MIN || minutes > BOOST_TIMED_MAX_MIN) return kBoostErrArg;
        if (dir == kBoostDirAuto) {
            if (pS->mode == kCtrlModeHeat)      b.dir = kCtrlModeHeat;
            else if (pS->mode == kCtrlModeCool) b.dir = kCtrlModeCool;
            else if (t < pS->heatSp)            b.dir = kCtrlModeHeat;
            else if (t > pS->coolSp)            b.dir = kCtrlModeCool;
            else return kBoostErrNoDirection;
        } else {
            b.dir = dir == kBoostDirHeat ? kCtrlModeHeat : kCtrlModeCool;
        }
        if ((b.dir == kCtrlModeHeat && t >= pC->p.maxSp) || (b.dir == kCtrlModeCool && t <= pC->p.minSp))
            return kBoostErrAtTarget;
        b.endAt = now + (U64)minutes * 60000u;
    } else {
        return kBoostErrArg;
    }
    /* switching direction mid-boost: drop the old latched call */
    if (pC->boost.kind && pC->boost.dir != b.dir) pC->heatCall = pC->coolCall = false;
    pC->boost = b;
    pC->softStop = true;   /* a stage the boost displaces still gets its min on-time */
    return kBoostOk;
}

void CtrlBoostCancel(Ctrl *pC)
{
    if (!pC->boost.kind) return;
    if (pC->boost.dir == kCtrlModeHeat) pC->heatCall = false; else pC->coolCall = false;
    pC->boost.kind = kBoostNone;
    pC->boost.lastEnd = kBoostEndCancelled;
    pC->softStop = true;
}

CtrlBoostStatus CtrlGetBoostStatus(const Ctrl *pC, U64 now)
{
    CtrlBoostStatus st = {.kind = pC->boost.kind, .dir = pC->boost.dir, .sp = pC->boost.sp,
                              .lastEnd = pC->boost.lastEnd};
    if (st.kind) st.remainingS = now >= pC->boost.endAt ? 0 : (U32)((pC->boost.endAt - now + 999) / 1000);
    return st;
}

I16 CtrlClampSwing(I32 v)
{
    return Clamp16(v, CTRL_SWING_MIN, CTRL_SWING_MAX);
}

void CtrlSetSwing(Ctrl *pC, I16 swing)
{
    pC->p.hysteresis = CtrlClampSwing(swing);
}
