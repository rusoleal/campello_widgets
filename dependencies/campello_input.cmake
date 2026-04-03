cmake_minimum_required(VERSION 3.5.0 FATAL_ERROR)
cmake_policy(SET CMP0077 NEW)

include(FetchContent)

FetchContent_Declare(
    campello_input
    GIT_REPOSITORY https://github.com/rusoleal/campello_input
    GIT_TAG        v0.2.1
)

if(NOT campello_input_POPULATED)
    set(CMAKE_POSITION_INDEPENDENT_CODE ON)
    # Disable campello_input tests - they have broken install targets on some platforms
    set(CAMPELLO_INPUT_BUILD_TESTS OFF CACHE BOOL "" FORCE)
    set(BUILD_TESTS_SAVED ${BUILD_TESTS})
    set(BUILD_TESTS OFF)
    FetchContent_Populate(campello_input)
    include_directories(${campello_input_SOURCE_DIR}/inc)
    add_subdirectory(${campello_input_SOURCE_DIR} ${campello_input_BINARY_DIR} EXCLUDE_FROM_ALL)
    set(BUILD_TESTS ${BUILD_TESTS_SAVED})
endif()
