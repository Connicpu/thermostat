/* zb_boost.h - manufacturer-specific "Boost" cluster (wire protocol shared
 * with ha/zha_quirk/connie_thermostat_v3.py and z2m/thermostat-v3.js).
 *
 * Cluster 0xFC57, server, endpoint 1. Every attribute is manufacturer-specific
 * with manufacturer code 0x131B (frames must carry it). */
#pragma once

#define ZB_BOOST_CLUSTER_ID     0xFC57
#define ZB_BOOST_MANUF_CODE     0x131B

enum {
    ZB_BOOST_ATTR_SETPOINT    = 0x0000, /* int16  RW  c°C, boost-to-target setpoint            */
    ZB_BOOST_ATTR_DURATION    = 0x0001, /* uint16 RW  minutes, timed boost length (1..120)      */
    ZB_BOOST_ATTR_MODE        = 0x0002, /* enum8  RW  0 auto, 1 heat, 2 cool                    */
    ZB_BOOST_ATTR_COMMAND     = 0x0003, /* enum8  RW  write 1 start-to-target, 2 start timed, 0 cancel */
    ZB_BOOST_ATTR_STATE       = 0x0004, /* enum8  R   see ZB_BOOST_STATE_*                      */
    ZB_BOOST_ATTR_REMAINING   = 0x0005, /* uint16 R   minutes left (0 when idle)                */
    ZB_BOOST_ATTR_LAST_RESULT = 0x0006, /* enum8  R   see ZB_BOOST_RESULT_*                     */
    /* not boost-specific, but lives here so there is one manufacturer cluster */
    ZB_ATTR_SWING             = 0x0010, /* uint16 RW  c°C each side of setpoint, 10..100 (0.1..1.0 °C), default 50 */
};

enum { ZB_BOOST_CMD_CANCEL = 0, ZB_BOOST_CMD_START_TARGET = 1, ZB_BOOST_CMD_START_TIMED = 2 };

enum {
    ZB_BOOST_STATE_OFF = 0, ZB_BOOST_STATE_HEAT_TO_TARGET = 1, ZB_BOOST_STATE_COOL_TO_TARGET = 2,
    ZB_BOOST_STATE_HEAT_TIMED = 3, ZB_BOOST_STATE_COOL_TIMED = 4,
};

enum {
    ZB_BOOST_RESULT_NONE = 0, ZB_BOOST_RESULT_STARTED = 1, ZB_BOOST_RESULT_REACHED = 2,
    ZB_BOOST_RESULT_EXPIRED = 3, ZB_BOOST_RESULT_CANCELLED = 4, ZB_BOOST_RESULT_MODE_CHANGED = 5,
    ZB_BOOST_RESULT_FAULT = 6, ZB_BOOST_RESULT_LIMIT = 7,
    ZB_BOOST_RESULT_REJECTED_ARG = 16, ZB_BOOST_RESULT_REJECTED_FAULT = 17,
    ZB_BOOST_RESULT_REJECTED_AT_TARGET = 18, ZB_BOOST_RESULT_REJECTED_NO_DIRECTION = 19,
};
