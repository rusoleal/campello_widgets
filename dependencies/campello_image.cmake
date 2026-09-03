cmake_minimum_required(VERSION 3.5.0 FATAL_ERROR)
cmake_policy(SET CMP0077 NEW)

# Skip if campello_image target is already defined (e.g., by parent project)
if(TARGET campello_image)
    message(STATUS "campello_image target already exists, skipping FetchContent")
    return()
endif()

include(FetchContent)

# campello_image - cross-platform C++20 image loading library
# Supports JPEG, PNG, BMP, TGA, GIF, and WebP
FetchContent_Declare(
    campello_image
    GIT_REPOSITORY https://github.com/rusoleal/campello_image.git
    GIT_TAG        v0.5.1  # Use latest stable release
)

if(NOT campello_image_POPULATED)
    set(CMAKE_POSITION_INDEPENDENT_CODE ON)

    # Prevent campello_image's own unit tests from being configured as a
    # subdirectory (mirrors campello_gpu.cmake's identical guard) --
    # campello_image's own BUILD_TESTS option() is a no-op once this
    # project's own option(BUILD_TESTS ...) has already set the cache
    # variable, so its own tests/CMakeLists.txt (and its own
    # FetchContent_Declare(googletest ...), pinned independently and out of
    # this project's control) were silently pulled in and built as part of
    # campello_widgets' own ctest run -- both an unintended extra 33 tests
    # in that run and, since FetchContent_Declare is first-wins, silently
    # overriding this project's own dependencies/googletest.cmake pin.
    set(_saved_campello_image_BUILD_TESTS "${BUILD_TESTS}")
    set(BUILD_TESTS OFF CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(campello_image)
    set(BUILD_TESTS "${_saved_campello_image_BUILD_TESTS}" CACHE BOOL "" FORCE)

    # Disable unity build for libwebp targets that have conflicting symbols
    # (static functions with same names but different signatures)
    foreach(target sharpyuv webpencode webpdecode webpdspdecode webputilsdecode)
        if(TARGET ${target})
            set_target_properties(${target} PROPERTIES UNITY_BUILD OFF)
        endif()
    endforeach()
endif()
