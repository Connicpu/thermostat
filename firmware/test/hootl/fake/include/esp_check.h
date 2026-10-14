#pragma once
#include "esp_err.h"
#include "esp_log.h"
#define ESP_RETURN_ON_FALSE(a, err, tag, ...) do { if (!(a)) { ESP_LOGE(tag, __VA_ARGS__); return (err); } } while (0)
#define ESP_RETURN_ON_ERROR(x, tag, ...) do { esp_err_t _e = (x); if (_e != ESP_OK) { ESP_LOGE(tag, __VA_ARGS__); return _e; } } while (0)
