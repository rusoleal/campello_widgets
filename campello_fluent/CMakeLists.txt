# campello_fluent — Microsoft Fluent 2 DesignSystem implementation.

file(GLOB_RECURSE CAMPELLO_FLUENT_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp")

add_library(campello_fluent ${CAMPELLO_FLUENT_SOURCES})

target_include_directories(campello_fluent
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/inc>
        $<INSTALL_INTERFACE:inc>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(campello_fluent PUBLIC campello_widgets)

if(MSVC)
    target_compile_options(campello_fluent PRIVATE /W4)
else()
    target_compile_options(campello_fluent PRIVATE -Wall -Wextra)
endif()

if(BUILD_TESTS)
    file(GLOB_RECURSE CAMPELLO_FLUENT_TEST_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/tests/*.cpp")
    if(CAMPELLO_FLUENT_TEST_SOURCES)
        add_executable(campello_fluent_tests ${CAMPELLO_FLUENT_TEST_SOURCES})
        target_link_libraries(campello_fluent_tests PRIVATE
            campello_widgets
            campello_fluent
            GTest::gtest_main
        )
        if(WIN32)
            add_custom_command(TARGET campello_fluent_tests POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    $<TARGET_FILE:campello_widgets>
                    $<TARGET_FILE:campello_fluent>
                    $<TARGET_FILE:campello_gpu>
                    $<TARGET_FILE:campello_image>
                    $<TARGET_FILE_DIR:campello_fluent_tests>
            )
        endif()
        include(GoogleTest)
        gtest_discover_tests(campello_fluent_tests DISCOVERY_MODE PRE_TEST)
    endif()
endif()
