cmake_minimum_required(VERSION 3.5.0 FATAL_ERROR)
cmake_policy(SET CMP0077 NEW)

if(TARGET campello_widgets)
    message(STATUS "campello_widgets target already exists, skipping FetchContent")
    return()
endif()

include(FetchContent)

# This file lives inside the campello_widgets repo itself (examples/gallery/
# android/app/src/main/cpp/) — point SOURCE_DIR at the repo root relative to
# this file's own location rather than a developer-specific absolute path.
get_filename_component(_campello_widgets_repo_root
    "${CMAKE_CURRENT_LIST_DIR}/../../../../../../.." ABSOLUTE)

FetchContent_Declare(
    campello_widgets
    SOURCE_DIR ${_campello_widgets_repo_root}
)

if(NOT campello_widgets_POPULATED)
    set(CMAKE_POSITION_INDEPENDENT_CODE ON)
    FetchContent_Populate(campello_widgets)
    include_directories(${campello_widgets_SOURCE_DIR}/inc)
    set(_saved_BUILD_TESTS             "${BUILD_TESTS}")
    set(_saved_BUILD_INTEGRATION_TESTS "${BUILD_INTEGRATION_TESTS}")
    set(_saved_BUILD_EXAMPLES          "${BUILD_EXAMPLES}")
    set(BUILD_TESTS             OFF CACHE BOOL "" FORCE)
    set(BUILD_INTEGRATION_TESTS OFF CACHE BOOL "" FORCE)
    set(BUILD_EXAMPLES          OFF CACHE BOOL "" FORCE)
    add_subdirectory(${campello_widgets_SOURCE_DIR} ${campello_widgets_BINARY_DIR} EXCLUDE_FROM_ALL)
    set(BUILD_TESTS             "${_saved_BUILD_TESTS}"             CACHE BOOL "" FORCE)
    set(BUILD_INTEGRATION_TESTS "${_saved_BUILD_INTEGRATION_TESTS}" CACHE BOOL "" FORCE)
    set(BUILD_EXAMPLES          "${_saved_BUILD_EXAMPLES}"          CACHE BOOL "" FORCE)
endif()
