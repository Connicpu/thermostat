/* HOOTL fake: esp_log.h. Every line carries the virtual time in ms. */
#pragma once
#include <stdint.h>
void SimLog(char level, const char *tag, const char *fmt, ...) __attribute__((format(printf, 3, 4)));
#define ESP_LOGE(tag, ...) SimLog('E', tag, __VA_ARGS__)
#define ESP_LOGW(tag, ...) SimLog('W', tag, __VA_ARGS__)
#define ESP_LOGI(tag, ...) SimLog('I', tag, __VA_ARGS__)
#define ESP_LOGD(tag, ...)                 \
    do                                     \
    {                                      \
        if (0)                             \
            SimLog('D', tag, __VA_ARGS__); \
    } while (0)
#define ESP_LOGV(tag, ...)                 \
    do                                     \
    {                                      \
        if (0)                             \
            SimLog('V', tag, __VA_ARGS__); \
    } while (0)
