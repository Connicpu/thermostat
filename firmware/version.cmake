# Firmware app version (PROJECT_VER / esp_app_desc), independent of Lovelace card tags.
#
# ESP-IDF otherwise runs `git describe --always --tags --dirty`, which follows the
# nearest tag of any kind. card-v* tags then become the boot-log "App version"
# (for example card-v1.1.1-20-g9511c9e).
#
# -DPROJECT_VER=... is kept as given (still rejected if it contains card- or
# exceeds 31 characters). An empty -DPROJECT_VER= is not an override: it falls
# through to the rules below, which is how `idf.py -DPROJECT_VER= reconfigure`
# clears a cached one. `idf.py` has no -U; `cmake -UPROJECT_VER <build>` also
# clears it.
#
# Otherwise, in order:
#   1. `git describe --tags --match fw-v* --dirty`, fw-v prefix stripped, when the
#      tag is fw-vX.Y.Z or fw-vX.Y.Z-rcN and that X.Y.Z matches FW_VERSION_*.
#      A release tag stays X.Y.Z. An rc tag stays X.Y.Z-rcN, and commits after it
#      are X.Y.Z-rcN-N-g<hash>. Any other fw-v tag (fw-v0.3.1b, fw-v0.3.1-beta)
#      is a WARNING and is ignored. A matching-form tag on HEAD whose X.Y.Z does
#      not match the header is a fatal error. A nearer-but-older tag is ignored.
#   2. firmware/version.txt plus -g and the short hash (and -dirty if the worktree
#      has tracked changes). version.txt is one X.Y.Z line, the same numbers as
#      FW_VERSION_*, so the OTA file version (0xMMmmpppp) stays consistent.
#   3. version.txt plus -nogit when git cannot be used (no git, a tarball, a
#      parent repo, dubious ownership). That is a WARNING, and a FATAL_ERROR when
#      $CI is set, so a tagged release is never confused with a git-less build.
#
# Included from the top-level CMakeLists before project().

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

# Leading X.Y.Z of a version string, or empty if it has none.
function(_fw_leading_semver ver out)
    if("${ver}" MATCHES "^([0-9]+)\\.([0-9]+)\\.([0-9]+)($|[^0-9])")
        set(${out} "${CMAKE_MATCH_1}.${CMAKE_MATCH_2}.${CMAKE_MATCH_3}" PARENT_SCOPE)
    else()
        set(${out} "" PARENT_SCOPE)
    endif()
endfunction()

function(_fw_reject_bad ver)
    if("${ver}" MATCHES "card-")
        message(FATAL_ERROR "fw version: '${ver}' contains a Lovelace card tag")
    endif()
    string(LENGTH "${ver}" _len)
    if(_len GREATER 31)
        message(FATAL_ERROR "fw version: '${ver}' is ${_len} characters; esp_app_desc.version holds 31")
    endif()
endfunction()

# The CI image check accepts the same two tag shapes. fw-v0.3.1b and fw-v0.3.1-beta do not qualify.
function(_fw_accepted_fw_tag tag out)
    if("${tag}" MATCHES "^fw-v[0-9]+\\.[0-9]+\\.[0-9]+(-rc[0-9]+)?$")
        set(${out} TRUE PARENT_SCOPE)
    else()
        set(${out} FALSE PARENT_SCOPE)
    endif()
endfunction()

# -DPROJECT_VER=... arrives as a cache variable. A later set() would shadow it.
if(DEFINED PROJECT_VER AND NOT "${PROJECT_VER}" STREQUAL "")
    _fw_reject_bad("${PROJECT_VER}")
    message(STATUS "App version: ${PROJECT_VER} (PROJECT_VER override)")
    return()
endif()

_fw_macro_number("${_fw_hdr}" FW_VERSION_MAJOR _fw_major)
_fw_macro_number("${_fw_hdr}" FW_VERSION_MINOR _fw_minor)
_fw_macro_number("${_fw_hdr}" FW_VERSION_PATCH _fw_patch)
set(_fw_semver "${_fw_major}.${_fw_minor}.${_fw_patch}")

if(NOT EXISTS "${_fw_txt}")
    message(FATAL_ERROR "fw version: ${_fw_txt} is missing")
