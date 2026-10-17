cmake_minimum_required(VERSION 3.16)

# Set PROJECT_VER before project() so IDF does not run plain `git describe`
# (`--always --tags --dirty`), which follows Lovelace card-v* tags. version.cmake
# uses `git describe --tags --match fw-v* --dirty` with the fw-v prefix stripped,
# or version.txt plus the short hash when no fw tag is visible.
include(${CMAKE_CURRENT_LIST_DIR}/version.cmake)
if(NOT PROJECT_VER)
    message(FATAL_ERROR "PROJECT_VER was not set")
endif()

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(thermostat_v3)
