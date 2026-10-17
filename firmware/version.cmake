# Firmware app version (PROJECT_VER / esp_app_desc), independent of Lovelace card tags.
#
# ESP-IDF otherwise runs `git describe --always --tags --dirty`, which follows the
# nearest tag of any kind. card-v* tags then become the boot-log "App version"
# (for example card-v1.1.1-20-g9511c9e).
#
# Preferred source: `git describe --tags --match fw-v* --dirty`, with the fw-v
# prefix stripped. No fw-v* tag (or a shallow checkout that cannot see one):
# firmware/version.txt plus the short hash. version.txt is also what IDF would
# read on its own if PROJECT_VER were unset, so it must stay a single X.Y.Z line
# with no comment. That X.Y.Z must match FW_VERSION_* so the Zigbee OTA file
# version (0xMMmmpppp) stays consistent with the string. The OTA encoding itself
# is not derived from git tags.
#
# Included from the top-level CMakeLists before project(), and runnable on its
# own: cmake -P version.cmake

set(_fw_src "${CMAKE_CURRENT_LIST_DIR}")
set(_fw_hdr "${_fw_src}/components/board/include/fw_version.h")
set(_fw_txt "${_fw_src}/version.txt")

function(_fw_macro_number path macro outvar)
    file(READ "${path}" _text)
    if(NOT _text MATCHES "#[ \t]*define[ \t]+${macro}[ \t]+([0-9]+)")
        message(FATAL_ERROR "fw version: ${macro} not found in ${path}")
    endif()
    set(${outvar} "${CMAKE_MATCH_1}" PARENT_SCOPE)
endfunction()

_fw_macro_number("${_fw_hdr}" FW_VERSION_MAJOR _fw_major)
_fw_macro_number("${_fw_hdr}" FW_VERSION_MINOR _fw_minor)
_fw_macro_number("${_fw_hdr}" FW_VERSION_PATCH _fw_patch)
set(_fw_semver "${_fw_major}.${_fw_minor}.${_fw_patch}")

if(NOT EXISTS "${_fw_txt}")
    message(FATAL_ERROR "fw version: ${_fw_txt} is missing")
endif()
file(STRINGS "${_fw_txt}" _fw_lines)
set(_fw_base "")
foreach(_line IN LISTS _fw_lines)
    string(STRIP "${_line}" _line)
    if(_line STREQUAL "" OR _line MATCHES "^#")
        continue()
    endif()
    set(_fw_base "${_line}")
    break()
endforeach()
if(NOT _fw_base STREQUAL "${_fw_semver}")
    message(FATAL_ERROR
        "fw version: version.txt is '${_fw_base}' but FW_VERSION_* is ${_fw_semver}. "
        "Keep them the same so the OTA file version matches the app version string.")
endif()

set(_fw_ver "")
set(_fw_from "version.txt")
find_package(Git QUIET)
if(GIT_FOUND)
    execute_process(
        COMMAND "${GIT_EXECUTABLE}" describe --tags --match fw-v* --dirty
        WORKING_DIRECTORY "${_fw_src}"
        RESULT_VARIABLE _desc_rc
        OUTPUT_VARIABLE _desc
        ERROR_VARIABLE _desc_err
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_STRIP_TRAILING_WHITESPACE)
    if(_desc_rc EQUAL 0 AND _desc MATCHES "^fw-v")
        string(REGEX REPLACE "^fw-v" "" _fw_ver "${_desc}")
        set(_fw_from "fw-v tag")
    elseif(NOT _desc_rc EQUAL 0 AND NOT _desc_err MATCHES "No names found|cannot describe|No tags can describe|no tag exactly matches")
        message(STATUS "fw version: git describe --match fw-v* failed (${_desc_rc}): ${_desc_err}")
    endif()
endif()

if(_fw_ver STREQUAL "")
    set(_fw_ver "${_fw_base}")
    if(GIT_FOUND)
        execute_process(
            COMMAND "${GIT_EXECUTABLE}" rev-parse --short=7 HEAD
            WORKING_DIRECTORY "${_fw_src}"
            RESULT_VARIABLE _hash_rc
            OUTPUT_VARIABLE _hash
            OUTPUT_STRIP_TRAILING_WHITESPACE
            ERROR_QUIET)
        if(_hash_rc EQUAL 0 AND NOT _hash STREQUAL "")
            set(_fw_ver "${_fw_base}-g${_hash}")
        endif()
        # Tracked changes only, same idea as `git describe --dirty` (untracked files do not count).
        execute_process(
            COMMAND "${GIT_EXECUTABLE}" status --porcelain --untracked-files=no
            WORKING_DIRECTORY "${_fw_src}"
            RESULT_VARIABLE _st_rc
            OUTPUT_VARIABLE _st
            OUTPUT_STRIP_TRAILING_WHITESPACE
            ERROR_QUIET)
        if(_st_rc EQUAL 0 AND NOT _st STREQUAL "")
            set(_fw_ver "${_fw_ver}-dirty")
        endif()
    endif()
endif()

if(_fw_ver STREQUAL "")
    message(FATAL_ERROR "fw version: could not determine PROJECT_VER")
endif()
if(_fw_ver MATCHES "card-")
    message(FATAL_ERROR "fw version: '${_fw_ver}' contains a Lovelace card tag")
endif()
# Dots are any-char in cmake regex; escape them so 0.30.0 cannot satisfy 0.3.0.
if(NOT _fw_ver MATCHES "^${_fw_major}\\.${_fw_minor}\\.${_fw_patch}($|[^0-9])")
    message(FATAL_ERROR
        "fw version: '${_fw_ver}' does not start with ${_fw_semver} from fw_version.h. "
        "Tag fw-v${_fw_semver}, or bump FW_VERSION_* and version.txt together.")
endif()
string(LENGTH "${_fw_ver}" _fw_len)
if(_fw_len GREATER 31)
    message(FATAL_ERROR
        "fw version: '${_fw_ver}' is ${_fw_len} characters; esp_app_desc.version holds 31")
endif()

set(PROJECT_VER "${_fw_ver}")

if(NOT CMAKE_SCRIPT_MODE_FILE)
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${_fw_txt}" "${_fw_hdr}")
    if(GIT_FOUND)
        execute_process(
            COMMAND "${GIT_EXECUTABLE}" rev-parse --absolute-git-dir
            WORKING_DIRECTORY "${_fw_src}"
            RESULT_VARIABLE _git_dir_rc
            OUTPUT_VARIABLE _git_dir
            OUTPUT_STRIP_TRAILING_WHITESPACE
            ERROR_QUIET)
        if(_git_dir_rc EQUAL 0 AND EXISTS "${_git_dir}/HEAD")
            set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${_git_dir}/HEAD")
        endif()
    endif()
    message(STATUS "App version: ${PROJECT_VER} (${_fw_from})")
else()
    execute_process(COMMAND "${CMAKE_COMMAND}" -E echo "${PROJECT_VER}")
endif()
