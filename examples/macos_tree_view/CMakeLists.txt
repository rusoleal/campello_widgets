cmake_minimum_required(VERSION 3.22.1)
project(campello_widgets_treeview)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_OBJCXX_STANDARD 20)

if(NOT TARGET campello_widgets)
    add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../..
                     ${CMAKE_BINARY_DIR}/campello_widgets)
endif()

add_executable(campello_widgets_treeview MACOSX_BUNDLE
    main.mm
)

set_target_properties(campello_widgets_treeview PROPERTIES
    MACOSX_BUNDLE_BUNDLE_NAME         "campello_widgets Tree View"
    MACOSX_BUNDLE_GUI_IDENTIFIER      "systems.leal.campello-widgets-treeview"
    MACOSX_BUNDLE_SHORT_VERSION_STRING "0.1.0"
)

target_link_libraries(campello_widgets_treeview
    campello_widgets
    "-framework Cocoa"
    "-framework Metal"
    "-framework MetalKit"
)
