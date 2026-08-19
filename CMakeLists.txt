cmake_minimum_required(VERSION 3.22.1)

project(campello_widgets VERSION 0.7.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ---------------------------------------------------------------------------
# Options
# ---------------------------------------------------------------------------
option(BUILD_TESTS            "Build universal tests (no GPU required)"       OFF)
option(BUILD_INTEGRATION_TESTS "Build platform-specific integration tests"   OFF)
option(BUILD_EXAMPLES         "Build example applications"                    OFF)
option(ENABLE_UNITY_BUILD     "Enable Unity Build for faster compilation"     ON)
option(CAMPELLO_GPU_VALIDATION "Enable Vulkan validation layers (debug only)" OFF)
option(BUILD_MATERIAL  "Build the campello_material design system library"  ON)
option(BUILD_CUPERTINO "Build the campello_cupertino design system library" ON)
option(BUILD_UI        "Build the campello_ui design system library"       ON)
option(BUILD_FLUENT    "Build the campello_fluent design system library"   ON)

if(BUILD_INTEGRATION_TESTS)
    set(BUILD_TESTS ON)
endif()

# ---------------------------------------------------------------------------
# Unity Build configuration
# ---------------------------------------------------------------------------
if(ENABLE_UNITY_BUILD)
    set(CMAKE_UNITY_BUILD ON)
    set(CMAKE_UNITY_BUILD_BATCH_SIZE 16)
    message(STATUS "Unity Build enabled (batch size: ${CMAKE_UNITY_BUILD_BATCH_SIZE})")
endif()

# ---------------------------------------------------------------------------
# Configuration header
# ---------------------------------------------------------------------------
configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/src/campello_widgets_config.h.in
    ${CMAKE_CURRENT_BINARY_DIR}/campello_widgets_config.h
)

# ---------------------------------------------------------------------------
# Dependencies
# ---------------------------------------------------------------------------
include(${CMAKE_CURRENT_SOURCE_DIR}/dependencies/campello_gpu.cmake)
include(${CMAKE_CURRENT_SOURCE_DIR}/dependencies/campello_input.cmake)
include(${CMAKE_CURRENT_SOURCE_DIR}/dependencies/vector_math.cmake)
include(${CMAKE_CURRENT_SOURCE_DIR}/dependencies/campello_image.cmake)

# ---------------------------------------------------------------------------
# Platform-specific build
# ---------------------------------------------------------------------------
if(ANDROID)
    include(android.cmake)
elseif(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
    include(macos.cmake)
elseif(CMAKE_SYSTEM_NAME STREQUAL "iOS")
    include(ios.cmake)
elseif(WIN32)
    include(windows.cmake)
elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    include(linux.cmake)
else()
    message(FATAL_ERROR "Unsupported platform: ${CMAKE_SYSTEM_NAME}")
endif()

# ---------------------------------------------------------------------------
# Tests
# ---------------------------------------------------------------------------
if(BUILD_TESTS)
    include(${CMAKE_CURRENT_SOURCE_DIR}/dependencies/googletest.cmake)
    enable_testing()
    add_subdirectory(tests)
endif()

# ---------------------------------------------------------------------------
# Design system libraries — campello_ui / campello_material / campello_cupertino
# / campello_fluent
#
# Each is a standalone DesignSystem implementation (see
# inc/campello_widgets/ui/design_system.hpp) that links against
# campello_widgets, never the other way around. Placed after the BUILD_TESTS
# block above so GTest::gtest_main already exists for campello_ui's own
# test target. See TODO.md Phase 16.
# ---------------------------------------------------------------------------
if(BUILD_UI)
    add_subdirectory(campello_ui)
endif()
if(BUILD_MATERIAL)
    add_subdirectory(campello_material)
endif()
if(BUILD_CUPERTINO)
    add_subdirectory(campello_cupertino)
endif()
if(BUILD_FLUENT)
    add_subdirectory(campello_fluent)
endif()

# Cross-cutting abstract-contract suite (needs all three design systems
# defined above plus BUILD_TESTS' GTest, hence the extra guards).
if(BUILD_TESTS AND BUILD_UI AND BUILD_MATERIAL AND BUILD_CUPERTINO)
    add_subdirectory(tests/design_system_contract)
endif()

# ---------------------------------------------------------------------------
# Examples
# ---------------------------------------------------------------------------
if(BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()
