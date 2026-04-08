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
    set(BUILD_TESTS_SAVED ${BUILD_TESTS})
    set(BUILD_TESTS OFF)
    FetchContent_Populate(campello_input)
    include_directories(${campello_input_SOURCE_DIR}/inc)

    if(CMAKE_SYSTEM_NAME STREQUAL "Android" OR CMAKE_SYSTEM_NAME STREQUAL "Linux" OR CMAKE_SYSTEM_NAME STREQUAL "iOS")
        # campello_input v0.2.1 has broken platform cmake/source files on Android,
        # Linux, and iOS that prevent a clean add_subdirectory build:
        #   - android.cmake: find_package(game-activity REQUIRED) — AGDK not
        #     installed on standard CI runners.
        #   - linux.cmake: only sets variables, never calls add_library, so the
        #     upstream install()/get_target_property() calls error during configure.
        #   - touch_apple.mm (iOS): references UITraitCollection.maximumNumberOfTouches
        #     (non-existent property) and CHHapticPatternPlayer (missing import),
        #     causing compile errors on the iOS SDK.
        # In all three cases campello_widgets' platform code does not reference any
        # campello_input symbols, so an INTERFACE stub satisfies the link
        # dependency without requiring a working campello_input build.
        configure_file(
            ${campello_input_SOURCE_DIR}/src/campello_input_config.h.in
            ${campello_input_BINARY_DIR}/campello_input_config.h
        )
        add_library(campello_input INTERFACE)
        target_include_directories(campello_input INTERFACE
            ${campello_input_SOURCE_DIR}/inc
            ${campello_input_BINARY_DIR}
        )

    else()
        # Disable campello_input tests — they have broken install targets on some platforms
        set(CAMPELLO_INPUT_BUILD_TESTS OFF CACHE BOOL "" FORCE)
        add_subdirectory(${campello_input_SOURCE_DIR} ${campello_input_BINARY_DIR} EXCLUDE_FROM_ALL)

        # Disable unity build for campello_input - it contains .mm (Objective-C++) files
        # that cannot be combined with regular C++ files in a unity build
        if(TARGET campello_input)
            set_target_properties(campello_input PROPERTIES UNITY_BUILD OFF)
        endif()
    endif()

    set(BUILD_TESTS ${BUILD_TESTS_SAVED})
endif()
