/* schedule.c - see schedule.h. Pure C99. */
#include "core.h"
#include "schedule.h"
#include <stddef.h>

/** Returns a minute-of-day clamped to 0..1439. */
static U16 ClampMin(U16 m)
{
    return m >= SCHED_MIN_PER_DAY ? SCHED_MIN_PER_DAY - 1 : m;
}

/** See schedule.h. */
void SchedSanitize(const CtrlParams *pP, SchedCfg *pC)
{
    pC->dayCoolStart = ClampMin(pC->dayCoolStart);
    pC->nightCoolStart = ClampMin(pC->nightCoolStart);
    pC->dayHeatStart = ClampMin(pC->dayHeatStart);
    pC->nightHeatStart = ClampMin(pC->nightHeatStart);
    pC->dayCool = Clamp16(pC->dayCool, pP->minSp, pP->maxSp);
    pC->nightCool = Clamp16(pC->nightCool, pP->minSp, pP->maxSp);
    pC->dayHeat = Clamp16(pC->dayHeat, pP->minSp, pP->maxSp);
    pC->nightHeat = Clamp16(pC->nightHeat, pP->minSp, pP->maxSp);
    if (pC->boostMin > SCHED_BOOST_MAX_MIN)
        pC->boostMin = SCHED_BOOST_MAX_MIN;
    pC->boostOffset = Clamp16(pC->boostOffset, 0, SCHED_BOOST_OFS_MAX);
}

/** Same-day or midnight-crossing DAY period. See schedule.h. */
SchedPeriod SchedPeriodAt(U16 day, U16 night, U16 minute)
{
    minute %= SCHED_MIN_PER_DAY;
    if (day == night)
        return kSchedDay;
    if (day < night)
        return (minute >= day && minute < night) ? kSchedDay : kSchedNight;
    /* day period crosses midnight (e.g. day 20:00, night 04:00) */
    return (minute >= day || minute < night) ? kSchedDay : kSchedNight;
}

/** boostMin capped to the length of the heating DAY period. See schedule.h. */
U16 SchedBoostLength(const SchedCfg *pC)
{
    if (!pC->boostMin || !pC->boostOffset)
        return 0;
    U16 day = pC->dayHeatStart % SCHED_MIN_PER_DAY, night = pC->nightHeatStart % SCHED_MIN_PER_DAY;
    if (day == night)
        return 0; /* program disabled: no "morning" to boost */
    U16 dayLen = (U16)((night + SCHED_MIN_PER_DAY - day) % SCHED_MIN_PER_DAY);
    return pC->boostMin < dayLen ? pC->boostMin : dayLen; /* never runs into the night period */
}

/** See schedule.h. */
bool SchedBoostWindow(const SchedCfg *pC, U16 minute)
{
    U16 len = SchedBoostLength(pC);
    if (!len)
        return false;
    minute %= SCHED_MIN_PER_DAY;
    U16 since = (U16)((minute + SCHED_MIN_PER_DAY - pC->dayHeatStart % SCHED_MIN_PER_DAY) % SCHED_MIN_PER_DAY);
    return since < len;
}

/** See schedule.h. */
SchedPeriod SchedCurrent(const SchedCfg *pC, CtrlMode mode, bool timeKnown, U16 minute)
{
    if (!timeKnown)
        return kSchedDay;
    return mode == kCtrlModeHeat ? SchedPeriodAt(pC->dayHeatStart, pC->nightHeatStart, minute)
                                 : SchedPeriodAt(pC->dayCoolStart, pC->nightCoolStart, minute);
}

/** See schedule.h. */
void SchedProgramSp(const SchedCfg *pC, bool known, U16 minute, I16 *pHeat, I16 *pCool)
{
    *pHeat = SchedCurrent(pC, kCtrlModeHeat, known, minute) == kSchedDay ? pC->dayHeat : pC->nightHeat;
    *pCool = SchedCurrent(pC, kCtrlModeCool, known, minute) == kSchedDay ? pC->dayCool : pC->nightCool;
}

