cmake_minimum_required(VERSION 3.22.1)
project(campello_widgets_multi_window)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT TARGET campello_widgets)
    add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../..
                     ${CMAKE_BINARY_DIR}/campello_widgets)
endif()

# A console-subsystem program (main()), so the self-tests' [demo] output is
# visible when run from a terminal.
add_executable(campello_widgets_multi_window main.cpp)
target_compile_definitions(campello_widgets_multi_window PRIVATE NOMINMAX)
target_link_libraries(campello_widgets_multi_window PRIVATE campello_widgets user32 gdi32)
set_target_properties(campello_widgets_multi_window PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
)

# campello_gpu / campello_image are separate DLLs under _deps/: copy every
# runtime DLL next to the executable.
add_custom_command(TARGET campello_widgets_multi_window POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        $<TARGET_RUNTIME_DLLS:campello_widgets_multi_window>
        $<TARGET_FILE_DIR:campello_widgets_multi_window>
    COMMAND_EXPAND_LISTS
)
