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

# GPU visual renderer: real Metal implementation on macOS, null stub elsewhere
if(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
    list(APPEND TESTING_SOURCES
        "${CMAKE_CURRENT_SOURCE_DIR}/../src/testing/gpu_visual_renderer.mm")
else()
    list(APPEND TESTING_SOURCES
        "${CMAKE_CURRENT_SOURCE_DIR}/../src/testing/gpu_visual_renderer_stub.cpp")
endif()

if(UNIVERSAL_TEST_SOURCES)
    add_executable(campello_widgets_tests ${UNIVERSAL_TEST_SOURCES} ${TESTING_SOURCES})
    
    # Include directories for stb headers
    target_include_directories(campello_widgets_tests PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}"
        "${CMAKE_CURRENT_SOURCE_DIR}/../src/testing"
    )
    
    target_link_libraries(campello_widgets_tests PRIVATE
        campello_widgets
        GTest::gtest_main
    )

    # On Windows the DLLs must live next to the test EXE so it can load them.
    if(WIN32)
        add_custom_command(TARGET campello_widgets_tests POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                $<TARGET_FILE:campello_widgets>
                $<TARGET_FILE_DIR:campello_widgets_tests>
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                $<TARGET_FILE:campello_gpu>
                $<TARGET_FILE_DIR:campello_widgets_tests>
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

    if(PLATFORM_TEST_SOURCES)
        add_executable(campello_widgets_integration_tests ${PLATFORM_TEST_SOURCES})
        target_link_libraries(campello_widgets_integration_tests PRIVATE
            campello_widgets
            GTest::gtest_main
        )
        include(GoogleTest)
        gtest_discover_tests(campello_widgets_integration_tests)
    endif()
endif()