/** Program setpoints plus morning boost, and the state to report. See schedule.h. */
SchedState SchedApply(const CtrlParams *pP, const SchedCfg *pC, bool known, U16 minute, CtrlSettings *pS)
{
    SchedProgramSp(pC, known, minute, &pS->heatSp, &pS->coolSp);
    if (!known)
        return kSchedStateTimeUnknown;
    /* the period shown/reported follows the active program; OFF shows the heating program */
    SchedPeriod per = SchedCurrent(pC, pS->mode == kCtrlModeCool ? kCtrlModeCool : kCtrlModeHeat, true, minute);
    if (pS->mode == kCtrlModeHeat && SchedBoostWindow(pC, minute))
    {
        pS->heatSp = Clamp16((I32)pS->heatSp + pC->boostOffset, pP->minSp, pP->maxSp);
        return kSchedStateDayBoost;
    }
    return per == kSchedDay ? kSchedStateDay : kSchedStateNight;
}

/** See schedule.h. */
bool SchedEditCurrent(const CtrlParams *pP, SchedCfg *pC, bool heat, bool known, U16 minute, I16 sp)
{
    sp = Clamp16(sp, pP->minSp, pP->maxSp);
    SchedPeriod per = SchedCurrent(pC, heat ? kCtrlModeHeat : kCtrlModeCool, known, minute);
    I16 *pDst =
        heat ? (per == kSchedDay ? &pC->dayHeat : &pC->nightHeat) : (per == kSchedDay ? &pC->dayCool : &pC->nightCool);
    if (*pDst == sp)
        return false;
    *pDst = sp;
    return true;
}

/** See schedule.h. */
SchedKind SchedItemKind(SchedItem it)
{
    switch (it)
    {
    case kSchedItDayHeatStart:
    case kSchedItNightHeatStart:
    case kSchedItDayCoolStart:
    case kSchedItNightCoolStart:
        return kSchedKindTime;
    case kSchedItBoostMin:
        return kSchedKindMinutes;
    case kSchedItBoostOfs:
        return kSchedKindDelta;
    default:
        return kSchedKindTemp;
    }
}

/**
 * Returns a pointer to the SchedCfg field behind a menu item (U16 for times/minutes, I16
 * otherwise), or NULL for an unknown item. Casts away const: SchedGet() only reads through it.
 */
static void *ItemPtr(const SchedCfg *pC, SchedItem it)
{
    SchedCfg *pM = (SchedCfg *)pC;
    switch (it)
    {
    case kSchedItDayHeatStart:
        return &pM->dayHeatStart;
    case kSchedItDayHeat:
        return &pM->dayHeat;
    case kSchedItNightHeatStart:
        return &pM->nightHeatStart;
    case kSchedItNightHeat:
        return &pM->nightHeat;
    case kSchedItBoostMin:
        return &pM->boostMin;
    case kSchedItBoostOfs:
        return &pM->boostOffset;
    case kSchedItDayCoolStart:
        return &pM->dayCoolStart;
    case kSchedItDayCool:
        return &pM->dayCool;
    case kSchedItNightCoolStart:
        return &pM->nightCoolStart;
    case kSchedItNightCool:
        return &pM->nightCool;
    default:
        return NULL;
    }
}

/** Returns true if the item's field is a U16 (times and minutes), false for I16 temperatures. */
static bool ItemUnsigned(SchedItem it)
{
    SchedKind k = SchedItemKind(it);
    return k == kSchedKindTime || k == kSchedKindMinutes;
}

/** See schedule.h. */
I32 SchedGet(const SchedCfg *pC, SchedItem it)
{
    void *pP = ItemPtr(pC, it);
    if (!pP)
        return 0;
    return ItemUnsigned(it) ? *(U16 *)pP : *(I16 *)pP;
}

/** See schedule.h. */
void SchedSet(SchedCfg *pC, SchedItem it, I32 v)
{
    void *pP = ItemPtr(pC, it);
    if (!pP)
        return;
    if (ItemUnsigned(it))
        *(U16 *)pP = (U16)(v < 0 ? 0 : v > 0xFFFF ? 0xFFFF : v);
    else
        *(I16 *)pP = (I16)(v < -32768 ? -32768 : v > 32767 ? 32767 : v);
}
