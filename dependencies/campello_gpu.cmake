cmake_minimum_required(VERSION 3.5.0 FATAL_ERROR)
cmake_policy(SET CMP0077 NEW)

# Skip if campello_gpu target is already defined (e.g., by parent project)
if(TARGET campello_gpu)
    message(STATUS "campello_gpu target already exists, skipping FetchContent")
    return()
endif()

include(FetchContent)

# Temporary local-testing override: if a sibling campello_gpu checkout exists,
# build against its working tree directly instead of fetching from GitHub.
set(_campello_gpu_local_dir "${CMAKE_CURRENT_SOURCE_DIR}/../campello_gpu")
if(EXISTS "${_campello_gpu_local_dir}/CMakeLists.txt")
    message(STATUS "Using local campello_gpu checkout at ${_campello_gpu_local_dir}")
    FetchContent_Declare(
        campello_gpu
        SOURCE_DIR ${_campello_gpu_local_dir}
    )
else()
    FetchContent_Declare(
        campello_gpu
        GIT_REPOSITORY https://github.com/rusoleal/campello_gpu.git
        GIT_TAG        v0.24.1
    )
endif()

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
    
    # Disable unity build for campello_gpu - it has naming conflicts with Metal's
    # MTL::Device and MTL::Buffer when using 'using namespace' directives
    if(TARGET campello_gpu)
        set_target_properties(campello_gpu PROPERTIES UNITY_BUILD OFF)
    endif()
endif()
