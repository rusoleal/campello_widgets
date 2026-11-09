# ---------------------------------------------------------------------------
# Web (Emscripten + WebGPU). Configure with emcmake:
#
#   source ~/emsdk/emsdk_env.sh
#   emcmake cmake -B build-web -S .
#   cmake --build build-web
#
# Rendering reuses the campello_gpu-based VulkanDrawBackend (src/gpu/vulkan/)
# with the WGSL ports of its shaders (shaders/webgpu/, embedded by
# build_webgpu_shaders.sh); campello_gpu emulates its push constants on
# WebGPU. The platform layer (canvas, browser events, Canvas2D text) is in
# src/web/.
# ---------------------------------------------------------------------------
file(GLOB_RECURSE CAMPELLO_WIDGETS_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
)
list(FILTER CAMPELLO_WIDGETS_SOURCES EXCLUDE REGEX ".*/src/(android|macos|ios|windows|linux|gdk)/.*")
list(FILTER CAMPELLO_WIDGETS_SOURCES EXCLUDE REGEX ".*/src/testing/.*")
list(FILTER CAMPELLO_WIDGETS_SOURCES EXCLUDE REGEX ".*/src/gpu/metal/.*")

add_library(campello_widgets STATIC ${CAMPELLO_WIDGETS_SOURCES})

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
)

target_compile_options(campello_widgets PRIVATE -Wall -Wextra)

# The browser glue (canvas sizing, DOM events, Canvas2D text rasterizer,
# clipboard, fetch) lives in a JS library linked into the final executable.
set(CAMPELLO_WIDGETS_WEB_JS "${CMAKE_CURRENT_SOURCE_DIR}/src/web/campello_widgets_web.js")
target_link_options(campello_widgets INTERFACE "--js-library=${CAMPELLO_WIDGETS_WEB_JS}")
set_property(TARGET campello_widgets APPEND PROPERTY INTERFACE_LINK_DEPENDS ${CAMPELLO_WIDGETS_WEB_JS})
