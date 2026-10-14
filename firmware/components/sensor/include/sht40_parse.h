/* Sht40Parse.h - pure decoding of an SHT4x 6-byte measurement frame. */
#pragma once
#include "core.h"
#include <stdbool.h>
#include <stdint.h>
U8 Sht40Crc8(const U8 *pD, int len);
/* Returns false on CRC error. Outputs c°C and c%RH (0..10000). */
bool Sht40Parse(const U8 frame[6], I16 *pTempC100, U16 *pRhC100);
