# SPDX-License-Identifier: MPL-2.0
cmake_minimum_required(VERSION 3.22.1)
project(sigil-jni C)

# Build sigil's C library by including its top-level CMakeLists. SIGIL_ROOT
# defaults to ../../../.. so this AAR can sit inside the sigil repo at
# bindings/android/. Consumers vendoring sigil at a different path can
# override SIGIL_ROOT via -DSIGIL_ROOT=... in their externalNativeBuild
# arguments.
if(NOT DEFINED SIGIL_ROOT)
    get_filename_component(SIGIL_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/../../../../.. ABSOLUTE)
endif()

# Don't build sigil's CLI or tests when embedded as a subdirectory.
set(SIGIL_BUILD_CLI   OFF CACHE BOOL "" FORCE)
set(SIGIL_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(SIGIL_BUILD_SHARED OFF CACHE BOOL "" FORCE)
add_subdirectory(${SIGIL_ROOT} sigil-build)

add_library(sigil-jni SHARED sigil_jni.c)
target_link_libraries(sigil-jni PRIVATE sigil log)
target_include_directories(sigil-jni PRIVATE ${SIGIL_ROOT}/include)