endif()
file(READ "${_fw_txt}" _fw_base)
string(STRIP "${_fw_base}" _fw_base)
if(NOT _fw_base STREQUAL "${_fw_semver}")
    message(FATAL_ERROR
        "fw version: version.txt is '${_fw_base}' but FW_VERSION_* is ${_fw_semver}. "
        "Keep them the same so the OTA file version matches the app version string.")
endif()

set(_fw_ver "")
set(_fw_from "version.txt")
set(_fw_git_ok FALSE)
set(_fw_git_err "")
set(_sym_ref "")

find_package(Git QUIET)
if(NOT GIT_FOUND)
    set(_fw_git_err "git was not found")
else()
    execute_process(
        COMMAND "${GIT_EXECUTABLE}" rev-parse --show-toplevel
        WORKING_DIRECTORY "${_fw_src}"
        RESULT_VARIABLE _top_rc
        OUTPUT_VARIABLE _top
        ERROR_VARIABLE _top_err
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_STRIP_TRAILING_WHITESPACE)
    if(NOT _top_rc EQUAL 0)
        set(_fw_git_err "${_top_err}")
    else()
        get_filename_component(_src_real "${_fw_src}" REALPATH)
        get_filename_component(_parent_real "${_fw_src}/.." REALPATH)
        if(_top STREQUAL "${_src_real}" OR _top STREQUAL "${_parent_real}")
            set(_fw_git_ok TRUE)
        else()
            set(_fw_git_err "git toplevel ${_top} is not this repo (${_parent_real})")
        endif()
    endif()
endif()

if(_fw_git_ok)
    execute_process(
        COMMAND "${GIT_EXECUTABLE}" describe --tags --exact-match --match fw-v*
        WORKING_DIRECTORY "${_fw_src}"
        RESULT_VARIABLE _exact_rc
        OUTPUT_VARIABLE _exact
        ERROR_VARIABLE _exact_err
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_STRIP_TRAILING_WHITESPACE)
    execute_process(
        COMMAND "${GIT_EXECUTABLE}" describe --tags --match fw-v* --dirty
        WORKING_DIRECTORY "${_fw_src}"
        RESULT_VARIABLE _desc_rc
        OUTPUT_VARIABLE _desc
        ERROR_VARIABLE _desc_err
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_STRIP_TRAILING_WHITESPACE)
    execute_process(
        COMMAND "${GIT_EXECUTABLE}" describe --tags --abbrev=0 --match fw-v*
        WORKING_DIRECTORY "${_fw_src}"
        RESULT_VARIABLE _tag_rc
        OUTPUT_VARIABLE _tag
        ERROR_VARIABLE _tag_err
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_STRIP_TRAILING_WHITESPACE)
    set(_fw_tag "")
    set(_fw_tag_ok FALSE)
    if(_tag_rc EQUAL 0 AND _tag MATCHES "^fw-v")
        set(_fw_tag "${_tag}")
        _fw_accepted_fw_tag("${_fw_tag}" _fw_tag_ok)
    endif()
    if(NOT _fw_tag STREQUAL "" AND NOT _fw_tag_ok)
        message(WARNING
            "fw version: ignoring ${_fw_tag}; only fw-vX.Y.Z and fw-vX.Y.Z-rcN are used. Using version.txt plus the short hash")
    elseif(_fw_tag_ok AND _exact_rc EQUAL 0 AND _exact MATCHES "^fw-v")
        string(REGEX REPLACE "^fw-v" "" _exact_ver "${_exact}")
        _fw_leading_semver("${_exact_ver}" _exact_semver)
        if(NOT _exact_semver STREQUAL "${_fw_semver}")
            message(FATAL_ERROR
                "fw version: HEAD is tagged fw-v${_exact_ver} but FW_VERSION_* is ${_fw_semver}. "
                "Move the tag, or bump version.txt and fw_version.h together.")
        endif()
        if(_desc_rc EQUAL 0 AND _desc MATCHES "^fw-v")
            string(REGEX REPLACE "^fw-v" "" _fw_ver "${_desc}")
        else()
            set(_fw_ver "${_exact_ver}")
        endif()
        set(_fw_from "fw-v tag")
    elseif(_fw_tag_ok AND _desc_rc EQUAL 0 AND _desc MATCHES "^fw-v")
        string(REGEX REPLACE "^fw-v" "" _desc_ver "${_desc}")
        _fw_leading_semver("${_desc_ver}" _desc_semver)
        if(_desc_semver STREQUAL "${_fw_semver}")
            set(_fw_ver "${_desc_ver}")
            set(_fw_from "fw-v tag")
        else()
            message(STATUS
                "fw version: nearest tag is ${_desc_semver}, FW_VERSION_* is ${_fw_semver}; using version.txt plus the short hash")
        endif()
    elseif(NOT _desc_rc EQUAL 0 AND NOT _desc_err MATCHES "No names found|cannot describe|No tags can describe|no tag exactly matches")
        # A missing fw-v tag is normal. Anything else (ownership, a shallow clone
        # that cannot walk to a tag) still falls through to the hash when git itself works.
        message(STATUS "fw version: git describe --match fw-v* failed (${_desc_rc}): ${_desc_err}")
    endif()
