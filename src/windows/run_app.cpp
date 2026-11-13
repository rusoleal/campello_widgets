#include <campello_widgets/windows/run_app.hpp>
#include <campello_widgets/campello_widgets.hpp>
#include <campello_widgets/widgets/element.hpp>
#include <campello_widgets/widgets/render_object_element.hpp>
#include <campello_widgets/widgets/window.hpp>
#include <campello_widgets/ui/renderer.hpp>
#include <campello_widgets/ui/render_box.hpp>
#include <campello_widgets/ui/view.hpp>
#include <campello_widgets/ui/ticker.hpp>
#include <campello_widgets/ui/thread_checker.hpp>
#include <campello_widgets/ui/platform_windowing.hpp>

#include <campello_gpu/device.hpp>

#include "win32_host.hpp"

#include <windows.h>
#include <dbghelp.h>
#pragma comment(lib, "dbghelp.lib")

#include <atomic>
#include <cstdio>
#include <memory>
#include <string>

namespace systems::leal::campello_widgets
{
    namespace GPU = ::systems::leal::campello_gpu;

namespace
{
    Brightness getSystemBrightness()
    {
        HKEY key;
        DWORD value = 1;  // light
        DWORD size  = sizeof(value);
        if (RegOpenKeyExW(HKEY_CURRENT_USER,
                          L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize",
                          0, KEY_READ, &key) == ERROR_SUCCESS)
        {
            RegQueryValueExW(key, L"AppsUseLightTheme", nullptr, nullptr,
                             reinterpret_cast<LPBYTE>(&value), &size);
            RegCloseKey(key);
        }
        return value == 0 ? Brightness::dark : Brightness::light;
    }

