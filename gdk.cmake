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
list(FILTER CAMPELLO_WIDGETS_SOURCES EXCLUDE REGEX ".*/src/(android|macos|ios|linux|windows)/.*")
list(FILTER CAMPELLO_WIDGETS_SOURCES EXCLUDE REGEX ".*/src/testing/.*")
# Same reasoning as windows.cmake: this target uses its own D3D12 backend
# (src/gdk/, src/windows/d3d_draw_backend.* below), not Vulkan or Metal.
list(FILTER CAMPELLO_WIDGETS_SOURCES EXCLUDE REGEX ".*/src/gpu/(vulkan|metal)/.*")

# src/gdk/run_app.cpp is the only GDK-specific source today (see TODO.md
# 4.5.3) -- everything else GDK needs is the same Win32-API-surface code
# windows.cmake builds, just compiled once more here under
# WINAPI_FAMILY_GAMES (see cmake/platforms/GDK.cmake in campello_editor,
# which sets that compile definition for this whole configure). These four
# specifically were checked before including them:
#   - d3d_draw_backend.cpp/.hpp: confirmed no HWND coupling -- its only
#     HFONT-related Win32 call, CreateCompatibleDC, is already passed
#     nullptr, not a window DC.
#   - platform_menu_delegate.cpp: a real no-op stub today (setMenus()/
#     clearMenus() are empty, no Win32 menu API calls at all) -- zero risk.
#   - clipboard.cpp / http_client.cpp: plain Win32 clipboard/WinHTTP calls,
#     same category as everything already confirmed for run_app.cpp itself.
# video_player_controller.cpp is included too -- src/ui/render_video_player.cpp
# (cross-platform, always compiled) calls into VideoPlayerController, so
# omitting the Windows implementation here would be a guaranteed undefined-
# symbol link error, not a safe scope cut. Its Media Foundation API surface
# (mfapi.h/mfreadwrite.h) is NOT verified against the WINAPI_FAMILY_GAMES
# partition, unlike the other three above -- this is exactly the kind of
# "surfaces an incompatible call" discovery TODO.md 4.5.1's last item
# (needs a real GDK toolchain build) is for, not something guessable from
# here.
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

# Restricts this target's own view of <windows.h> et al to the GAMES
# partition. Set directly here (not left to a superproject) so a standalone
# build of this repo is self-sufficient -- see the GDK root discovery block
# above for the same reasoning. Harmless to set twice if campello_editor's
# GDK.cmake also sets it at the directory level (identical macro/value).
target_compile_definitions(campello_widgets PRIVATE WINAPI_FAMILY=WINAPI_FAMILY_GAMES)

target_include_directories(campello_widgets
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/inc>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}>
        $<INSTALL_INTERFACE:inc>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${CMAKE_CURRENT_SOURCE_DIR}/src/windows
)

# Deliberately drops dwmapi/imm32 from windows.cmake's list: no DWM
# composition or IME model applies under WINAPI_FAMILY_GAMES (see
# src/gdk/run_app.cpp's header doc comment). Also deliberately drops
# user32/gdi32: xgameplatform.lib is Microsoft's own umbrella replacement
# for exactly those desktop-shell libs (confirmed against
# learn.microsoft.com -- "you are linking against xgameplatform.lib rather
# than against the standard Win32 desktop libs (kernel32, user32, gdi32,
# ...)"), so CreateWindowEx/GetDpiForWindow/GDI calls in
# src/gdk/run_app.cpp and d3d_draw_backend.cpp resolve from there instead;
# linking user32/gdi32 directly here as well would defeat the point of the
# GDK.cmake-level bare /NODEFAULTLIB (see campello_editor's
# cmake/platforms/GDK.cmake). dxgi/d3d11/d3d12/winhttp/ole32/propsys/
# mfplat/mfreadwrite/mfuuid are NOT part of that desktop-shell family --
# lower-level system libs GDK samples link directly alongside
# xgameplatform.lib -- kept as-is. xgameplatform/xgameruntime themselves
# come from the GDK's own lib dir, already added via link_directories() in
# campello_editor's cmake/platforms/GDK.cmake (link_directories() scope
# extends to this add_subdirectory()'d target).
target_link_libraries(campello_widgets
    PUBLIC
        campello_gpu
        campello_input
        vector_math
        campello_image
        Clipper2
    PRIVATE
        xgameplatform
        xgameruntime
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
# target is still fundamentally the Win32-API-surface backend, just under a
# restricted partition, so code gated on "is this Windows" (e.g.
# d3d_draw_backend.cpp) should still see itself as Windows. PLATFORM_GDK
# (from campello_editor's cmake/platforms/GDK.cmake) is the finer-grained
# flag for GDK-specific behavior differences (this file's own
# src/gdk/run_app.cpp doesn't need it -- it's only ever compiled for GDK
# anyway, per this file's own source-list filtering above).
target_compile_definitions(campello_widgets PUBLIC CAMPELLO_PLATFORM_WINDOWS NOMINMAX)
