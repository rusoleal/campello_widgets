cmake_minimum_required(VERSION 3.22.1)
project(campello_widgets_multi_window)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT TARGET campello_widgets)
    add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../..
                     ${CMAKE_BINARY_DIR}/campello_widgets)
endif()

find_package(PkgConfig REQUIRED)
pkg_check_modules(MULTI_WINDOW_X11 REQUIRED x11)

# The self-tests drive the windows through XTest (libXtst, loaded at run time).
add_executable(campello_widgets_multi_window main.cpp)
target_include_directories(campello_widgets_multi_window PRIVATE ${MULTI_WINDOW_X11_INCLUDE_DIRS})
target_link_libraries(campello_widgets_multi_window PRIVATE campello_widgets ${MULTI_WINDOW_X11_LIBRARIES} ${CMAKE_DL_LIBS})
set_target_properties(campello_widgets_multi_window PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
)
