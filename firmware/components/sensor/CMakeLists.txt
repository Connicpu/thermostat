idf_component_register(SRCS "sht40_parse.c" "sensor_stuck.c" "sensor_cycle.c" "sensor.c" INCLUDE_DIRS "include" REQUIRES core esp_driver_i2c board)
