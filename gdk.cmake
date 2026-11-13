# ---------------------------------------------------------------------------
# GDK root discovery
#
# When built as a FetchContent subdirectory of campello_editor's superproject,
# CAMPELLO_GDK_INCLUDE_DIR/CAMPELLO_GDK_LIB_DIR are already set by that
# project's own cmake/platforms/GDK.cmake (directory-scoped variables are
# inherited by add_subdirectory()'d children) -- skip rediscovering them in
# that case. When this repo builds standalone (its own CI, or any other
# consumer), do the same GameDKCoreLatest/GRDKLatest lookup here so it's
# self-sufficient. Kept in sync with campello_editor's copy of this logic.
# ---------------------------------------------------------------------------
if(NOT CAMPELLO_GDK_INCLUDE_DIR)
    if(NOT CAMPELLO_GDK_ROOT)
        if(DEFINED ENV{GameDKCoreLatest})
            set(CAMPELLO_GDK_ROOT "$ENV{GameDKCoreLatest}")
        elseif(DEFINED ENV{GRDKLatest})
            set(CAMPELLO_GDK_ROOT "$ENV{GRDKLatest}")
        endif()
    endif()

    if(NOT CAMPELLO_GDK_ROOT)
        message(FATAL_ERROR
            "CAMPELLO_GDK_GAMING_DESKTOP is ON but no Microsoft GDK installation "
            "was found. Install the Microsoft GDK "
            "(https://github.com/microsoft/GDK, e.g. `winget install "
            "Microsoft.Gaming.GDK`) so the GameDKCoreLatest environment "
            "variable is set, or configure with -DCAMPELLO_GDK_ROOT=<path> "
            "explicitly.")
    endif()

    file(TO_CMAKE_PATH "${CAMPELLO_GDK_ROOT}" CAMPELLO_GDK_ROOT)
    if(NOT CAMPELLO_GDK_ROOT MATCHES "/$")
        set(CAMPELLO_GDK_ROOT "${CAMPELLO_GDK_ROOT}/")
    endif()

    set(CAMPELLO_GDK_INCLUDE_DIR "${CAMPELLO_GDK_ROOT}windows/include")
    set(CAMPELLO_GDK_LIB_DIR     "${CAMPELLO_GDK_ROOT}windows/lib/x64")

    if(NOT EXISTS "${CAMPELLO_GDK_INCLUDE_DIR}")
        message(FATAL_ERROR
            "Resolved GDK root '${CAMPELLO_GDK_ROOT}' but "
            "'${CAMPELLO_GDK_INCLUDE_DIR}' doesn't exist -- either an older "
            "GDK layout is installed (pre \"new layout\" migration) or "
            "CAMPELLO_GDK_ROOT/GameDKCoreLatest points somewhere unexpected.")
    endif()

    include_directories(${CAMPELLO_GDK_INCLUDE_DIR})
    link_directories(${CAMPELLO_GDK_LIB_DIR})
endif()

file(GLOB_RECURSE CAMPELLO_WIDGETS_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
)
list(FILTER CAMPELLO_WIDGETS_SOURCES EXCLUDE REGEX ".*/src/(android|macos|ios|linux|windows|web)/.*")
list(FILTER CAMPELLO_WIDGETS_SOURCES EXCLUDE REGEX ".*/src/testing/.*")
# Same reasoning as windows.cmake: this target uses its own D3D12 backend
# (src/gdk/, src/windows/d3d_draw_backend.* below), not Vulkan or Metal.
list(FILTER CAMPELLO_WIDGETS_SOURCES EXCLUDE REGEX ".*/src/gpu/(vulkan|metal)/.*")

# src/gdk/run_app.cpp is the only GDK-specific source today (see TODO.md
# 4.5.3). Everything else GDK needs is the same Win32 code windows.cmake
# builds, compiled once more here: Gaming.Desktop.x64 is an ordinary Windows
# desktop process (WINAPI_FAMILY_DESKTOP_APP, the default), so user32/gdi32,
# the clipboard, GDI text measurement and Media Foundation are all available.
#   - d3d_draw_backend.cpp/.hpp: no HWND coupling (CreateCompatibleDC is
#     passed nullptr, not a window DC).
#   - platform_menu_delegate.cpp: a no-op stub today.
#   - clipboard.cpp / http_client.cpp: plain Win32 clipboard/WinHTTP calls.
#   - video_player_controller.cpp: src/ui/render_video_player.cpp (always
#     compiled) calls into VideoPlayerController, so omitting it would be an
#     undefined-symbol link error.
list(APPEND CAMPELLO_WIDGETS_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/src/windows/d3d_draw_backend.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/windows/clipboard.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/windows/http_client.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/windows/platform_menu_delegate.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/windows/video_player_controller.cpp"
)

add_library(campello_widgets SHARED ${CAMPELLO_WIDGETS_SOURCES})
set_target_properties(campello_widgets PROPERTIES WINDOWS_EXPORT_ALL_SYMBOLS ON)

# See windows.cmake's identical comment for why this define is needed.
target_compile_definitions(campello_widgets PRIVATE CAMPELLO_WIDGETS_BUILDING_DLL)

# No WINAPI_FAMILY override. This target used to set
# WINAPI_FAMILY=WINAPI_FAMILY_GAMES, which is the Xbox console partition: it
# hides desktop APIs this code needs (GDI text, the clipboard, GetKeyState,
# GetDpiForWindow, TrackMouseEvent, ...) and the GDK job never compiled.
# An Xbox (Gaming.Xbox.*) target would need its own sources, not this file.

target_include_directories(campello_widgets
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/inc>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}>
        $<INSTALL_INTERFACE:inc>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${CMAKE_CURRENT_SOURCE_DIR}/src/windows
)

# The same system libraries as windows.cmake (a Gaming.Desktop.x64 title is
# a desktop process), plus the GDK's xgameruntime (XGameRuntimeInitialize)
# and appnotify (RegisterAppStateChangeNotification, used for PLM in
# src/gdk/run_app.cpp). xgameplatform.lib is the console replacement for
# user32/gdi32 and isn't linked here.
target_link_libraries(campello_widgets
    PUBLIC
        campello_gpu
        campello_input
        vector_math
        campello_image
        Clipper2
    PRIVATE
        xgameruntime
        appnotify
        user32
        gdi32
        dwrite
        d2d1
        d3d11
        d3d12
        dxgi
        winhttp
        ole32
        propsys
        mfplat
        mfreadwrite
        mfuuid
)

target_compile_options(campello_widgets PRIVATE /W4)

# Reuses the same CAMPELLO_PLATFORM_WINDOWS macro as windows.cmake -- this
# target is still fundamentally the Win32-API-surface backend (Gaming.Desktop.x64 is a
# desktop process), so code gated on "is this Windows" (e.g.
# d3d_draw_backend.cpp) should still see itself as Windows. PLATFORM_GDK
# (from campello_editor's cmake/platforms/GDK.cmake) is the finer-grained
# flag for GDK-specific behavior differences (this file's own
# src/gdk/run_app.cpp doesn't need it -- it's only ever compiled for GDK
# anyway, per this file's own source-list filtering above).
target_compile_definitions(campello_widgets PUBLIC CAMPELLO_PLATFORM_WINDOWS NOMINMAX)
