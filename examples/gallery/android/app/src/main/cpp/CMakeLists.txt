cmake_minimum_required(VERSION 3.22.1)

project("campello_gallery")

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(${CMAKE_CURRENT_SOURCE_DIR}/campello_widgets.cmake)

add_library(campello_gallery SHARED
    main.cpp
    multi_window_demo.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../../gallery_app.cpp
)

target_include_directories(campello_gallery PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../../
)

target_link_libraries(campello_gallery
    campello_widgets
    campello_ui
    campello_material
    campello_cupertino
    campello_fluent
    android
    log
)
