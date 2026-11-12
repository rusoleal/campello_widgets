cmake_minimum_required(VERSION 3.22.1)

# Multi-window on the web (Phase 17 M4): open web_multi_window.html from a web
# server (e.g. python3 -m http.server in the build directory).
add_executable(web_multi_window main.cpp)
target_link_libraries(web_multi_window PRIVATE campello_widgets)

set_target_properties(web_multi_window PROPERTIES
    SUFFIX ".html"
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
)
target_link_options(web_multi_window PRIVATE
    -sALLOW_MEMORY_GROWTH=1
    -sSTACK_SIZE=1MB
    --shell-file ${CMAKE_CURRENT_SOURCE_DIR}/shell.html
)
set_property(TARGET web_multi_window APPEND PROPERTY LINK_DEPENDS
    ${CMAKE_CURRENT_SOURCE_DIR}/shell.html)
