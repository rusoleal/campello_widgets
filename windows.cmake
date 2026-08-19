file(GLOB_RECURSE CAMPELLO_WIDGETS_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
)
list(FILTER CAMPELLO_WIDGETS_SOURCES EXCLUDE REGEX ".*/src/(android|macos|ios|linux)/.*")
list(FILTER CAMPELLO_WIDGETS_SOURCES EXCLUDE REGEX ".*/src/testing/.*")
# Windows uses its own D3D12 backend (src/windows/), not Vulkan or Metal.
list(FILTER CAMPELLO_WIDGETS_SOURCES EXCLUDE REGEX ".*/src/gpu/(vulkan|metal)/.*")

add_library(campello_widgets SHARED ${CAMPELLO_WIDGETS_SOURCES})
set_target_properties(campello_widgets PROPERTIES WINDOWS_EXPORT_ALL_SYMBOLS ON)

# WINDOWS_EXPORT_ALL_SYMBOLS only auto-exports function symbols, not data
# symbols (e.g. C++17 `inline static` class members like DebugFlags' flags).
# CAMPELLO_WIDGETS_BUILDING_DLL is checked in debug_flags.hpp to apply an
# explicit dllexport/dllimport so that storage is shared across the DLL
# boundary instead of duplicated per translation unit. PRIVATE: only the
# library's own sources see this, not consumers (tests, examples).
target_compile_definitions(campello_widgets PRIVATE CAMPELLO_WIDGETS_BUILDING_DLL)

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
    PRIVATE
        user32
        gdi32
        dwrite
        d2d1
        dxgi
        d3d11
        d3d12
        winhttp
        dwmapi
        imm32
        mfplat
        mfreadwrite
        mfuuid
        ole32
        propsys
)

target_compile_options(campello_widgets PRIVATE /W4)

# Define Windows platform macro.
# NOMINMAX prevents <windows.h> from defining min()/max() macros, which
# conflict with member variables and function parameters named min/max
# (e.g. Slider::min, Slider::max) in constructor initializer lists.
target_compile_definitions(campello_widgets PUBLIC CAMPELLO_PLATFORM_WINDOWS NOMINMAX)
