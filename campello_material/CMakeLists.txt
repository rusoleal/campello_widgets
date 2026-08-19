# campello_material — Material Design 3 DesignSystem implementation.

file(GLOB_RECURSE CAMPELLO_MATERIAL_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp")

add_library(campello_material ${CAMPELLO_MATERIAL_SOURCES})

target_include_directories(campello_material
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/inc>
        $<INSTALL_INTERFACE:inc>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(campello_material PUBLIC campello_widgets)

if(MSVC)
    target_compile_options(campello_material PRIVATE /W4)
else()
    target_compile_options(campello_material PRIVATE -Wall -Wextra)
endif()

if(BUILD_TESTS)
    file(GLOB_RECURSE CAMPELLO_MATERIAL_TEST_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/tests/*.cpp")
    if(CAMPELLO_MATERIAL_TEST_SOURCES)
        add_executable(campello_material_tests ${CAMPELLO_MATERIAL_TEST_SOURCES})
        target_link_libraries(campello_material_tests PRIVATE
            campello_widgets
            campello_material
            GTest::gtest_main
        )
        if(WIN32)
            add_custom_command(TARGET campello_material_tests POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    $<TARGET_FILE:campello_widgets>
                    $<TARGET_FILE:campello_material>
                    $<TARGET_FILE:campello_gpu>
                    $<TARGET_FILE:campello_image>
                    $<TARGET_FILE_DIR:campello_material_tests>
            )
        endif()
        include(GoogleTest)
        gtest_discover_tests(campello_material_tests DISCOVERY_MODE PRE_TEST)
    endif()
endif()
