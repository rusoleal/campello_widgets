file(GLOB_RECURSE CAMPELLO_WIDGETS_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.mm"
)
list(FILTER CAMPELLO_WIDGETS_SOURCES EXCLUDE REGEX ".*/src/(android|ios|windows|linux)/.*")
list(FILTER CAMPELLO_WIDGETS_SOURCES EXCLUDE REGEX ".*/src/testing/.*")

add_library(campello_widgets SHARED ${CAMPELLO_WIDGETS_SOURCES})

target_include_directories(campello_widgets
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/inc>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}>
        $<INSTALL_INTERFACE:inc>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(campello_widgets
    PUBLIC
        campello_gpu
        campello_input
        vector_math
        campello_image
    PRIVATE
        "-framework Cocoa"
        "-framework Metal"
        "-framework MetalKit"
        "-framework CoreText"
        "-framework CoreGraphics"
        "-framework Foundation"
        "-framework CFNetwork"
        "-framework CoreFoundation"
)

target_compile_options(campello_widgets PRIVATE -Wall -Wextra)

# Exclude .mm (Objective-C++) files from unity build — they cannot be combined
# with regular C++ files in a unity batch.  The .cpp files are still unity-built.
foreach(src ${CAMPELLO_WIDGETS_SOURCES})
    if(src MATCHES "\\.mm$")
        set_source_files_properties(${src} PROPERTIES SKIP_UNITY_BUILD_INCLUSION ON)
    endif()
endforeach()
