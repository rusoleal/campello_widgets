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
)

target_include_directories(campello_widgets_gallery PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/..)

# Dev-only: the gallery runs directly out of the build tree (never
# redistributed), so a compile-time absolute path to examples/gallery/assets/
# is simpler and just as reliable as bundling sample_video.mp4 into the
# .app's Resources — no NSBundle lookup needed from gallery_app.cpp, which
# is portable C++, not Objective-C++.
target_compile_definitions(campello_widgets_gallery PRIVATE
    CAMPELLO_GALLERY_ASSETS_DIR="${CMAKE_CURRENT_SOURCE_DIR}/../assets")

target_link_libraries(campello_widgets_gallery
    campello_widgets
    campello_ui
    campello_material
    campello_cupertino
    campello_fluent
    "-framework Cocoa"
    "-framework Metal"
    "-framework MetalKit"
)
