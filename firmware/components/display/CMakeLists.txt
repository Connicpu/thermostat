idf_component_register(SRCS "gfx.c" "font_data.c" "display_render.c" "ssd1680.c" "display.c"
                       INCLUDE_DIRS "include"
                       REQUIRES core control board esp_driver_spi esp_driver_gpio esp_timer)
