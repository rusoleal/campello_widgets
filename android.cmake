file(GLOB_RECURSE CAMPELLO_WIDGETS_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
)
list(FILTER CAMPELLO_WIDGETS_SOURCES EXCLUDE REGEX ".*/src/(macos|ios|windows|linux)/.*")

add_library(campello_widgets SHARED ${CAMPELLO_WIDGETS_SOURCES})

target_include_directories(campello_widgets
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/inc>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}>
        $<INSTALL_INTERFACE:inc>
)

if(ANDROID_NDK)
    target_include_directories(campello_widgets PRIVATE
        ${ANDROID_NDK}/sources/android/native_app_glue
    )
endif()

target_link_libraries(campello_widgets
    PUBLIC
        campello_gpu
        campello_input
        vector_math
        campello_image
        android
        log
)

target_compile_options(campello_widgets PRIVATE -Wall -Wextra)
