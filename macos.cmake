file(GLOB_RECURSE CAMPELLO_WIDGETS_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.mm"
)
list(FILTER CAMPELLO_WIDGETS_SOURCES EXCLUDE REGEX ".*/src/(android|ios|windows|linux)/.*")
list(FILTER CAMPELLO_WIDGETS_SOURCES EXCLUDE REGEX ".*/src/testing/.*")
# macOS uses the shared Metal backend (src/gpu/metal/), not Vulkan.
list(FILTER CAMPELLO_WIDGETS_SOURCES EXCLUDE REGEX ".*/src/gpu/vulkan/.*")

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

# Enable Automatic Reference Counting for Objective-C++ files, matching
# ios.cmake. The flag is ignored for regular C++ sources, so we apply it to
# all files. Without this, .mm files compiled under MRC by default — which
# previously meant __bridge_retained/__bridge_transfer casts were silent
# no-ops (caught via the -Warc-bridge-casts-disallowed-in-nonarc warning in
# run_app.mm).
target_compile_options(campello_widgets PRIVATE -fobjc-arc)

# platform_menu_delegate.mm deliberately leaks menu objects forever (see its
# own comments: a documented workaround for an AppKit issue where the async
# keyboard-shortcut updater accesses menu items after the menu bar has been
# replaced). Under ARC, its NSMutableArray* members would be __strong and
# get auto-released when MacOSPlatformMenuDelegate's destructor runs,
# silently undoing that workaround. Opt this one file out of ARC so its
# existing manual retain/never-release behavior is unchanged.
set_source_files_properties(
    "${CMAKE_CURRENT_SOURCE_DIR}/src/macos/platform_menu_delegate.mm"
    PROPERTIES COMPILE_OPTIONS "-fno-objc-arc")

# Exclude .mm (Objective-C++) files from unity build — they cannot be combined
# with regular C++ files in a unity batch.  The .cpp files are still unity-built.
foreach(src ${CAMPELLO_WIDGETS_SOURCES})
    if(src MATCHES "\\.mm$")
        set_source_files_properties(${src} PROPERTIES SKIP_UNITY_BUILD_INCLUSION ON)
    endif()
endforeach()
