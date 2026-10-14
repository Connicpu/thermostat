/* HOOTL fake OTA: two slots + otadata state machine persisted across sim reboots (fake/periph.c). */
#pragma once
#include <stdint.h>
#include <stddef.h>
#include "esp_err.h"
#include "esp_partition.h"
typedef uint32_t esp_ota_handle_t;
typedef enum { ESP_OTA_IMG_NEW = 0, ESP_OTA_IMG_PENDING_VERIFY = 1, ESP_OTA_IMG_VALID = 2, ESP_OTA_IMG_INVALID = 3,
               ESP_OTA_IMG_ABORTED = 4, ESP_OTA_IMG_UNDEFINED = -1 } esp_ota_img_states_t;
#define OTA_SIZE_UNKNOWN 0xffffffff
#define OTA_WITH_SEQUENTIAL_WRITES 0xfffffffe
const esp_partition_t *esp_ota_get_running_partition(void);
const esp_partition_t *esp_ota_get_next_update_partition(const esp_partition_t *start);
esp_err_t esp_ota_get_state_partition(const esp_partition_t *p, esp_ota_img_states_t *st);
esp_err_t esp_ota_mark_app_valid_cancel_rollback(void);
bool esp_ota_check_rollback_is_possible(void);
esp_err_t esp_ota_mark_app_invalid_rollback_and_reboot(void);
esp_err_t esp_ota_begin(const esp_partition_t *p, size_t size, esp_ota_handle_t *out);
esp_err_t esp_ota_write(esp_ota_handle_t h, const void *d, size_t n);
esp_err_t esp_ota_end(esp_ota_handle_t h);
esp_err_t esp_ota_abort(esp_ota_handle_t h);
esp_err_t esp_ota_set_boot_partition(const esp_partition_t *p);
