#include "core.h"
#include "sht40_parse.h"

U8 Sht40Crc8(const U8 *pD, int len)
{
    U8 crc = 0xFF;                       /* poly 0x31, init 0xFF (Sensirion) */
    for (int i = 0; i < len; i++) {
        crc ^= pD[i];
        for (int b = 0; b < 8; b++) crc = (crc & 0x80) ? (U8)((crc << 1) ^ 0x31) : (U8)(crc << 1);
    }
    return crc;
}

bool Sht40Parse(const U8 f[6], I16 *pTempC100, U16 *pRhC100)
{
    if (Sht40Crc8(f, 2) != f[2] || Sht40Crc8(f + 3, 2) != f[5]) return false;
    U32 rt = ((U32)f[0] << 8) | f[1];
    U32 rh = ((U32)f[3] << 8) | f[4];
    /* T = -45 + 175 * rt / 65535 ; RH = -6 + 125 * rh / 65535 (datasheet §4.6) */
    I32 t = -4500 + (I32)((17500u * rt + 32767u) / 65535u);
    I32 h = -600 + (I32)((12500u * rh + 32767u) / 65535u);
    if (h < 0) h = 0;
    if (h > 10000) h = 10000;
    *pTempC100 = (I16)t;
    *pRhC100 = (U16)h;
    return true;
}
