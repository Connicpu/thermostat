#include "sht40_parse.h"

uint8_t sht40_crc8(const uint8_t *d, int len)
{
    uint8_t crc = 0xFF;                       /* poly 0x31, init 0xFF (Sensirion) */
    for (int i = 0; i < len; i++) {
        crc ^= d[i];
        for (int b = 0; b < 8; b++) crc = (crc & 0x80) ? (uint8_t)((crc << 1) ^ 0x31) : (uint8_t)(crc << 1);
    }
    return crc;
}

bool sht40_parse(const uint8_t f[6], int16_t *temp_c100, uint16_t *rh_c100)
{
    if (sht40_crc8(f, 2) != f[2] || sht40_crc8(f + 3, 2) != f[5]) return false;
    uint32_t rt = ((uint32_t)f[0] << 8) | f[1];
    uint32_t rh = ((uint32_t)f[3] << 8) | f[4];
    /* T = -45 + 175 * rt / 65535 ; RH = -6 + 125 * rh / 65535 (datasheet §4.6) */
    int32_t t = -4500 + (int32_t)((17500u * rt + 32767u) / 65535u);
    int32_t h = -600 + (int32_t)((12500u * rh + 32767u) / 65535u);
    if (h < 0) h = 0;
    if (h > 10000) h = 10000;
    *temp_c100 = (int16_t)t;
    *rh_c100 = (uint16_t)h;
    return true;
}
