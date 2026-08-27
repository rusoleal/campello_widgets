cmake_minimum_required(VERSION 3.22.1)

# Universal tests — no GPU/input hardware required
file(GLOB_RECURSE UNIVERSAL_TEST_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/universal/*.cpp"
)

# Add testing sources (excluded from main library)
set(TESTING_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/../src/testing/fidelity.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/../src/testing/visual_fidelity.cpp"
)

# Offscreen GPU capture: platform-specific draw-backend factory. This wraps
# the same production IDrawBackend (MetalDrawBackend/VulkanDrawBackend/
# D3DDrawBackend) real running apps use — see src/testing/offscreen_draw_backend.hpp.
if(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
    list(APPEND TESTING_SOURCES
        "${CMAKE_CURRENT_SOURCE_DIR}/../src/testing/offscreen_draw_backend_metal.mm")
elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux" OR CMAKE_SYSTEM_NAME STREQUAL "Android")
    list(APPEND TESTING_SOURCES
        "${CMAKE_CURRENT_SOURCE_DIR}/../src/testing/offscreen_draw_backend_vulkan.cpp")
elseif(WIN32)
    list(APPEND TESTING_SOURCES
        "${CMAKE_CURRENT_SOURCE_DIR}/../src/testing/offscreen_draw_backend_d3d.cpp")
else()
    list(APPEND TESTING_SOURCES
        "${CMAKE_CURRENT_SOURCE_DIR}/../src/testing/offscreen_draw_backend_stub.cpp")
endif()

if(UNIVERSAL_TEST_SOURCES)
    add_executable(campello_widgets_tests ${UNIVERSAL_TEST_SOURCES} ${TESTING_SOURCES})
    
    # Include directories for stb headers, plus src/ itself so tests can
    # reach backend-internal-but-pure-logic headers (e.g. gpu/stroke_geometry.hpp)
    # the same way src/gpu/*.cpp already includes them ("gpu/foo.hpp").
    target_include_directories(campello_widgets_tests PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}"
        "${CMAKE_CURRENT_SOURCE_DIR}/../src/testing"
        "${CMAKE_CURRENT_SOURCE_DIR}/../src"
    )
    
    target_link_libraries(campello_widgets_tests PRIVATE
        campello_widgets
        GTest::gtest_main
    )

    # Standalone thematic fidelity harness — renders DesignSystem components to PNG.
    add_executable(campello_widgets_thematic_fidelity
        "${CMAKE_CURRENT_SOURCE_DIR}/visual_fidelity/themed_component_harness.cpp"
        ${TESTING_SOURCES})
    target_include_directories(campello_widgets_thematic_fidelity PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}"
        "${CMAKE_CURRENT_SOURCE_DIR}/../src/testing"
    )
    target_link_libraries(campello_widgets_thematic_fidelity PRIVATE
        campello_widgets
        campello_ui
        campello_material
        campello_cupertino
        campello_fluent
    )

    # On Windows the DLLs must live next to the test EXE so it can load them.
    if(WIN32)
        add_custom_command(TARGET campello_widgets_tests POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                $<TARGET_FILE:campello_widgets>
                $<TARGET_FILE:campello_gpu>
                $<TARGET_FILE:campello_image>
                $<TARGET_FILE_DIR:campello_widgets_tests>
        )
        add_custom_command(TARGET campello_widgets_thematic_fidelity POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                $<TARGET_FILE:campello_widgets>
                $<TARGET_FILE:campello_gpu>
                $<TARGET_FILE:campello_image>
                $<TARGET_FILE_DIR:campello_widgets_thematic_fidelity>
        )
    endif()

    include(GoogleTest)
    # PRE_TEST defers discovery to ctest time so the post-build copy runs first.
    gtest_discover_tests(campello_widgets_tests DISCOVERY_MODE PRE_TEST)
endif()

# Platform integration tests — require real GPU/input
if(BUILD_INTEGRATION_TESTS)
    file(GLOB_RECURSE PLATFORM_TEST_SOURCES
        "${CMAKE_CURRENT_SOURCE_DIR}/platform/*.cpp"
    )
    if(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
        file(GLOB_RECURSE PLATFORM_TEST_OBJC_SOURCES
            "${CMAKE_CURRENT_SOURCE_DIR}/platform/*.mm"
        )
        list(APPEND PLATFORM_TEST_SOURCES ${PLATFORM_TEST_OBJC_SOURCES})
    endif()

    if(PLATFORM_TEST_SOURCES)
        add_executable(campello_widgets_integration_tests ${PLATFORM_TEST_SOURCES})
        target_link_libraries(campello_widgets_integration_tests PRIVATE
            campello_widgets
            GTest::gtest_main
        )
        # .mm files must be compiled as Objective-C++ and cannot participate in unity build
        if(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
            set_target_properties(campello_widgets_integration_tests PROPERTIES UNITY_BUILD OFF)
        endif()
        include(GoogleTest)
        gtest_discover_tests(campello_widgets_integration_tests)
    endif()
endif()
