cmake_minimum_required(VERSION 3.5.0 FATAL_ERROR)
cmake_policy(SET CMP0077 NEW)

# Skip if Clipper2 target is already defined (e.g., by parent project)
if(TARGET Clipper2)
    message(STATUS "Clipper2 target already exists, skipping FetchContent")
    return()
endif()

include(FetchContent)

FetchContent_Declare(
    clipper2
    GIT_REPOSITORY https://github.com/AngusJohnson/Clipper2.git
    GIT_TAG        Clipper2_2.0.1
)

if(NOT clipper2_POPULATED)
    set(CMAKE_POSITION_INDEPENDENT_CODE ON)
    set(CLIPPER2_UTILS OFF CACHE BOOL "" FORCE)
    set(CLIPPER2_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(CLIPPER2_TESTS OFF CACHE BOOL "" FORCE)
    set(CLIPPER2_USINGZ "OFF" CACHE STRING "" FORCE)   # don't need the Z-tagged variant
    FetchContent_Populate(clipper2)
    add_subdirectory(${clipper2_SOURCE_DIR}/CPP ${clipper2_BINARY_DIR} EXCLUDE_FROM_ALL)

    # Defensive, matching campello_gpu.cmake's precedent: keep Clipper2's own .cpp files out
    # of this project's CMAKE_UNITY_BUILD batching (no benefit for a small vendored lib, and
    # avoids introducing a new class of unity-merge risk this session already hit once).
    if(TARGET Clipper2)
        set_target_properties(Clipper2 PROPERTIES UNITY_BUILD OFF)
    endif()
endif()
