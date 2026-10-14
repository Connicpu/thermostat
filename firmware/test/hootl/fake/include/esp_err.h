/* HOOTL fake: esp_err.h (values match ESP-IDF). */
#pragma once
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
typedef int esp_err_t;
#define ESP_OK                             0
#define ESP_FAIL                           -1
#define ESP_ERR_NO_MEM                     0x101
#define ESP_ERR_INVALID_ARG                0x102
#define ESP_ERR_INVALID_STATE              0x103
#define ESP_ERR_INVALID_SIZE               0x104
#define ESP_ERR_NOT_FOUND                  0x105
#define ESP_ERR_NOT_SUPPORTED              0x106
#define ESP_ERR_TIMEOUT                    0x107
#define ESP_ERR_INVALID_RESPONSE           0x108
#define ESP_ERR_INVALID_CRC                0x109
#define ESP_ERR_INVALID_VERSION            0x10A
#define ESP_ERR_NVS_BASE                   0x1100
#define ESP_ERR_NVS_NOT_INITIALIZED        (ESP_ERR_NVS_BASE + 0x01)
#define ESP_ERR_NVS_NOT_FOUND              (ESP_ERR_NVS_BASE + 0x02)
#define ESP_ERR_NVS_NOT_ENOUGH_SPACE       (ESP_ERR_NVS_BASE + 0x05)
#define ESP_ERR_NVS_INVALID_LENGTH         (ESP_ERR_NVS_BASE + 0x0c)
#define ESP_ERR_NVS_NO_FREE_PAGES          (ESP_ERR_NVS_BASE + 0x0d)
#define ESP_ERR_NVS_NEW_VERSION_FOUND      (ESP_ERR_NVS_BASE + 0x10)
#define ESP_ERR_NVS_CORRUPT_KEY_PART       (ESP_ERR_NVS_BASE + 0x17)
#define ESP_ERR_OTA_BASE                   0x1500
#define ESP_ERR_OTA_PARTITION_CONFLICT     (ESP_ERR_OTA_BASE + 0x01)
#define ESP_ERR_OTA_VALIDATE_FAILED        (ESP_ERR_OTA_BASE + 0x03)
#define ESP_ERR_OTA_ROLLBACK_INVALID_STATE (ESP_ERR_OTA_BASE + 0x05)
#define ESP_ERR_OTA_ROLLBACK_FAILED        (ESP_ERR_OTA_BASE + 0x08)
const char *esp_err_to_name(esp_err_t code);
void SimAbort(const char *why, const char *file, int line);
#define ESP_ERROR_CHECK(x)                                                                                       \
    do                                                                                                           \
    {                                                                                                            \
        esp_err_t _e = (x);                                                                                      \
        if (_e != ESP_OK)                                                                                        \
        {                                                                                                        \
            printf("ESP_ERROR_CHECK failed: %s (0x%x) at %s:%d\n", esp_err_to_name(_e), _e, __FILE__, __LINE__); \
            SimAbort("ESP_ERROR_CHECK", __FILE__, __LINE__);                                                     \
        }                                                                                                        \
    } while (0)
