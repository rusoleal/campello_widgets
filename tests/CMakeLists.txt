cmake_minimum_required(VERSION 3.22.1)

# Universal tests — no GPU/input hardware required
file(GLOB_RECURSE UNIVERSAL_TEST_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/universal/*.cpp"
)

if(UNIVERSAL_TEST_SOURCES)
    add_executable(campello_widgets_tests ${UNIVERSAL_TEST_SOURCES})
    target_link_libraries(campello_widgets_tests PRIVATE
        campello_widgets
        GTest::gtest_main
    )
    include(GoogleTest)
    gtest_discover_tests(campello_widgets_tests)
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
