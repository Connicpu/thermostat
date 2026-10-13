idf_component_register(SRCS "hvac_interlock.c" "hvac_outputs.c"
                       INCLUDE_DIRS "include"
                       REQUIRES esp_driver_gpio board)
