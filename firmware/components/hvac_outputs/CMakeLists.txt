idf_component_register(SRCS "hvac_interlock.c" "hvac_outputs.c"
                       INCLUDE_DIRS "include"
                       REQUIRES core esp_driver_gpio esp_timer board)
