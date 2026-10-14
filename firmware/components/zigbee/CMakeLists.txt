idf_component_register(SRCS "zb_thermostat.c" "zb_time.c" "zb_retry.c"
                       INCLUDE_DIRS "include"
                       REQUIRES core control display app_update esp_timer
                       PRIV_REQUIRES espressif__esp-zigbee-lib nvs_flash esp_partition)
