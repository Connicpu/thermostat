/* sht40_parse.h - pure decoding of an SHT4x 6-byte measurement frame. */
#pragma once
#include <stdbool.h>
#include <stdint.h>
uint8_t sht40_crc8(const uint8_t *d, int len);
/* Returns false on CRC error. Outputs c°C and c%RH (0..10000). */
bool sht40_parse(const uint8_t frame[6], int16_t *temp_c100, uint16_t *rh_c100);
