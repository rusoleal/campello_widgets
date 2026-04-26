cmake_minimum_required(VERSION 3.22.1)

project(campello_widgets VERSION 0.3.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ---------------------------------------------------------------------------
# Options
# ---------------------------------------------------------------------------
option(BUILD_TESTS            "Build universal tests (no GPU required)"       OFF)
option(BUILD_INTEGRATION_TESTS "Build platform-specific integration tests"   OFF)
option(BUILD_EXAMPLES         "Build example applications"                    OFF)
option(ENABLE_UNITY_BUILD     "Enable Unity Build for faster compilation"     ON)

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
# Examples
# ---------------------------------------------------------------------------
if(BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()
