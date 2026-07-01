cmake_minimum_required(VERSION 3.22.1)

project("campello_gallery")

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(${CMAKE_CURRENT_SOURCE_DIR}/campello_widgets.cmake)

add_library(campello_gallery SHARED
    main.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../../gallery_app.cpp
    ${ANDROID_NDK}/sources/android/native_app_glue/android_native_app_glue.c
)

target_include_directories(campello_gallery PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../../
    ${ANDROID_NDK}/sources/android/native_app_glue
)

target_link_libraries(campello_gallery
    campello_widgets
    android
    log
)
