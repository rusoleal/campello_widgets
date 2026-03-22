cmake_minimum_required(VERSION 3.22.1)
project(campello_widgets_hello)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_OBJCXX_STANDARD 20)

# campello_widgets is available from the parent project
if(NOT TARGET campello_widgets)
    add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../..
                     ${CMAKE_BINARY_DIR}/campello_widgets)
endif()

add_executable(campello_widgets_hello MACOSX_BUNDLE
    main.mm
)

set_target_properties(campello_widgets_hello PROPERTIES
    MACOSX_BUNDLE_BUNDLE_NAME         "campello_widgets Hello"
    MACOSX_BUNDLE_GUI_IDENTIFIER      "systems.leal.campello-widgets-hello"
    MACOSX_BUNDLE_SHORT_VERSION_STRING "0.1.0"
)

target_link_libraries(campello_widgets_hello
    campello_widgets
    "-framework Cocoa"
    "-framework Metal"
    "-framework MetalKit"
)
