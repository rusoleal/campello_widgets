cmake_minimum_required(VERSION 3.22.1)
project(campello_widgets_multi_window)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_OBJCXX_STANDARD 20)

if(NOT TARGET campello_widgets)
    add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../..
                     ${CMAKE_BINARY_DIR}/campello_widgets)
endif()

add_executable(campello_widgets_multi_window MACOSX_BUNDLE
    main.mm
)

# main.mm uses Objective-C (the CW_MULTIWINDOW_DEMO=2 self-test sends AppKit
# events). Without OBJCXX enabled for this project, CMake compiles .mm as
# plain C++ (inside a Unity Build .cxx), where Cocoa headers don't parse.
set_source_files_properties(main.mm PROPERTIES
    SKIP_UNITY_BUILD_INCLUSION ON
    COMPILE_OPTIONS "-xobjective-c++;-fobjc-arc")

set_target_properties(campello_widgets_multi_window PROPERTIES
    MACOSX_BUNDLE_BUNDLE_NAME         "campello_widgets Multi-window"
    MACOSX_BUNDLE_GUI_IDENTIFIER      "systems.leal.campello-widgets-multi-window"
    MACOSX_BUNDLE_SHORT_VERSION_STRING "0.1.0"
)

target_link_libraries(campello_widgets_multi_window
    campello_widgets
    "-framework Cocoa"
    "-framework Metal"
    "-framework MetalKit"
)
