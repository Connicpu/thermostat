/* fw_version.h - semver for the on-device menu, Zigbee SW build ID, and OTA file version.
 * firmware/version.txt must match these three numbers. The boot-log App version is
 * PROJECT_VER from version.cmake (an fw-v* tag, or version.txt plus the short hash). */
#pragma once
#include "core.h"
#define FW_VERSION_MAJOR 0
#define FW_VERSION_MINOR 3
#define FW_VERSION_PATCH 0
#define FW_STR2(x)       #x
#define FW_STR(x)        FW_STR2(x)
#define FW_VERSION       FW_STR(FW_VERSION_MAJOR) "." FW_STR(FW_VERSION_MINOR) "." FW_STR(FW_VERSION_PATCH)
/* Zigbee OTA file version derived from FW_VERSION: 0xMMmmpppp */
#define FW_OTA_FILE_VERSION \
    ((U32)(((U32)(FW_VERSION_MAJOR) << 24) | ((U32)(FW_VERSION_MINOR) << 16) | (U32)(FW_VERSION_PATCH)))
