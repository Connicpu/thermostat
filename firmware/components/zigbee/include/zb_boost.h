/* zb_boost.h - manufacturer-specific "Boost" cluster (wire protocol shared
 * with ha/zha_quirk/connie_thermostat_v3.py and z2m/thermostat-v3.js).
 *
 * Cluster 0xFC57, server, endpoint 1. Every attribute is manufacturer-specific
 * with manufacturer code 0x434E (frames must carry it). */
#pragma once

#define ZB_BOOST_CLUSTER_ID 0xFC57
#define ZB_BOOST_MANUF_CODE 0x434E

enum
{
    kZbBoostAttrSetpoint = 0x0000,   /* int16  RW  c°C, boost-to-target setpoint            */
    kZbBoostAttrDuration = 0x0001,   /* uint16 RW  minutes, timed boost length (1..120)      */
    kZbBoostAttrMode = 0x0002,       /* enum8  RW  0 auto, 1 heat, 2 cool                    */
    kZbBoostAttrCommand = 0x0003,    /* enum8  RW  write 1 start-to-target, 2 start timed, 0 cancel */
    kZbBoostAttrState = 0x0004,      /* enum8  R   see kZbBoostState*                      */
    kZbBoostAttrRemaining = 0x0005,  /* uint16 R   minutes left (0 when idle)                */
    kZbBoostAttrLastResult = 0x0006, /* enum8  R   see kZbBoostResult*                     */
    /* not boost-specific, but lives here so there is one manufacturer cluster */
    kZbAttrSwing = 0x0010, /* uint16 RW  c°C each side of setpoint, 10..100 (0.1..1.0 °C), default 50 */

    /* Day/night program schedule (see components/control/include/schedule.h).
     * Times: uint16 minutes after local midnight, 0..1439. Temps: int16 c°C, 1000..3200.
     * All RW ones are persisted; all are reported to the coordinator when changed on the device. */
    kZbAttrDayCoolStart = 0x0020,    /* uint16 RW  default 480  (08:00) */
    kZbAttrDayCoolTemp = 0x0021,     /* int16  RW  default 2450 (24.5 °C) */
    kZbAttrNightCoolStart = 0x0022,  /* uint16 RW  default 1320 (22:00) */
    kZbAttrNightCoolTemp = 0x0023,   /* int16  RW  default 2400 (24.0 °C) */
    kZbAttrDayHeatStart = 0x0024,    /* uint16 RW  default 360  (06:00) */
    kZbAttrDayHeatTemp = 0x0025,     /* int16  RW  default 2000 (20.0 °C) */
    kZbAttrNightHeatStart = 0x0026,  /* uint16 RW  default 1320 (22:00) */
    kZbAttrNightHeatTemp = 0x0027,   /* int16  RW  default 1800 (18.0 °C) */
    kZbAttrMorningBoostMin = 0x0028, /* uint16 RW  0..240 min from day heat start, default 120; 0 = off */
    kZbAttrMorningBoostOfs = 0x0029, /* int16  RW  0..300 c°C added in HEAT mode, default 50 (+0.5 °C) */
    kZbAttrSchedState =
        0x002A, /* enum8  R   SchedState: 0 no clock (day used), 1 day, 2 night, 3 day + morning boost */
};

#define ZB_SCHED_ATTR_FIRST kZbAttrDayCoolStart
#define ZB_SCHED_ATTR_LAST  kZbAttrMorningBoostOfs

enum
{
    kZbBoostCmdCancel = 0,
    kZbBoostCmdStartTarget = 1,
    kZbBoostCmdStartTimed = 2
};

enum
{
    kZbBoostStateOff = 0,
    kZbBoostStateHeatToTarget = 1,
    kZbBoostStateCoolToTarget = 2,
    kZbBoostStateHeatTimed = 3,
    kZbBoostStateCoolTimed = 4,
};

enum
{
    kZbBoostResultNone = 0,
    kZbBoostResultStarted = 1,
    kZbBoostResultReached = 2,
    kZbBoostResultExpired = 3,
    kZbBoostResultCancelled = 4,
    kZbBoostResultModeChanged = 5,
    kZbBoostResultFault = 6,
    kZbBoostResultLimit = 7,
    kZbBoostResultRejectedArg = 16,
    kZbBoostResultRejectedFault = 17,
    kZbBoostResultRejectedAtTarget = 18,
    kZbBoostResultRejectedNoDirection = 19,
};
