cmake_minimum_required(VERSION 3.22.1)
project(campello_widgets_gallery)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_OBJCXX_STANDARD 20)

if(NOT TARGET campello_widgets)
    add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../../..
                     ${CMAKE_BINARY_DIR}/campello_widgets)
endif()

add_executable(campello_widgets_gallery MACOSX_BUNDLE
    main.mm
    ../gallery_app.cpp
    ../assets/sample_video.mp4
)

# Bundled as an app resource (looked up via NSBundle in main.mm at
# runtime) rather than referenced by a compile-time absolute path the way
# the macOS flavor does — this app is sandboxed, no access to an arbitrary
# host filesystem path. MACOSX_PACKAGE_LOCATION applies to iOS .app
# bundles too despite the name; same bundle machinery as macOS.
set_source_files_properties(../assets/sample_video.mp4 PROPERTIES
    MACOSX_PACKAGE_LOCATION "Resources")

# Unity Build (see root CMakeLists.txt's ENABLE_UNITY_BUILD) batches
# main.mm together with plain .cpp translation units, compiling it as
# pure C++ instead of Objective-C++ — silently harmless while main.mm had
# no actual Objective-C syntax, but now that it does (NSBundle lookup for
# the bundled sample video), that produces cascading "unknown type NSString"
# errors from deep inside Foundation.h. Same fix macos.cmake already
# applies to the core library's own .mm files.
set_source_files_properties(main.mm PROPERTIES SKIP_UNITY_BUILD_INCLUSION ON)

set_target_properties(campello_widgets_gallery PROPERTIES
    MACOSX_BUNDLE_BUNDLE_NAME          "campello_widgets Gallery"
    MACOSX_BUNDLE_GUI_IDENTIFIER       "systems.leal.campello-widgets-gallery"
    MACOSX_BUNDLE_SHORT_VERSION_STRING "0.1.0"
    MACOSX_BUNDLE_INFO_PLIST           "${CMAKE_CURRENT_SOURCE_DIR}/Info.plist.in"
    XCODE_ATTRIBUTE_TARGETED_DEVICE_FAMILY "1,2"
    XCODE_ATTRIBUTE_IPHONEOS_DEPLOYMENT_TARGET "16.0"
    XCODE_ATTRIBUTE_LD_RUNPATH_SEARCH_PATHS "@executable_path/Frameworks"
)

target_include_directories(campello_widgets_gallery PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/..)

target_compile_options(campello_widgets_gallery PRIVATE -fobjc-arc)

target_link_libraries(campello_widgets_gallery
    campello_widgets
    campello_ui
    campello_material
    campello_cupertino
    "-framework UIKit"
    "-framework Metal"
    "-framework MetalKit"
    "-framework CoreText"
    "-framework CoreGraphics"
    "-framework Foundation"
    "-framework AVFoundation"
    "-framework CoreMedia"
    "-framework CoreVideo"
)

# campello_gpu/campello_image build as shared libraries (.dylib). On the
# Simulator the app process runs natively on the Mac's own filesystem, so
# @rpath entries pointing at the build directory happen to resolve and the
# missing "embed" step goes unnoticed. On a real device the app is sandboxed
# to its own bundle — without embedding, dyld can't find these at launch
# ("Library not loaded: @rpath/libcampello_gpu.dylib") and the app crashes
# immediately. XCODE_EMBED_FRAMEWORKS can't resolve these targets (they're
# nested two FetchContent add_subdirectory() levels down from this
# CMakeLists.txt), so copy + re-sign them into the bundle directly instead.
# CODE_SIGN_IDENTITY is exported by Xcode's build environment into this
# script phase; empty on the Simulator (ad-hoc), a real identity on-device.
add_custom_command(TARGET campello_widgets_gallery POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory
        "$<TARGET_BUNDLE_CONTENT_DIR:campello_widgets_gallery>/Frameworks"
    COMMAND ${CMAKE_COMMAND} -E copy
        "$<TARGET_FILE:campello_gpu>" "$<TARGET_FILE:campello_image>"
        "$<TARGET_BUNDLE_CONTENT_DIR:campello_widgets_gallery>/Frameworks/"
    COMMAND /bin/sh -c "codesign --force --sign \"\${EXPANDED_CODE_SIGN_IDENTITY:--}\" \"$<TARGET_BUNDLE_CONTENT_DIR:campello_widgets_gallery>/Frameworks/$<TARGET_FILE_NAME:campello_gpu>\" \"$<TARGET_BUNDLE_CONTENT_DIR:campello_widgets_gallery>/Frameworks/$<TARGET_FILE_NAME:campello_image>\""
    COMMENT "Embedding and signing campello_gpu/campello_image into app bundle"
)
