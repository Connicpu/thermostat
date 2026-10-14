idf_component_register(SRCS "gfx.c" "font_data.c" "display_render.c" "ssd1680.c" "display.c"
                       INCLUDE_DIRS "include"
                       REQUIRES core control board esp_driver_spi esp_driver_gpio esp_timer)
# Panel mounting variant: idf.py -DEPD_ROTATION=3 build (see gfx.c). Unset = gfx.c default (1).
if(DEFINED EPD_ROTATION)
    target_compile_definitions(${COMPONENT_LIB} PRIVATE EPD_ROTATION=${EPD_ROTATION})
endif()
