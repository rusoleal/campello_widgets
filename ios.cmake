file(GLOB_RECURSE CAMPELLO_WIDGETS_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.mm"
)
list(FILTER CAMPELLO_WIDGETS_SOURCES EXCLUDE REGEX ".*/src/(android|macos|windows|linux)/.*")

add_library(campello_widgets STATIC ${CAMPELLO_WIDGETS_SOURCES})

target_include_directories(campello_widgets
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/inc>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}>
        $<INSTALL_INTERFACE:inc>
)

target_link_libraries(campello_widgets
    PUBLIC
        campello_gpu
        campello_input
        vector_math
        campello_image
    PRIVATE
        "-framework UIKit"
        "-framework Metal"
        "-framework MetalKit"
        "-framework CoreText"
        "-framework CoreGraphics"
        "-framework Foundation"
        "-framework CFNetwork"
        "-framework CoreFoundation"
)

target_compile_options(campello_widgets PRIVATE -Wall -Wextra)

# Enable Automatic Reference Counting for Objective-C++ files.
# This is required for __weak and modern property attributes (strong, weak).
# The flag is ignored for regular C++ sources, so we apply it to all files.
target_compile_options(campello_widgets PRIVATE -fobjc-arc)

# Disable unity build for campello_widgets - it contains .mm (Objective-C++) files
# that cannot be combined with regular C++ files in a unity build
set_target_properties(campello_widgets PROPERTIES UNITY_BUILD OFF)
