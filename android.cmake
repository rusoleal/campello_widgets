file(GLOB_RECURSE CAMPELLO_WIDGETS_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
)
list(FILTER CAMPELLO_WIDGETS_SOURCES EXCLUDE REGEX ".*/src/(macos|ios|windows|linux|gdk|web)/.*")
# src/testing/ is fidelity-testing infrastructure (captureToPng, the offscreen
# GPU capture helpers) that is only compiled into campello_widgets_tests /
# campello_widgets_thematic_fidelity — linking it into the shipped library
# leaves it with unresolved symbols for any non-test executable.
list(FILTER CAMPELLO_WIDGETS_SOURCES EXCLUDE REGEX ".*/src/testing/.*")

add_library(campello_widgets SHARED ${CAMPELLO_WIDGETS_SOURCES})

target_include_directories(campello_widgets
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/inc>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}>
        $<INSTALL_INTERFACE:inc>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)


target_link_libraries(campello_widgets
    PUBLIC
        campello_gpu
        campello_input
        vector_math
        campello_image
        Clipper2
        android
        log
        mediandk
)

target_compile_options(campello_widgets PRIVATE -Wall -Wextra)
