file(GLOB_RECURSE CAMPELLO_WIDGETS_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
)
list(FILTER CAMPELLO_WIDGETS_SOURCES EXCLUDE REGEX ".*/src/(android|macos|ios|windows)/.*")

add_library(campello_widgets SHARED ${CAMPELLO_WIDGETS_SOURCES})

target_include_directories(campello_widgets
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/inc>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}>
        $<INSTALL_INTERFACE:inc>
)

# Linux platform dependencies: X11, D-Bus (IBus IME), FreeType, HarfBuzz, fontconfig
find_package(PkgConfig REQUIRED)
pkg_check_modules(LINUX_DEPS REQUIRED x11 dbus-1 freetype2 harfbuzz fontconfig)

# Optional Wayland support
pkg_check_modules(WAYLAND_DEPS wayland-client xkbcommon)
if(WAYLAND_DEPS_FOUND)
    message(STATUS "Wayland support enabled")
    target_compile_definitions(campello_widgets PRIVATE CAMPHELLO_WIDGETS_HAS_WAYLAND=1)
    list(APPEND LINUX_DEPS_INCLUDE_DIRS ${WAYLAND_DEPS_INCLUDE_DIRS})
    list(APPEND LINUX_DEPS_LIBRARIES    ${WAYLAND_DEPS_LIBRARIES})
    list(APPEND LINUX_DEPS_LDFLAGS_OTHER ${WAYLAND_DEPS_LDFLAGS_OTHER})
else()
    message(STATUS "Wayland support disabled (wayland-client and/or xkbcommon not found)")
endif()

target_include_directories(campello_widgets
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${CMAKE_CURRENT_SOURCE_DIR}/src/linux/protocols
        ${LINUX_DEPS_INCLUDE_DIRS}
)

target_link_libraries(campello_widgets
    PUBLIC
        campello_gpu
        campello_input
        vector_math
        campello_image
    PRIVATE
        curl
        ${LINUX_DEPS_LIBRARIES}
)

target_compile_options(campello_widgets PRIVATE -Wall -Wextra)
target_link_options(campello_widgets PRIVATE ${LINUX_DEPS_LDFLAGS_OTHER})
