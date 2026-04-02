cmake_minimum_required(VERSION 3.22.1)

if(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
    add_subdirectory(macos_hello)
    add_subdirectory(macos_counter)
    add_subdirectory(macos_listview)
    add_subdirectory(macos_animated)
    add_subdirectory(macos_keyboard)
endif()
