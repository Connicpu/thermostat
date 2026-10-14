/* fw_version.h - single source of the firmware version string (menu, Zigbee SW build ID). */
#pragma once
#define FW_VERSION_MAJOR 0
#define FW_VERSION_MINOR 2
#define FW_VERSION_PATCH 1
#define FW_STR2(x) #x
#define FW_STR(x) FW_STR2(x)
#define FW_VERSION FW_STR(FW_VERSION_MAJOR) "." FW_STR(FW_VERSION_MINOR) "." FW_STR(FW_VERSION_PATCH)
/* Zigbee OTA file version derived from FW_VERSION: 0xMMmmpppp */
#define FW_OTA_FILE_VERSION ((uint32_t)(((FW_VERSION_MAJOR) << 24) | ((FW_VERSION_MINOR) << 16) | (FW_VERSION_PATCH)))
