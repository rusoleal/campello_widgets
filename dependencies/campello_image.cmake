cmake_minimum_required(VERSION 3.5.0 FATAL_ERROR)
cmake_policy(SET CMP0077 NEW)

include(FetchContent)

# campello_image - cross-platform C++20 image loading library
# Supports JPEG, PNG, BMP, TGA, GIF, and WebP
FetchContent_Declare(
    campello_image
    GIT_REPOSITORY https://github.com/rusoleal/campello_image.git
    GIT_TAG        v0.3.1  # Use latest stable release
)

if(NOT campello_image_POPULATED)
    set(CMAKE_POSITION_INDEPENDENT_CODE ON)
    FetchContent_MakeAvailable(campello_image)
    
    # Disable unity build for libwebp targets that have conflicting symbols
    # (static functions with same names but different signatures)
    foreach(target sharpyuv webpencode webpdecode webpdspdecode webputilsdecode)
        if(TARGET ${target})
            set_target_properties(${target} PROPERTIES UNITY_BUILD OFF)
        endif()
    endforeach()
endif()
