# campello_cupertino — Apple HIG / Cupertino DesignSystem implementation.

file(GLOB_RECURSE CAMPELLO_CUPERTINO_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp")

add_library(campello_cupertino ${CAMPELLO_CUPERTINO_SOURCES})

target_include_directories(campello_cupertino
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/inc>
        $<INSTALL_INTERFACE:inc>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(campello_cupertino PUBLIC campello_widgets)

if(MSVC)
    target_compile_options(campello_cupertino PRIVATE /W4)
else()
    target_compile_options(campello_cupertino PRIVATE -Wall -Wextra)
endif()

if(BUILD_TESTS)
    file(GLOB_RECURSE CAMPELLO_CUPERTINO_TEST_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/tests/*.cpp")
    if(CAMPELLO_CUPERTINO_TEST_SOURCES)
        add_executable(campello_cupertino_tests ${CAMPELLO_CUPERTINO_TEST_SOURCES})
        target_link_libraries(campello_cupertino_tests PRIVATE
            campello_widgets
            campello_cupertino
            GTest::gtest_main
        )
        if(WIN32)
            add_custom_command(TARGET campello_cupertino_tests POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    $<TARGET_FILE:campello_widgets>
                    $<TARGET_FILE:campello_cupertino>
                    $<TARGET_FILE:campello_gpu>
                    $<TARGET_FILE:campello_image>
                    $<TARGET_FILE_DIR:campello_cupertino_tests>
            )
        endif()
        include(GoogleTest)
        gtest_discover_tests(campello_cupertino_tests DISCOVERY_MODE PRE_TEST)
    endif()
endif()
