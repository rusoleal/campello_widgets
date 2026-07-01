cmake_minimum_required(VERSION 3.22.1)

add_executable(campello_widgets_gallery main.cpp ../gallery_app.cpp)

target_include_directories(campello_widgets_gallery PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/..)

target_link_libraries(campello_widgets_gallery PRIVATE campello_widgets)

set_target_properties(campello_widgets_gallery PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
)
