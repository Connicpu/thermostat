/* Sht40Parse.h - pure decoding of an SHT4x 6-byte measurement frame. */
#pragma once
#include "core.h"
#include <stdbool.h>
#include <stdint.h>

/**
 * Sensirion CRC-8 (polynomial 0x31, init 0xFF, no reflection, no final XOR).
 *
 * `pD` points at the `len` bytes to check.
 *
 * Returns the CRC.
 */
U8 Sht40Crc8(const U8 *pD, int len);

/**
 * Decode a 6-byte SHT4x measurement frame (T msb, T lsb, T crc, RH msb, RH lsb, RH crc).
 *
 * Uses the datasheet conversions (T = -45 + 175*raw/65535 °C, RH = -6 + 125*raw/65535 %),
 * rounded to the nearest unit, in integer arithmetic. RH is clamped to 0..100 %. Pure function.
 * Outputs are untouched when false is returned.
 *
 * `frame` is the 6 raw bytes; `pTempC100` receives temperature, 0.01 °C; `pRhC100` receives relative humidity, 0.01 %RH (0..10000).
 *
 * Returns false on a CRC error in either word.
 */
bool Sht40Parse(const U8 frame[6], I16 *pTempC100, U16 *pRhC100);
