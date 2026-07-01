cmake_minimum_required(VERSION 3.22.1)
project(campello_widgets_gallery)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_OBJCXX_STANDARD 20)

if(NOT TARGET campello_widgets)
    add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../../..
                     ${CMAKE_BINARY_DIR}/campello_widgets)
endif()

add_executable(campello_widgets_gallery MACOSX_BUNDLE
    main.mm
    ../gallery_app.cpp
)

set_target_properties(campello_widgets_gallery PROPERTIES
    MACOSX_BUNDLE_BUNDLE_NAME          "campello_widgets Gallery"
    MACOSX_BUNDLE_GUI_IDENTIFIER       "systems.leal.campello-widgets-gallery"
    MACOSX_BUNDLE_SHORT_VERSION_STRING "0.1.0"
    XCODE_ATTRIBUTE_TARGETED_DEVICE_FAMILY "1,2"
    XCODE_ATTRIBUTE_IPHONEOS_DEPLOYMENT_TARGET "16.0"
)

target_include_directories(campello_widgets_gallery PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/..)

target_compile_options(campello_widgets_gallery PRIVATE -fobjc-arc)

target_link_libraries(campello_widgets_gallery
    campello_widgets
    "-framework UIKit"
    "-framework Metal"
    "-framework MetalKit"
    "-framework CoreText"
    "-framework CoreGraphics"
    "-framework Foundation"
)
