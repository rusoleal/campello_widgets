# Parameterized abstract-contract test suite — runs identical assertions
# against every concrete DesignSystem implementation. Needs campello_ui,
# campello_material, and campello_cupertino all defined, so this is added
# from the root CMakeLists.txt *after* those three subdirectories (unlike
# tests/CMakeLists.txt, which is processed before they exist).

add_executable(design_system_contract_tests test_design_system_contract.cpp)

target_link_libraries(design_system_contract_tests PRIVATE
    campello_widgets
    campello_ui
    campello_material
    campello_cupertino
    GTest::gtest_main
)

if(WIN32)
    add_custom_command(TARGET design_system_contract_tests POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_FILE:campello_widgets>
            $<TARGET_FILE:campello_ui>
            $<TARGET_FILE:campello_material>
            $<TARGET_FILE:campello_cupertino>
            $<TARGET_FILE:campello_gpu>
            $<TARGET_FILE:campello_image>
            $<TARGET_FILE_DIR:design_system_contract_tests>
    )
endif()

include(GoogleTest)
gtest_discover_tests(design_system_contract_tests DISCOVERY_MODE PRE_TEST)
