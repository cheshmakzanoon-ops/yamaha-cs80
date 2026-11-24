# CI-only post-configure check. Do not include this in CMakeLists.txt: local
# builds may intentionally use deps/juce or FETCHCONTENT_SOURCE_DIR_JUCE.
cmake_minimum_required(VERSION 3.22)

foreach(required IN ITEMS BUILD_DIR EXPECTED_JUCE_SHA EXPECTED_JUCE_VERSION)
    if(NOT DEFINED ${required} OR "${${required}}" STREQUAL "")
        message(FATAL_ERROR "CI JUCE verification requires -D${required}=...")
    endif()
endforeach()

get_filename_component(build_dir "${BUILD_DIR}" ABSOLUTE)
set(expected_source "${build_dir}/_deps/juce-src")
set(report "${build_dir}/ci-juce-provenance.txt")
file(MAKE_DIRECTORY "${build_dir}")
string(TIMESTAMP checked_at "%Y-%m-%dT%H:%M:%SZ" UTC)
file(WRITE "${report}"
    "checked_at=${checked_at}\n"
    "expected_sha=${EXPECTED_JUCE_SHA}\n"
    "expected_version=${EXPECTED_JUCE_VERSION}\n"
    "expected_source=${expected_source}\n"
    "workflow_commit=$ENV{GITHUB_SHA}\n"
    "workflow_run=$ENV{GITHUB_RUN_ID}\n")

function(fail reason)
    file(APPEND "${report}" "verification=FAIL\nreason=${reason}\n")
    message(FATAL_ERROR "CI JUCE verification: ${reason}\nEvidence: ${report}")
endfunction()

if(NOT EXISTS "${build_dir}/CMakeCache.txt")
    fail("CMakeCache.txt is missing; run this check after a successful configure")
endif()
load_cache("${build_dir}" READ_WITH_PREFIX configured_
    JUCE_SOURCE_DIR FETCHCONTENT_SOURCE_DIR_JUCE CMAKE_HOME_DIRECTORY)
file(APPEND "${report}"
    "configured_source=${configured_JUCE_SOURCE_DIR}\n"
    "source_override=${configured_FETCHCONTENT_SOURCE_DIR_JUCE}\n"
    "project_source=${configured_CMAKE_HOME_DIRECTORY}\n")
if(NOT "${configured_FETCHCONTENT_SOURCE_DIR_JUCE}" STREQUAL "")
    fail("FETCHCONTENT_SOURCE_DIR_JUCE overrides are not allowed in CI")
endif()
if(NOT IS_DIRECTORY "${configured_JUCE_SOURCE_DIR}")
    fail("Configured JUCE_SOURCE_DIR is missing or is not a directory")
endif()
file(REAL_PATH "${configured_JUCE_SOURCE_DIR}" actual_source)
file(REAL_PATH "${expected_source}" expected_source)
if(NOT actual_source STREQUAL expected_source)
    fail("CI must build the fetched _deps/juce-src checkout, not a source override")
endif()
# Without this guard, git -C can silently inspect the enclosing project repo.
if(NOT EXISTS "${actual_source}/.git")
    fail("The configured JUCE source has no .git metadata; CI requires a verifiable checkout")
endif()
find_program(git_executable NAMES git)
if(NOT git_executable)
    fail("Git is unavailable; cannot verify the configured JUCE checkout")
endif()

function(read_git output)
    execute_process(COMMAND "${git_executable}" -C "${actual_source}" ${ARGN}
        RESULT_VARIABLE result OUTPUT_VARIABLE value ERROR_VARIABLE error
        OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_STRIP_TRAILING_WHITESPACE)
    if(NOT result EQUAL 0)
        fail("Git inspection failed (${ARGN}): ${error}")
    endif()
    set(${output} "${value}" PARENT_SCOPE)
endfunction()

read_git(git_root rev-parse --show-toplevel)
file(REAL_PATH "${git_root}" git_root)
if(NOT git_root STREQUAL actual_source)
    fail("JUCE_SOURCE_DIR is not the root of its own Git checkout")
endif()
read_git(actual_sha rev-parse HEAD)
read_git(origin config --get remote.origin.url)
read_git(description describe --tags --always)
read_git(changes status --porcelain=v1 --untracked-files=all)
file(APPEND "${report}"
    "actual_sha=${actual_sha}\n"
    "origin=${origin}\n"
    "git_description=${description}\n")
if(changes STREQUAL "")
    file(APPEND "${report}" "source_clean=true\n")
else()
    file(APPEND "${report}" "source_clean=false\nsource_changes:\n${changes}\n")
endif()

if(NOT EXISTS "${actual_source}/CMakeLists.txt")
    fail("JUCE CMakeLists.txt is missing")
endif()
file(STRINGS "${actual_source}/CMakeLists.txt" version_line
    REGEX "^project\\(JUCE VERSION [0-9]+\\.[0-9]+\\.[0-9]+")
if(NOT version_line MATCHES "^project\\(JUCE VERSION ([0-9]+\\.[0-9]+\\.[0-9]+)")
    fail("Cannot read the JUCE project version")
endif()
set(actual_version "${CMAKE_MATCH_1}")
file(APPEND "${report}" "actual_version=${actual_version}\n")

if(NOT actual_sha STREQUAL EXPECTED_JUCE_SHA)
    fail("JUCE HEAD ${actual_sha} does not match expected ${EXPECTED_JUCE_SHA}")
endif()
if(NOT changes STREQUAL "")
    fail("The configured JUCE checkout is dirty")
endif()
if(NOT actual_version STREQUAL EXPECTED_JUCE_VERSION)
    fail("JUCE version ${actual_version} does not match expected ${EXPECTED_JUCE_VERSION}")
endif()
file(APPEND "${report}" "verification=PASS\n")
message(STATUS "Verified clean JUCE ${actual_version} at ${actual_sha}; evidence: ${report}")
