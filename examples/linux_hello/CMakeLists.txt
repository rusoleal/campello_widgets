cmake_minimum_required(VERSION 3.22.1)

add_executable(campello_widgets_linux_hello main.cpp)

target_link_libraries(campello_widgets_linux_hello PRIVATE campello_widgets)

set_target_properties(campello_widgets_linux_hello PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
)