endif()

if(_fw_ver STREQUAL "" AND _fw_git_ok)
    execute_process(
        COMMAND "${GIT_EXECUTABLE}" rev-parse --short=7 HEAD
        WORKING_DIRECTORY "${_fw_src}"
        RESULT_VARIABLE _hash_rc
        OUTPUT_VARIABLE _hash
        ERROR_VARIABLE _hash_err
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_STRIP_TRAILING_WHITESPACE)
    if(_hash_rc EQUAL 0 AND NOT _hash STREQUAL "")
        set(_fw_ver "${_fw_base}-g${_hash}")
        # Tracked changes only, same idea as `git describe --dirty`.
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
    else()
        set(_fw_git_ok FALSE)
        set(_fw_git_err "${_hash_err}")
    endif()
endif()

if(_fw_ver STREQUAL "")
    set(_fw_ver "${_fw_base}-nogit")
    set(_fw_from "no git")
    message(WARNING "fw version: no usable git data (${_fw_git_err}); App version is ${_fw_ver}")
    if(NOT "$ENV{CI}" STREQUAL "")
        message(FATAL_ERROR "fw version: CI requires a git version, not ${_fw_ver} (${_fw_git_err})")
    endif()
endif()

_fw_reject_bad("${_fw_ver}")
# Dots are any-char in cmake regex; escape them so 0.30.0 cannot satisfy 0.3.0.
if(NOT _fw_ver MATCHES "^${_fw_major}\\.${_fw_minor}\\.${_fw_patch}($|[^0-9])")
    message(FATAL_ERROR
        "fw version: '${_fw_ver}' does not start with ${_fw_semver} from fw_version.h. "
        "Tag fw-v${_fw_semver}, or bump FW_VERSION_* and version.txt together.")
endif()

set(PROJECT_VER "${_fw_ver}")

set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${_fw_txt}" "${_fw_hdr}")
if(_fw_git_ok)
    # HEAD changes on a branch switch, but stays "ref: refs/heads/<branch>" across
    # commits, so the branch file is what a new commit updates. packed-refs covers
    # a packed branch, and logs/HEAD covers the reflog. HEAD itself is watched so
    # a checkout still reconfigures when the reflog is off. --git-path resolves
    # all four in this checkout or a worktree.
    execute_process(
        COMMAND "${GIT_EXECUTABLE}" rev-parse --symbolic-full-name HEAD
        WORKING_DIRECTORY "${_fw_src}"
        RESULT_VARIABLE _sym_rc
        OUTPUT_VARIABLE _sym_ref
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET)
    set(_fw_git_names "HEAD" "packed-refs" "logs/HEAD")
    if(_sym_rc EQUAL 0 AND _sym_ref MATCHES "^refs/")
        list(APPEND _fw_git_names "${_sym_ref}")
    endif()
    foreach(_fw_git_name IN LISTS _fw_git_names)
        execute_process(
            COMMAND "${GIT_EXECUTABLE}" rev-parse --git-path "${_fw_git_name}"
            WORKING_DIRECTORY "${_fw_src}"
            RESULT_VARIABLE _gp_rc
            OUTPUT_VARIABLE _gp
            OUTPUT_STRIP_TRAILING_WHITESPACE
            ERROR_QUIET)
        if(_gp_rc EQUAL 0 AND NOT _gp STREQUAL "")
            if(IS_ABSOLUTE "${_gp}")
                set(_gp_abs "${_gp}")
            else()
                get_filename_component(_gp_abs "${_fw_src}/${_gp}" ABSOLUTE)
            endif()
            if(EXISTS "${_gp_abs}")
                set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${_gp_abs}")
            endif()
        endif()
    endforeach()
endif()
message(STATUS "App version: ${PROJECT_VER} (${_fw_from})")
