# campello_ui — the bespoke "warm teal" design system, home of
# CampelloDesignSystem. This is campello_editor's own visual language, kept
# as a standalone DesignSystem implementation (see design_system.hpp in
# campello_widgets) rather than baked into the framework core.

file(GLOB_RECURSE CAMPELLO_UI_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp")

add_library(campello_ui ${CAMPELLO_UI_SOURCES})

target_include_directories(campello_ui
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/inc>
        $<INSTALL_INTERFACE:inc>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(campello_ui PUBLIC campello_widgets)

target_compile_options(campello_ui PRIVATE -Wall -Wextra)

if(BUILD_TESTS)
    file(GLOB_RECURSE CAMPELLO_UI_TEST_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/tests/*.cpp")
    if(CAMPELLO_UI_TEST_SOURCES)
        add_executable(campello_ui_tests ${CAMPELLO_UI_TEST_SOURCES})
        target_link_libraries(campello_ui_tests PRIVATE
            campello_widgets
            campello_ui
            GTest::gtest_main
        )
        if(WIN32)
            add_custom_command(TARGET campello_ui_tests POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    $<TARGET_FILE:campello_widgets>
                    $<TARGET_FILE:campello_ui>
                    $<TARGET_FILE:campello_gpu>
                    $<TARGET_FILE:campello_image>
                    $<TARGET_FILE_DIR:campello_ui_tests>
            )
        endif()
        include(GoogleTest)
        gtest_discover_tests(campello_ui_tests DISCOVERY_MODE PRE_TEST)
    endif()
endif()
