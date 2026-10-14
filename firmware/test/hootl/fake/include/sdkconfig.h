/* HOOTL simulation config. Mirrors sdkconfig.defaults + main/Kconfig defaults.
 * tools/hootl_check_config.py verifies the THERMO_* values match the production sdkconfig. */
#pragma once
#define CONFIG_IDF_TARGET_ESP32C6           1
#define CONFIG_FREERTOS_HZ                  1000
#define CONFIG_ESP_DEFAULT_CPU_FREQ_MHZ     160
#define CONFIG_PM_ENABLE                    0
#define CONFIG_THERMO_DISPLAY_FAHRENHEIT    1
#define CONFIG_THERMO_SELF_HEAT_OFFSET_C100 0
#define CONFIG_THERMO_SENSOR_PERIOD_MS      2000
#define CONFIG_THERMO_HW_WDT_ENABLE         1
#define CONFIG_THERMO_ZB_LONG_POLL_MS       3000
#define CONFIG_THERMO_OTA_MANUFACTURER      0x131B
#define CONFIG_THERMO_OTA_IMAGE_TYPE        0x0301
#define CONFIG_THERMO_HW_VERSION            3
#define CONFIG_ESP_TASK_WDT_TIMEOUT_S       5
