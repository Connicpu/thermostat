idf_component_register(SRCS "main.c" "settings.c"
                       INCLUDE_DIRS "."
                       PRIV_REQUIRES control hvac_outputs sensor display ui zigbee board
                                     nvs_flash esp_timer esp_pm app_update esp_system)
