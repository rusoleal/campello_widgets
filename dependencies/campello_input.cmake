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

    if(CMAKE_SYSTEM_NAME STREQUAL "Android")
        # campello_input v0.2.1 android.cmake requires the AGDK game-activity
        # package (find_package(game-activity REQUIRED)) which is not installed
        # on standard CI runners. campello_widgets' Android code does not
        # reference any campello_input symbols, so an INTERFACE stub is enough
        # to satisfy the link dependency without needing the full AGDK.
        configure_file(
            ${campello_input_SOURCE_DIR}/src/campello_input_config.h.in
            ${campello_input_BINARY_DIR}/campello_input_config.h
        )
        add_library(campello_input INTERFACE)
        target_include_directories(campello_input INTERFACE
            ${campello_input_SOURCE_DIR}/inc
            ${campello_input_BINARY_DIR}
        )

    elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
        # campello_input v0.2.1 linux.cmake only sets variables but never calls
        # add_library, so the target is never created and the install()/
        # get_target_property() calls in the upstream CMakeLists.txt error out.
        # Work around this by building the target ourselves instead of using
        # add_subdirectory.

        # Generate the version config header that configure_file would produce.
        set(_ci_src ${campello_input_SOURCE_DIR})
        configure_file(
            ${_ci_src}/src/campello_input_config.h.in
            ${campello_input_BINARY_DIR}/campello_input_config.h
        )
        include_directories(${campello_input_BINARY_DIR})

        find_package(PkgConfig REQUIRED)
        pkg_check_modules(CAMPELLO_INPUT_EVDEV REQUIRED libevdev)
        pkg_check_modules(CAMPELLO_INPUT_UDEV  REQUIRED libudev)

        add_library(campello_input SHARED
            ${_ci_src}/src/linux/input_linux_system.cpp
            ${_ci_src}/src/linux/gamepad_linux.cpp
            ${_ci_src}/src/linux/keyboard_linux.cpp
            ${_ci_src}/src/linux/mouse_linux.cpp
        )
        target_include_directories(campello_input PUBLIC
            ${_ci_src}/inc
            ${_ci_src}/src/linux/inc
            ${campello_input_BINARY_DIR}
            ${CAMPELLO_INPUT_EVDEV_INCLUDE_DIRS}
            ${CAMPELLO_INPUT_UDEV_INCLUDE_DIRS}
        )
        target_compile_definitions(campello_input PRIVATE
            CAMPHELLO_INPUT_LINUX
            ${CAMPELLO_INPUT_EVDEV_CFLAGS_OTHER}
            ${CAMPELLO_INPUT_UDEV_CFLAGS_OTHER}
        )
        target_link_libraries(campello_input PUBLIC
            ${CAMPELLO_INPUT_EVDEV_LIBRARIES}
            ${CAMPELLO_INPUT_UDEV_LIBRARIES}
        )
        set_target_properties(campello_input PROPERTIES UNITY_BUILD OFF)
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
