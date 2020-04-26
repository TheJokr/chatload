# Input: SRC, DST, BUILD_VERSION (fallback value), CACHE_FILE (see below)
# Configures SRC to DST with the following variables:
#  - BUILD_VERSION: version string (see below)
#  - GIT_SHA1: full commit hash
#  - BUILD_TIME: build timestamp (only changed if any of the other variables change)

find_package(Git)
if (GIT_FOUND)
    execute_process(COMMAND "${GIT_EXECUTABLE}" "rev-parse" "--verify" "HEAD" TIMEOUT 2 ERROR_QUIET
                    RESULT_VARIABLE GIT_SHA1_RES OUTPUT_VARIABLE GIT_SHA1 OUTPUT_STRIP_TRAILING_WHITESPACE)
    execute_process(COMMAND "${GIT_EXECUTABLE}" "rev-parse" "--short=8" "HEAD" TIMEOUT 2 ERROR_QUIET
                    RESULT_VARIABLE GIT_SHORT_RES OUTPUT_VARIABLE GIT_SHORT OUTPUT_STRIP_TRAILING_WHITESPACE)
    execute_process(COMMAND "${GIT_EXECUTABLE}" "describe" "--tags" "--exact-match" "HEAD" TIMEOUT 2
                    RESULT_VARIABLE GIT_TAG_RES OUTPUT_VARIABLE GIT_TAG OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
endif()

# Fallback version string is passed in from CMakeLists.txt.
# Overwrite with tag if present, otherwise add "-dev" suffix
# and, if git command was successful, short commit hash.
if (GIT_TAG_RES EQUAL "0")
    set(BUILD_VERSION "${GIT_TAG}")
else()
    string(APPEND BUILD_VERSION "-dev")

    if (GIT_SHORT_RES EQUAL "0")
        string(APPEND BUILD_VERSION "+${GIT_SHORT}")
    endif()
endif()

# Check values against cache to avoid rebuilds due to new timestamp
# NOTE: this means timestamp won't change unless changes are commited!
if (DEFINED CACHE_FILE)
    set(CACHE_VALUE "${BUILD_VERSION}\n${GIT_SHA1}\n")

    # Return early if output exists and cache is up-to-date
    if (EXISTS "${DST}" AND EXISTS "${CACHE_FILE}")
        file(READ "${CACHE_FILE}" CACHE_CONTENT)
        if (CACHE_CONTENT STREQUAL CACHE_VALUE)
            return()
        endif()
    endif()

    # Cache is outdated, overwrite it
    file(WRITE "${CACHE_FILE}" "${CACHE_VALUE}")
endif()

string(TIMESTAMP BUILD_TIME "%Y-%m-%d %H:%M:%S UTC" UTC)
configure_file("${SRC}" "${DST}" @ONLY)
