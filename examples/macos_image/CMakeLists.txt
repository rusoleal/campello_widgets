add_executable(campello_widgets_image main.mm)

set_target_properties(campello_widgets_image PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
    MACOSX_BUNDLE TRUE
    MACOSX_BUNDLE_INFO_PLIST ${CMAKE_CURRENT_SOURCE_DIR}/Info.plist
)

target_link_libraries(campello_widgets_image
    PRIVATE
        campello_widgets
)

# Copy any test images to the bundle
set(IMAGE_RESOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/test_image.png
)

# If no test image exists, we'll generate one at runtime
# This is just a placeholder for the resource copy
