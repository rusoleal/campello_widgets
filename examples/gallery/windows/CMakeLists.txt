cmake_minimum_required(VERSION 3.22.1)
project(campello_widgets_gallery)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT TARGET campello_widgets)
    add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../../..
                     ${CMAKE_BINARY_DIR}/campello_widgets)
endif()

add_executable(campello_widgets_gallery
    main.cpp
    ../gallery_app.cpp
)

target_include_directories(campello_widgets_gallery PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/..)

target_compile_definitions(campello_widgets_gallery PRIVATE NOMINMAX)

target_link_libraries(campello_widgets_gallery PRIVATE campello_widgets)

set_target_properties(campello_widgets_gallery PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
)

# campello_gpu / campello_image build as separate FetchContent subprojects,
# so their DLLs land under _deps/*-build/ instead of next to this .exe.
# Copy every runtime DLL dependency alongside it so Windows can find them.
#
# This is deliberately an OUTPUT-based custom command (with the DLL list also
# in DEPENDS), not a POST_BUILD command on campello_widgets_gallery directly:
# a POST_BUILD command only re-runs when that target's OWN build step runs,
# which the build system can skip when only a dependency DLL's *contents*
# changed but the executable's own link inputs (e.g. campello_gpu's import
# library / exported symbols) didn't — silently leaving a stale DLL next to
# the exe. Listing the DLLs in DEPENDS makes this command track their
# timestamps directly, so it reruns whenever any of them rebuild.
add_custom_command(
    OUTPUT  ${CMAKE_BINARY_DIR}/campello_widgets_gallery_runtime_dlls.stamp
    # Unlike the old POST_BUILD command (which ran after the linker had
    # already created the exe's output directory), this custom command has
    # no guarantee that directory exists yet — e.g. a from-scratch multi-config
    # (Visual Studio/MSBuild) Release configure, where nothing else has
    # touched $<TARGET_FILE_DIR:...>/Release yet. copy_if_different fails if
    # the destination directory is missing, so create it first.
    COMMAND ${CMAKE_COMMAND} -E make_directory
        $<TARGET_FILE_DIR:campello_widgets_gallery>
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        $<TARGET_RUNTIME_DLLS:campello_widgets_gallery>
        $<TARGET_FILE_DIR:campello_widgets_gallery>
    COMMAND ${CMAKE_COMMAND} -E touch ${CMAKE_BINARY_DIR}/campello_widgets_gallery_runtime_dlls.stamp
    DEPENDS $<TARGET_RUNTIME_DLLS:campello_widgets_gallery>
    COMMAND_EXPAND_LISTS
    VERBATIM
)
add_custom_target(campello_widgets_gallery_copy_runtime_dlls ALL
    DEPENDS ${CMAKE_BINARY_DIR}/campello_widgets_gallery_runtime_dlls.stamp)
# ALL alone only pulls this into a default/whole-project build; run.bat
# builds --target campello_widgets_gallery specifically, so without this
# explicit edge the copy step would silently stop running again.
add_dependencies(campello_widgets_gallery campello_widgets_gallery_copy_runtime_dlls)
# The DEPENDS file list above (resolved from $<TARGET_RUNTIME_DLLS:...>) tells
# CMake to rerun this command whenever those DLLs' timestamps change, but
# doesn't by itself guarantee they're BUILT before this command first runs —
# on a from-scratch multi-config (MSBuild) build the DLL-producing projects
# and this custom-command project can be scheduled in parallel, and the copy
# fails with "No such file or directory" if it runs first. Explicit target
# dependencies force the correct build order.
if(TARGET campello_widgets)
    add_dependencies(campello_widgets_gallery_copy_runtime_dlls campello_widgets)
endif()
if(TARGET campello_gpu)
    add_dependencies(campello_widgets_gallery_copy_runtime_dlls campello_gpu)
endif()
if(TARGET campello_image)
    add_dependencies(campello_widgets_gallery_copy_runtime_dlls campello_image)
endif()
