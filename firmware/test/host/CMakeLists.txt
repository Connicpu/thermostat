cmake_minimum_required(VERSION 3.16)
project(thermostat_host_tests C)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)   # used by tools/check_naming.py
set(CMAKE_C_STANDARD 11)
set(FW ${CMAKE_CURRENT_SOURCE_DIR}/../../components)
add_compile_options(-Wall -Wextra -Werror -fsanitize=address,undefined -g)
add_link_options(-fsanitize=address,undefined)
include_directories(${FW}/core/include ${FW}/control/include ${FW}/hvac_outputs/include ${FW}/sensor/include
                    ${FW}/ui/include ${FW}/display/include ${FW}/zigbee/include
                    ${CMAKE_CURRENT_SOURCE_DIR}/../../main .)
add_executable(test_control test_control.c
    ${FW}/control/control.c ${FW}/hvac_outputs/hvac_interlock.c)
add_executable(test_sensor test_sensor.c
    ${FW}/sensor/sensor_cycle.c ${FW}/sensor/sht40_parse.c)
add_executable(test_misc test_misc.c
    ${FW}/sensor/sht40_parse.c ${FW}/sensor/sensor_stuck.c ${FW}/ui/buttons.c
    ${FW}/display/gfx.c ${FW}/display/font_data.c ${FW}/display/display_render.c
    ${FW}/display/display_policy.c ${FW}/control/control.c)
add_executable(test_menu test_menu.c
    ${FW}/ui/menu.c ${FW}/control/schedule.c ${FW}/ui/buttons.c
    ${FW}/display/gfx.c ${FW}/display/font_data.c ${FW}/display/display_render.c ${FW}/control/control.c)
add_executable(test_schedule test_schedule.c
    ${FW}/control/schedule.c ${FW}/control/control.c)
add_executable(test_settings test_settings.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../main/settings_codec.c ${FW}/control/schedule.c ${FW}/control/control.c)
add_executable(test_zb_time test_zb_time.c ${FW}/zigbee/zb_time.c)
add_executable(test_zb_retry test_zb_retry.c ${FW}/zigbee/zb_retry.c)
enable_testing()
add_test(NAME control COMMAND test_control)
add_test(NAME sensor COMMAND test_sensor)
add_test(NAME misc COMMAND test_misc)
add_test(NAME menu COMMAND test_menu)
add_test(NAME schedule COMMAND test_schedule)
add_test(NAME settings COMMAND test_settings)
add_test(NAME zb_time COMMAND test_zb_time)
add_test(NAME zb_retry COMMAND test_zb_retry)
