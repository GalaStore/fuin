# SPDX-License-Identifier: MIT
# Smoke tests for the fuin command line, run with `cmake -P` so they need nothing beyond CMake.
#   cmake -DFUIN=<fuin> -DMODE=version|context -DVERSION=<x.y.z> -DWORK=<scratch dir> -P smoke.cmake
cmake_minimum_required(VERSION 3.22)

foreach(var FUIN MODE VERSION WORK)
    if(NOT DEFINED ${var})
        message(FATAL_ERROR "smoke.cmake: -D${var}=... is required")
    endif()
endforeach()

# A throwaway HOME keeps the user's config out of the test, and the MCP roots sandbox defaults to ~,
# so the fixture has to live under it.
set(home "${WORK}/home")
file(REMOVE_RECURSE "${WORK}")
file(MAKE_DIRECTORY "${home}")
set(ENV{HOME} "${home}")
set(ENV{XDG_CONFIG_HOME} "${home}/.config")

set(fixture "${home}/fixture")
set(secret "FUIN_SMOKE_SECRET_7f3a")
file(WRITE "${fixture}/README.md" "# Fixture\n")
file(WRITE "${fixture}/src/main.py" "print('fuin')\n")
# Written here instead of committed because .env is gitignored.
file(WRITE "${fixture}/.env" "API_TOKEN=${secret}\n")

function(expect_contains what haystack needle)
    string(FIND "${haystack}" "${needle}" pos)
    if(pos EQUAL -1)
        message(FATAL_ERROR "${what}: expected to find '${needle}' in:\n${haystack}")
    endif()
endfunction()

function(expect_lacks what haystack needle)
    string(FIND "${haystack}" "${needle}" pos)
    if(NOT pos EQUAL -1)
        message(FATAL_ERROR "${what}: '${needle}' must not appear in:\n${haystack}")
    endif()
endfunction()

if(MODE STREQUAL "version")
    execute_process(COMMAND "${FUIN}" --version RESULT_VARIABLE rc OUTPUT_VARIABLE out ERROR_VARIABLE err)
    string(STRIP "${out}" out)
    if(NOT rc EQUAL 0 OR NOT out STREQUAL "fuin ${VERSION}")
        message(FATAL_ERROR "fuin --version: exit ${rc}, expected 'fuin ${VERSION}', got '${out}'\n${err}")
    endif()

elseif(MODE STREQUAL "context")
    foreach(format markdown xml)
        set(what "fuin --context --format ${format}")
        execute_process(COMMAND "${FUIN}" --context --format ${format} "${fixture}" RESULT_VARIABLE rc OUTPUT_VARIABLE out ERROR_VARIABLE err)
        if(NOT rc EQUAL 0)
            message(FATAL_ERROR "${what}: exit ${rc}\n${err}")
        endif()
        if(format STREQUAL "markdown")
            expect_contains("${what}" "${out}" "## README.md")
            expect_contains("${what}" "${out}" "## src/main.py")
        else()
            expect_contains("${what}" "${out}" "<source>README.md</source>")
            expect_contains("${what}" "${out}" "<source>src/main.py</source>")
        endif()
        expect_contains("${what}" "${out}" "# Fixture")
        expect_lacks("${what}" "${out}" ".env")
        expect_lacks("${what}" "${out}" "${secret}")
    endforeach()

else()
    message(FATAL_ERROR "smoke.cmake: unknown MODE '${MODE}'")
endif()
