cmake_minimum_required(VERSION 3.5.0 FATAL_ERROR)
cmake_policy(SET CMP0077 NEW)

include(FetchContent)

FetchContent_Declare(
    vector_math
    GIT_REPOSITORY https://github.com/rusoleal/vector_math
    GIT_TAG        main
)

if(NOT vector_math_POPULATED)
    set(CMAKE_POSITION_INDEPENDENT_CODE ON)
    FetchContent_Populate(vector_math)
    include_directories(SYSTEM ${vector_math_SOURCE_DIR}/inc)
    add_subdirectory(${vector_math_SOURCE_DIR} ${vector_math_BINARY_DIR} EXCLUDE_FROM_ALL)
endif()
