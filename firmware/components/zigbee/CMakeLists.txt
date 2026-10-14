idf_component_register(SRCS "zb_thermostat.c" "zb_time.c"
                       INCLUDE_DIRS "include"
                       REQUIRES control display app_update esp_timer
                       PRIV_REQUIRES espressif__esp-zigbee-lib nvs_flash esp_partition)
