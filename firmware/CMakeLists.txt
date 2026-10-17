cmake_minimum_required(VERSION 3.16)

# Set PROJECT_VER before project() so IDF does not run plain `git describe`
# (`--always --tags --dirty`), which follows Lovelace card-v* tags.
# -DPROJECT_VER=... is left alone. See version.cmake.
include(${CMAKE_CURRENT_LIST_DIR}/version.cmake)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(thermostat_v3)
