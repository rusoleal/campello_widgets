cmake_minimum_required(VERSION 3.22.1)

# The gallery in the browser: open campello_widgets_gallery.html from a web
# server (e.g. python3 -m http.server in the build directory).
add_executable(campello_widgets_gallery main.cpp ../gallery_app.cpp)

target_include_directories(campello_widgets_gallery PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/..)

target_link_libraries(campello_widgets_gallery PRIVATE
    campello_widgets
    campello_ui
    campello_material
    campello_cupertino
    campello_fluent
)

set_target_properties(campello_widgets_gallery PROPERTIES
    SUFFIX ".html"
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
)
target_link_options(campello_widgets_gallery PRIVATE
    -sALLOW_MEMORY_GROWTH=1
    -sSTACK_SIZE=1MB
    --shell-file ${CMAKE_CURRENT_SOURCE_DIR}/shell.html
)
set_property(TARGET campello_widgets_gallery APPEND PROPERTY LINK_DEPENDS
    ${CMAKE_CURRENT_SOURCE_DIR}/shell.html)
