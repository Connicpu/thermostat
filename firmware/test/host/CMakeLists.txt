cmake_minimum_required(VERSION 3.16)
project(thermostat_host_tests C)
set(CMAKE_C_STANDARD 11)
set(FW ${CMAKE_CURRENT_SOURCE_DIR}/../../components)
add_compile_options(-Wall -Wextra -Werror -fsanitize=address,undefined -g)
add_link_options(-fsanitize=address,undefined)
include_directories(${FW}/control/include ${FW}/hvac_outputs/include ${FW}/sensor/include
                    ${FW}/ui/include ${FW}/display/include .)
add_executable(test_control test_control.c
    ${FW}/control/control.c ${FW}/hvac_outputs/hvac_interlock.c)
add_executable(test_misc test_misc.c
    ${FW}/sensor/sht40_parse.c ${FW}/ui/buttons.c
    ${FW}/display/gfx.c ${FW}/display/font_data.c ${FW}/display/display_render.c ${FW}/control/control.c)
add_executable(test_menu test_menu.c
    ${FW}/ui/menu.c ${FW}/ui/buttons.c
    ${FW}/display/gfx.c ${FW}/display/font_data.c ${FW}/display/display_render.c ${FW}/control/control.c)
enable_testing()
add_test(NAME control COMMAND test_control)
add_test(NAME misc COMMAND test_misc)
add_test(NAME menu COMMAND test_menu)
