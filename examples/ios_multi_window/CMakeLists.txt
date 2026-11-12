cmake_minimum_required(VERSION 3.22.1)

# Multi-window on iPadOS (Phase 17 M5): each window is a UIWindowScene.
# Run it on an iPad (Simulator or device); on iPhone only the main window opens.
add_executable(campello_widgets_ios_multi_window MACOSX_BUNDLE main.mm)

# Objective-C++ (NSBundle, dispatch, UIApplication): keep it out of Unity Build.
set_source_files_properties(main.mm PROPERTIES SKIP_UNITY_BUILD_INCLUSION ON)

set_target_properties(campello_widgets_ios_multi_window PROPERTIES
    MACOSX_BUNDLE_BUNDLE_NAME          "campello_widgets Multi-window"
    MACOSX_BUNDLE_GUI_IDENTIFIER       "systems.leal.campello-widgets-multi-window"
    MACOSX_BUNDLE_SHORT_VERSION_STRING "0.1.0"
    MACOSX_BUNDLE_INFO_PLIST           "${CMAKE_CURRENT_SOURCE_DIR}/Info.plist.in"
    XCODE_ATTRIBUTE_TARGETED_DEVICE_FAMILY "1,2"
    XCODE_ATTRIBUTE_IPHONEOS_DEPLOYMENT_TARGET "16.0"
    XCODE_ATTRIBUTE_LD_RUNPATH_SEARCH_PATHS "@executable_path/Frameworks"
)

target_compile_options(campello_widgets_ios_multi_window PRIVATE -fobjc-arc)

target_link_libraries(campello_widgets_ios_multi_window
    campello_widgets
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

# Embed the shared campello_gpu/campello_image libraries (see gallery/ios).
add_custom_command(TARGET campello_widgets_ios_multi_window POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory
        "$<TARGET_BUNDLE_CONTENT_DIR:campello_widgets_ios_multi_window>/Frameworks"
    COMMAND ${CMAKE_COMMAND} -E copy
        "$<TARGET_FILE:campello_gpu>" "$<TARGET_FILE:campello_image>"
        "$<TARGET_BUNDLE_CONTENT_DIR:campello_widgets_ios_multi_window>/Frameworks/"
    COMMAND /bin/sh -c "codesign --force --sign \"\${EXPANDED_CODE_SIGN_IDENTITY:--}\" \"$<TARGET_BUNDLE_CONTENT_DIR:campello_widgets_ios_multi_window>/Frameworks/$<TARGET_FILE_NAME:campello_gpu>\" \"$<TARGET_BUNDLE_CONTENT_DIR:campello_widgets_ios_multi_window>/Frameworks/$<TARGET_FILE_NAME:campello_image>\""
    COMMENT "Embedding and signing campello_gpu/campello_image into app bundle"
)
