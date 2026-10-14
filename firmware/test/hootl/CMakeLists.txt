# HOOTL (hardware-out-of-the-loop) simulator: the real firmware sources (main/ +
# components/, unchanged) built for the host against fake ESP-IDF / FreeRTOS /
# esp-zigbee-lib *implementations*. The real esp-zigbee-lib headers are used, so
# zb_thermostat.c compiles exactly as on the chip. See README "HOOTL simulation".
cmake_minimum_required(VERSION 3.16)
project(thermostat_hootl C)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)   # used by tools/check_naming.py
set(CMAKE_C_STANDARD 11)
set(ROOT ${CMAKE_CURRENT_SOURCE_DIR}/../..)
set(FW ${ROOT}/components)
set(ZBLIB ${ROOT}/managed_components/espressif__esp-zigbee-lib/include)
set(ZBOSS ${ROOT}/managed_components/espressif__esp-zboss-lib/include)
if(NOT EXISTS ${ZBLIB}/esp_zigbee_core.h)
  message(FATAL_ERROR "esp-zigbee-lib headers missing: run 'idf.py reconfigure' once (or the CI step) to fetch managed_components")
endif()

file(GLOB FW_SRCS
  ${ROOT}/main/*.c
  ${FW}/control/*.c ${FW}/hvac_outputs/*.c ${FW}/sensor/*.c ${FW}/display/*.c
  ${FW}/ui/*.c ${FW}/board/*.c ${FW}/zigbee/*.c)
add_executable(tstat_sim ${FW_SRCS}
  fake/kernel.c fake/sim_main.c fake/periph.c fake/esp_zb_fake.c)
target_include_directories(tstat_sim PRIVATE
  fake/include fake
  ${ROOT}/main
  ${FW}/core/include ${FW}/control/include ${FW}/hvac_outputs/include ${FW}/sensor/include ${FW}/display/include
  ${FW}/ui/include ${FW}/board/include ${FW}/zigbee/include ${FW}/display
  ${ZBLIB} ${ZBOSS})
# firmware code: the same warning bar as the chip build; fake code: strict too
target_compile_options(tstat_sim PRIVATE -Wall -Wextra -Werror -Wno-unused-parameter -Wno-missing-field-initializers
  -include freertos/FreeRTOS.h -g -O1 -fsanitize=address,undefined -fno-omit-frame-pointer)
target_link_options(tstat_sim PRIVATE -fsanitize=address,undefined)
target_link_libraries(tstat_sim PRIVATE pthread m)