    // Any window other than the hidden main one still on screen?
    bool anyVisibleWindow()
    {
        for (win32::Win32Window* w : win32::Win32Window::all())
            if (w->hwnd() && IsWindowVisible(w->hwnd())) return true;
        return false;
    }
}

// Best-effort crash diagnostic: prints the exception code/address to stderr
// (visible in redirected logs, unlike the crash dialog / Event Viewer entry
// alone) and writes a full minidump — via dbghelp.dll's MiniDumpWriteDump,
// which ships with Windows, no external debugger needed — before the
// default handler runs. Does NOT catch STATUS_FATAL_APP_EXIT (0xC000041D) —
// fail-fast exceptions are deliberately unfilterable.
static LONG WINAPI crashDiagnosticFilter(EXCEPTION_POINTERS* info)
{
    // Multiple threads can fault around the same time (e.g. a shared-state
    // race). Only the first one through writes the dump/log; the rest just
    // block here so they don't stomp the same dump file or interleave stderr.
    static std::atomic<bool> dumping{false};
    bool expected = false;
    if (!dumping.compare_exchange_strong(expected, true))
    {
        Sleep(INFINITE);
    }

    std::fprintf(stderr, "[CRASH] exception code=0x%08lX address=%p thread=%lu\n",
        info->ExceptionRecord->ExceptionCode,
        info->ExceptionRecord->ExceptionAddress,
        GetCurrentThreadId());
    if (info->ExceptionRecord->ExceptionCode == EXCEPTION_ACCESS_VIOLATION &&
        info->ExceptionRecord->NumberParameters >= 2)
    {
        std::fprintf(stderr, "[CRASH] access violation %s address 0x%p\n",
            info->ExceptionRecord->ExceptionInformation[0] ? "writing to" : "reading from",
            reinterpret_cast<void*>(info->ExceptionRecord->ExceptionInformation[1]));
    }
    std::fflush(stderr);

    HANDLE file = CreateFileA("C:\\Users\\rusol\\source\\repos\\campello_widgets\\crash_dumps\\manual_crash.dmp",
        GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file != INVALID_HANDLE_VALUE)
    {
        MINIDUMP_EXCEPTION_INFORMATION mei{};
        mei.ThreadId          = GetCurrentThreadId();
        mei.ExceptionPointers = info;
        mei.ClientPointers    = FALSE;

        BOOL ok = MiniDumpWriteDump(
            GetCurrentProcess(), GetCurrentProcessId(), file,
            static_cast<MINIDUMP_TYPE>(MiniDumpWithFullMemory | MiniDumpWithHandleData | MiniDumpWithThreadInfo),
            &mei, nullptr, nullptr);
        std::fprintf(stderr, "[CRASH] MiniDumpWriteDump %s\n", ok ? "succeeded" : "FAILED");
        std::fflush(stderr);
        CloseHandle(file);
    }
    else
    {
        std::fprintf(stderr, "[CRASH] could not open dump file for writing, GetLastError=%lu\n", GetLastError());
        std::fflush(stderr);
    }

    return EXCEPTION_CONTINUE_SEARCH;
}

int runApp(const std::string& title, int width, int height, WidgetRef root_widget)
{
    return runApp(title, width, height, std::move(root_widget), true);
}

int runApp(const std::string& title, int width, int height, WidgetRef root_widget, bool resizable)
{
    // Must run before any window exists. Without it the process is
    // DPI-unaware: Windows bitmap-stretches its output on high-DPI monitors
    // (blurry text) and GetDpiForWindow() always reports 96.
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
    SetUnhandledExceptionFilter(crashDiagnosticFilter);

    // Bind the UI thread before any widget tree mutation.
    ThreadChecker::instance().bindToCurrentThread();

    // One device for every window, created without a window: each window
    // presents through its own campello_gpu Surface (Phase 17), so the main
    // window and secondary windows take the same path.
    auto device = GPU::Device::createDefaultDevice(nullptr);
    if (!device)
    {
        MessageBoxW(nullptr, L"Failed to create GPU device", L"Error", MB_OK | MB_ICONERROR);
        return 1;
    }
    if (!win32::initHost(device))
    {
        MessageBoxW(nullptr, L"Failed to initialise the window host", L"Error", MB_OK | MB_ICONERROR);
        return 1;
    }

    // The main window's View (Phase 17): its pointer dispatcher, focus and
    // text input. makePrimary() also installs them as the process-wide
    // fallbacks, so code without a window context behaves as before.
    // Declared first among the per-window objects so it is destroyed last.
    auto view = std::make_unique<View>();
    view->makePrimary();

    // Secondary windows (WindowController / Window widget). Installed before
    // the tree mounts, since a State's initState() may open one.
    auto windowing = win32::makePlatformWindowing();
    PlatformWindowing::setInstance(windowing.get());

    auto ticker = std::make_unique<TickerScheduler>();
    TickerScheduler::setActive(ticker.get());

    // `width` x `height` is the client size in logical pixels.
    win32::WindowConfig config;
    config.title     = title;
    config.size      = Size{ static_cast<float>(width), static_cast<float>(height) };
    config.resizable = resizable;
    auto main_window = std::make_unique<win32::Win32Window>(*view, config);
    if (!main_window->hwnd() || !main_window->hasSurface())
    {
        MessageBoxW(nullptr, L"Failed to create the main window", L"Error", MB_OK | MB_ICONERROR);
        main_window.reset();
        PlatformWindowing::setInstance(nullptr);
        win32::shutdownHost();
        return 1;
    }

    // The main window's MediaQuery (secondary windows get theirs from the
    // Window widget).
    MediaQueryData media;
    media.device_pixel_ratio  = main_window->devicePixelRatio();
    media.logical_size        = main_window->logicalSize();
    media.platform_brightness = getSystemBrightness();

    std::shared_ptr<Element> root_element = std::make_shared<MediaQuery>(media, root_widget)->createElement();
    {
        // Elements and render objects created during the initial mount
        // belong to the main window.
        View::Scope scope(view.get());
        root_element->mount(nullptr);
    }

    auto* roe = root_element->findDescendantRenderObjectElement();
    auto render_box = roe ? std::dynamic_pointer_cast<RenderBox>(roe->sharedRenderObject()) : nullptr;
    if (!render_box)
    {
        MessageBoxW(nullptr, L"The widget tree produced no root RenderBox", L"Error", MB_OK | MB_ICONERROR);
        return 1;
    }
    main_window->setRoot(render_box, Color::white());

    auto rebuildMediaQuery = [&] {
        View::Scope scope(view.get());
        root_element->update(std::make_shared<MediaQuery>(media, root_widget));
    };
    main_window->on_metrics_changed = [&] {
        MediaQueryData next = media;
        next.device_pixel_ratio = main_window->devicePixelRatio();
        next.logical_size       = main_window->logicalSize();
        if (next != media) { media = next; rebuildMediaQuery(); }
    };
    main_window->on_settings_changed = [&] {
        const Brightness brightness = getSystemBrightness();
        if (brightness != media.platform_brightness)
        {
            media.platform_brightness = brightness;
            rebuildMediaQuery();
        }
    };

    // Closing the main window: by default the app quits, closing every
    // window; with MainWindowClose::keep_running it hides and the app quits
    // after the last other window closes.
    bool main_hidden = false;
    main_window->on_close = [&] {
        if (WindowController::mainWindowClosePolicy() == WindowController::MainWindowClose::quit_app)
        {
            PostQuitMessage(0);
            return;
        }
        ShowWindow(main_window->hwnd(), SW_HIDE);
        main_hidden = true;
        if (!anyVisibleWindow()) PostQuitMessage(0);
    };
    win32::setWindowDestroyedHook([&] {
        if (main_hidden && !anyVisibleWindow()) PostQuitMessage(0);
    });

    main_window->show(/*activate=*/true);

    // GetMessage blocks while idle (~0% CPU). Frames are produced only when a
    // window asks for one (setState, markNeedsPaint, tickers): the vsync
    // thread then posts a frame message after the next DWM composition.
    MSG msg;
    while (GetMessageW(&msg, nullptr, 0, 0) > 0)
    {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    // ---- Teardown, in dependency order --------------------------------
    win32::setWindowDestroyedHook(nullptr);

    // Close every other window first, while the tree is intact: their
    // on_destroyed callbacks usually point into States (setState to drop the
    // controller), which must still exist when those run.
    for (View* v : View::all())
        if (WindowController* controller = WindowController::forView(v)) controller->destroy();
    win32::drainTasks();
    {
        // The tree first: States dispose (closing the windows they own),
        // Window widgets unmount their hosted subtrees.
        View::Scope scope(view.get());
        root_element->unmount();
    }
    // The windows they closed (WindowController::destroy() is deferred),
    // while the States their on_destroyed callbacks point at still exist.
    win32::drainTasks();
    root_element.reset();
    win32::drainTasks();

    main_window.reset();  // renderer, surface, HWND (before its View)
    PlatformWindowing::setInstance(nullptr);
    windowing.reset();
    TickerScheduler::setActive(nullptr);
    ticker.reset();

    // ImageLoader/ImageCache are function-local statics whose destructors run
    // after main() returns, when the D3D12 device is long gone: release their
    // GPU textures while it is still alive.
    ImageLoader::instance().shutdown();
    ImageCache::instance().clear();

    win32::shutdownHost();
    view.reset();
    device.reset();
    return 0;
}

} // namespace systems::leal::campello_widgets
