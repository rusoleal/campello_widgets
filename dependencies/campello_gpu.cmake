cmake_minimum_required(VERSION 3.5.0 FATAL_ERROR)
cmake_policy(SET CMP0077 NEW)

include(FetchContent)

FetchContent_Declare(
    campello_gpu
    GIT_REPOSITORY https://github.com/rusoleal/campello_gpu
    GIT_TAG        v0.4.1
)

if(NOT campello_gpu_POPULATED)
    set(CMAKE_POSITION_INDEPENDENT_CODE ON)
    FetchContent_Populate(campello_gpu)
    include_directories(${campello_gpu_SOURCE_DIR}/inc)
    # Prevent campello_gpu's own tests from being configured as a subdirectory —
    # v0.3.7 uses CMAKE_SOURCE_DIR in its tests/CMakeLists.txt which resolves to
    # campello_widgets' root when included as a subdirectory.
    set(_saved_BUILD_TESTS "${BUILD_TESTS}")
    set(_saved_BUILD_INTEGRATION_TESTS "${BUILD_INTEGRATION_TESTS}")
    set(BUILD_TESTS OFF CACHE BOOL "" FORCE)
    set(BUILD_INTEGRATION_TESTS OFF CACHE BOOL "" FORCE)
    add_subdirectory(${campello_gpu_SOURCE_DIR} ${campello_gpu_BINARY_DIR} EXCLUDE_FROM_ALL)
    set(BUILD_TESTS "${_saved_BUILD_TESTS}" CACHE BOOL "" FORCE)
    set(BUILD_INTEGRATION_TESTS "${_saved_BUILD_INTEGRATION_TESTS}" CACHE BOOL "" FORCE)
endif()
