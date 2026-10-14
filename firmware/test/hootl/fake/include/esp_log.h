/* HOOTL fake: esp_log.h. Every line carries the virtual time in ms. */
#pragma once
#include <stdint.h>
void sim_log(char level, const char *tag, const char *fmt, ...) __attribute__((format(printf, 3, 4)));
#define ESP_LOGE(tag, ...) sim_log('E', tag, __VA_ARGS__)
#define ESP_LOGW(tag, ...) sim_log('W', tag, __VA_ARGS__)
#define ESP_LOGI(tag, ...) sim_log('I', tag, __VA_ARGS__)
#define ESP_LOGD(tag, ...) do { if (0) sim_log('D', tag, __VA_ARGS__); } while (0)
#define ESP_LOGV(tag, ...) do { if (0) sim_log('V', tag, __VA_ARGS__); } while (0)
