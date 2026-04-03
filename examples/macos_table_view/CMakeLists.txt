cmake_minimum_required(VERSION 3.22.1)
project(campello_widgets_tableview)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_OBJCXX_STANDARD 20)

if(NOT TARGET campello_widgets)
    add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../..
                     ${CMAKE_BINARY_DIR}/campello_widgets)
endif()

add_executable(campello_widgets_tableview MACOSX_BUNDLE
    main.mm
)

set_target_properties(campello_widgets_tableview PROPERTIES
    MACOSX_BUNDLE_BUNDLE_NAME         "campello_widgets Table View"
    MACOSX_BUNDLE_GUI_IDENTIFIER      "systems.leal.campello-widgets-tableview"
    MACOSX_BUNDLE_SHORT_VERSION_STRING "0.1.0"
)

target_link_libraries(campello_widgets_tableview
    campello_widgets
    "-framework Cocoa"
    "-framework Metal"
    "-framework MetalKit"
)
