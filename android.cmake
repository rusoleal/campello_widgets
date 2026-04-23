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

set(ANDROID_NDK_PATH "")
if(ANDROID_NDK)
    set(ANDROID_NDK_PATH ${ANDROID_NDK})
elseif(CMAKE_ANDROID_NDK)
    set(ANDROID_NDK_PATH ${CMAKE_ANDROID_NDK})
elseif(ANDROID_NDK_HOME)
    set(ANDROID_NDK_PATH ${ANDROID_NDK_HOME})
elseif(DEFINED ENV{ANDROID_NDK_HOME})
    set(ANDROID_NDK_PATH $ENV{ANDROID_NDK_HOME})
elseif(CMAKE_TOOLCHAIN_FILE)
    # Derive NDK root from toolchain path: <ndk>/build/cmake/android.toolchain.cmake
    get_filename_component(_toolchain_dir ${CMAKE_TOOLCHAIN_FILE} DIRECTORY)
    get_filename_component(ANDROID_NDK_PATH ${_toolchain_dir}/../../.. ABSOLUTE)
endif()

if(ANDROID_NDK_PATH)
    target_include_directories(campello_widgets PRIVATE
        ${ANDROID_NDK_PATH}/sources/android/native_app_glue
    )
    message(STATUS "Android NDK native_app_glue include: ${ANDROID_NDK_PATH}/sources/android/native_app_glue")
else()
    message(WARNING "Could not locate Android NDK path for native_app_glue includes")
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
