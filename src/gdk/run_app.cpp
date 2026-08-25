#include <campello_widgets/gdk/run_app.hpp>
#include <campello_widgets/campello_widgets.hpp>
#include <campello_widgets/widgets/element.hpp>
#include <campello_widgets/widgets/render_object_element.hpp>
#include <campello_widgets/ui/renderer.hpp>
#include <campello_widgets/ui/render_box.hpp>
#include <campello_widgets/ui/pointer_event.hpp>
#include <campello_widgets/ui/pointer_dispatcher.hpp>
#include <campello_widgets/ui/key_event.hpp>
#include <campello_widgets/ui/focus_manager.hpp>
#include <campello_widgets/ui/ticker.hpp>
#include <campello_widgets/ui/frame_scheduler.hpp>
#include <campello_widgets/ui/text_input_manager.hpp>
#include <campello_widgets/ui/thread_checker.hpp>

#include <campello_gpu/device.hpp>
#include <campello_gpu/texture_view.hpp>
#include <campello_gpu/constants/pixel_format.hpp>

#include "d3d_draw_backend.hpp"

#include <windows.h>
#include <windowsx.h>
#include <XGameRuntime.h>

#include <chrono>
#include <cstdio>
#include <memory>
#include <atomic>
#include <string>
#include <vector>

// Namespace aliases - using global qualification to work correctly in Unity Build
namespace GPU     = ::systems::leal::campello_gpu;
namespace Widgets = ::systems::leal::campello_widgets;

// ---------------------------------------------------------------------------
// Internal state
// ---------------------------------------------------------------------------
namespace {
    Widgets::WidgetRef gRootWidget;
    std::string        gTitle;
    int                gWidth  = 800;
    int                gHeight = 600;
    bool               gResizable = true;

    // The self-posted message a PLM suspend callback uses to move actual
    // suspend handling onto the main/UI thread — see runApp()'s
    // RegisterAppStateChangeNotification callback and the WM_USER handler
    // in windowProc() below. Matches the pattern in Microsoft's own
    // Xbox-GDK-Samples (e.g. Samples/Graphics/HDR10/Main.cpp).
    constexpr UINT kPlmSuspendMessage = WM_USER;
}

// ---------------------------------------------------------------------------
// Window state
// ---------------------------------------------------------------------------
struct WindowState
{
    HWND                                      hwnd = nullptr;
    std::shared_ptr<GPU::Device>              device;
    std::shared_ptr<Widgets::Renderer>        renderer;
    std::shared_ptr<Widgets::Element>         root_element;
    std::shared_ptr<Widgets::PointerDispatcher> dispatcher;
    std::shared_ptr<Widgets::FocusManager>      focus_manager;
    std::unique_ptr<Widgets::TickerScheduler>   ticker_scheduler;
    std::unique_ptr<Widgets::D3DDrawBackend>    draw_backend;
    std::unique_ptr<Widgets::TextInputManager>  text_input_manager;
    Widgets::MediaQueryData                   media_data;
    Widgets::WidgetRef                        user_root_widget;

    // For tracking pointer position
    bool mouse_tracking = false;

    // PLM (Process Lifecycle Management) suspend/resume — see runApp().
    PAPPSTATE_REGISTRATION plm_registration = nullptr;
    HANDLE                 plm_suspend_complete = nullptr;
    HANDLE                 plm_signal_resume    = nullptr;
};

// No OS-level light/dark theme setting applies here (no desktop registry,
// no Settings app) — Gaming.Desktop.x64/Xbox has no equivalent of the
// desktop backend's HKCU\...\Personalize\AppsUseLightTheme read. A fixed
// default stands in; a game that wants a user-facing theme toggle exposes
// its own in-game settings instead.
static Widgets::Brightness getDefaultBrightness()
{
    return Widgets::Brightness::light;
}

static void rebuildMediaQuery(WindowState* state)
{
    if (!state || !state->root_element) return;
    auto newMediaQuery = std::make_shared<Widgets::MediaQuery>(
        state->media_data, state->user_root_widget);
    state->root_element->update(newMediaQuery);
    Widgets::FrameScheduler::scheduleFrame();
}

static WindowState* gWindowState = nullptr;

// ---------------------------------------------------------------------------
// Frame rendering — called once per main-loop iteration (see runApp()'s
// message loop), not from WM_PAINT. Pacing comes from campello_gpu's own
// swap-chain Present(1, 0) call inside renderFrame() below, which blocks
// until the next vsync — no DwmFlush()/vsync thread needed (see this file's
// header doc comment for why that desktop-backend mechanism doesn't apply
// here).
// ---------------------------------------------------------------------------
static void renderFrame(WindowState* state)
{
    if (!state || !state->renderer || !state->device || !state->hwnd) return;

    RECT client_rect;
    GetClientRect(state->hwnd, &client_rect);
    const int w = client_rect.right  - client_rect.left;
    const int h = client_rect.bottom - client_rect.top;
    if (w <= 0 || h <= 0) return;

    UINT dpi = GetDpiForWindow(state->hwnd);
    state->renderer->setDevicePixelRatio(static_cast<float>(dpi) / 96.0f);

    auto color_view = state->device->getSwapchainTextureView();
    if (!color_view) return;

    // Note: PointerDispatcher/TickerScheduler are ticked inside
    // Renderer::renderFrame() itself — don't duplicate that here.
    state->renderer->renderFrame(color_view, static_cast<float>(w), static_cast<float>(h));
}

// ---------------------------------------------------------------------------
// Key code translation (identical to the desktop Win32 backend)
// ---------------------------------------------------------------------------

static Widgets::KeyCode windowsKeyCodeToKeyCode(WPARAM wparam)
{
    switch (wparam) {
        case 'A': return Widgets::KeyCode::a;
        case 'B': return Widgets::KeyCode::b;
        case 'C': return Widgets::KeyCode::c;
        case 'D': return Widgets::KeyCode::d;
        case 'E': return Widgets::KeyCode::e;
        case 'F': return Widgets::KeyCode::f;
        case 'G': return Widgets::KeyCode::g;
        case 'H': return Widgets::KeyCode::h;
        case 'I': return Widgets::KeyCode::i;
        case 'J': return Widgets::KeyCode::j;
        case 'K': return Widgets::KeyCode::k;
        case 'L': return Widgets::KeyCode::l;
        case 'M': return Widgets::KeyCode::m;
        case 'N': return Widgets::KeyCode::n;
        case 'O': return Widgets::KeyCode::o;
        case 'P': return Widgets::KeyCode::p;
        case 'Q': return Widgets::KeyCode::q;
        case 'R': return Widgets::KeyCode::r;
        case 'S': return Widgets::KeyCode::s;
        case 'T': return Widgets::KeyCode::t;
        case 'U': return Widgets::KeyCode::u;
        case 'V': return Widgets::KeyCode::v;
        case 'W': return Widgets::KeyCode::w;
        case 'X': return Widgets::KeyCode::x;
        case 'Y': return Widgets::KeyCode::y;
        case 'Z': return Widgets::KeyCode::z;
        case '0': return Widgets::KeyCode::digit_0;
        case '1': return Widgets::KeyCode::digit_1;
        case '2': return Widgets::KeyCode::digit_2;
        case '3': return Widgets::KeyCode::digit_3;
        case '4': return Widgets::KeyCode::digit_4;
        case '5': return Widgets::KeyCode::digit_5;
        case '6': return Widgets::KeyCode::digit_6;
        case '7': return Widgets::KeyCode::digit_7;
        case '8': return Widgets::KeyCode::digit_8;
        case '9': return Widgets::KeyCode::digit_9;
        case VK_SPACE:     return Widgets::KeyCode::space;
        case VK_RETURN:    return Widgets::KeyCode::enter;
        case VK_ESCAPE:    return Widgets::KeyCode::escape;
        case VK_BACK:      return Widgets::KeyCode::backspace;
        case VK_TAB:       return Widgets::KeyCode::tab;
        case VK_LEFT:      return Widgets::KeyCode::left;
        case VK_RIGHT:     return Widgets::KeyCode::right;
        case VK_UP:        return Widgets::KeyCode::up;
        case VK_DOWN:      return Widgets::KeyCode::down;
        case VK_HOME:      return Widgets::KeyCode::home;
        case VK_END:       return Widgets::KeyCode::end;
        case VK_PRIOR:     return Widgets::KeyCode::page_up;
        case VK_NEXT:      return Widgets::KeyCode::page_down;
        case VK_DELETE:    return Widgets::KeyCode::delete_forward;
        case VK_SHIFT:     return Widgets::KeyCode::left_shift;
        case VK_CONTROL:   return Widgets::KeyCode::left_ctrl;
        case VK_MENU:      return Widgets::KeyCode::left_alt;
        case VK_CAPITAL:   return Widgets::KeyCode::caps_lock;
        case VK_F1:        return Widgets::KeyCode::f1;
        case VK_F2:        return Widgets::KeyCode::f2;
        case VK_F3:        return Widgets::KeyCode::f3;
        case VK_F4:        return Widgets::KeyCode::f4;
        case VK_F5:        return Widgets::KeyCode::f5;
        case VK_F6:        return Widgets::KeyCode::f6;
        case VK_F7:        return Widgets::KeyCode::f7;
        case VK_F8:        return Widgets::KeyCode::f8;
        case VK_F9:        return Widgets::KeyCode::f9;
        case VK_F10:       return Widgets::KeyCode::f10;
        case VK_F11:       return Widgets::KeyCode::f11;
        case VK_F12:       return Widgets::KeyCode::f12;
        default:           return Widgets::KeyCode::unknown;
    }
}

static uint32_t windowsModifiersToKeyModifiers()
{
    uint32_t mods = Widgets::KeyModifiers::none;
    if (GetKeyState(VK_SHIFT)   & 0x8000) mods |= Widgets::KeyModifiers::shift;
    if (GetKeyState(VK_CONTROL) & 0x8000) mods |= Widgets::KeyModifiers::ctrl;
    if (GetKeyState(VK_MENU)    & 0x8000) mods |= Widgets::KeyModifiers::alt;
    if (GetKeyState(VK_LWIN)    & 0x8000) mods |= Widgets::KeyModifiers::meta;
    if (GetKeyState(VK_RWIN)    & 0x8000) mods |= Widgets::KeyModifiers::meta;
    return mods;
}

// ---------------------------------------------------------------------------
// Window procedure
// ---------------------------------------------------------------------------

// See the desktop Win32 backend's identical helper for why this division
// by DPR is necessary (WM_MOUSEMOVE/etc. report physical pixels; the widget
// tree hit-tests in logical/DIP coordinates).
static float windowDpr(HWND hwnd)
{
    return static_cast<float>(GetDpiForWindow(hwnd)) / 96.0f;
}

static LRESULT CALLBACK windowProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
    WindowState* state = reinterpret_cast<WindowState*>(
        GetWindowLongPtr(hwnd, GWLP_USERDATA));

    switch (msg) {
        case WM_CREATE: {
            CREATESTRUCT* cs = reinterpret_cast<CREATESTRUCT*>(lparam);
            SetWindowLongPtr(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(cs->lpCreateParams));
            return 0;
        }

        case WM_CLOSE:
            PostQuitMessage(0);
            return 0;

        case WM_SIZE: {
            if (state && state->renderer) {
                int width = LOWORD(lparam);
                int height = HIWORD(lparam);
                if (width > 0 && height > 0) {
                    // Update MediaQueryData logical size. No InvalidateRect
                    // here (unlike the desktop backend) -- rendering is
                    // driven by the main loop every iteration, not by
                    // WM_PAINT (see runApp()'s message loop doc comment).
                    Widgets::MediaQueryData newData = state->media_data;
                    newData.logical_size = Widgets::Size{
                        static_cast<float>(width),
                        static_cast<float>(height) };
                    if (newData != state->media_data) {
                        state->media_data = newData;
                        rebuildMediaQuery(state);
                    }
                }
            }
            return 0;
        }

        case WM_PAINT: {
            // No-op beyond satisfying Windows' invalidation bookkeeping --
            // actual rendering happens once per main-loop iteration (see
            // renderFrame()/runApp()), not here.
            PAINTSTRUCT ps;
            BeginPaint(hwnd, &ps);
            EndPaint(hwnd, &ps);
            return 0;
        }

        // PLM suspend notification, self-posted from the
        // RegisterAppStateChangeNotification callback (which runs on an
        // arbitrary GDK-internal thread) so the actual handling happens on
        // the main/UI thread. Matches Microsoft's own Xbox-GDK-Samples
        // pattern (e.g. Samples/Graphics/HDR10/Main.cpp).
        case kPlmSuspendMessage: {
            if (state) {
                // Flush any in-flight GPU work before the OS actually
                // suspends the process -- mirrors the desktop backend's
                // ordering concerns around tearing down a live device (see
                // ImageLoader::shutdown()'s comment in runApp() below),
                // just triggered by PLM instead of process exit.
                if (state->device) state->device->waitForIdle();
                if (state->plm_suspend_complete) SetEvent(state->plm_suspend_complete);
            }
            return 0;
        }

        // Mouse events
        case WM_MOUSEMOVE: {
            if (state && state->dispatcher) {
                const float dpr = windowDpr(hwnd);
                float x = static_cast<float>(GET_X_LPARAM(lparam)) / dpr;
                float y = static_cast<float>(GET_Y_LPARAM(lparam)) / dpr;

                if (!state->mouse_tracking) {
                    TRACKMOUSEEVENT tme = {};
                    tme.cbSize = sizeof(tme);
                    tme.dwFlags = TME_LEAVE;
                    tme.hwndTrack = hwnd;
                    TrackMouseEvent(&tme);
                    state->mouse_tracking = true;
                }

                state->dispatcher->handlePointerEvent({
                    Widgets::PointerEventKind::move,
                    0,  // pointer_id
                    { x, y },
                    1.0f
                });
            }
            return 0;
        }

        case WM_MOUSELEAVE: {
            if (state) state->mouse_tracking = false;
            return 0;
        }

        case WM_LBUTTONDOWN:
        case WM_RBUTTONDOWN:
        case WM_MBUTTONDOWN: {
            if (state && state->dispatcher) {
                const float dpr = windowDpr(hwnd);
                float x = static_cast<float>(GET_X_LPARAM(lparam)) / dpr;
                float y = static_cast<float>(GET_Y_LPARAM(lparam)) / dpr;
                SetCapture(hwnd);
                state->dispatcher->handlePointerEvent({
                    Widgets::PointerEventKind::down,
                    0,
                    { x, y },
                    1.0f
                });
            }
            return 0;
        }

        case WM_LBUTTONUP:
        case WM_RBUTTONUP:
        case WM_MBUTTONUP: {
            if (state && state->dispatcher) {
                const float dpr = windowDpr(hwnd);
                float x = static_cast<float>(GET_X_LPARAM(lparam)) / dpr;
                float y = static_cast<float>(GET_Y_LPARAM(lparam)) / dpr;
                ReleaseCapture();
                state->dispatcher->handlePointerEvent({
                    Widgets::PointerEventKind::up,
                    0,
                    { x, y },
                    0.0f
                });
            }
            return 0;
        }

        case WM_MOUSEWHEEL: {
            if (state && state->dispatcher) {
                const float dpr = windowDpr(hwnd);
                int x = GET_X_LPARAM(lparam);
                int y = GET_Y_LPARAM(lparam);
                float delta = static_cast<float>(GET_WHEEL_DELTA_WPARAM(wparam)) / WHEEL_DELTA;

                Widgets::PointerEvent e;
                e.kind = Widgets::PointerEventKind::scroll;
                e.pointer_id = 0;
                e.position = { static_cast<float>(x) / dpr, static_cast<float>(y) / dpr };
                e.pressure = 0.0f;
                e.scroll_delta_x = 0.0f;
                e.scroll_delta_y = delta * 40.0f;
                state->dispatcher->handlePointerEvent(e);
            }
            return 0;
        }

        // Keyboard events. No IME handling on this target (see this file's
        // header doc comment) -- WM_CHAR delivers text directly, with no
        // composing-suppression check needed since nothing ever starts a
        // composition here.
        case WM_KEYDOWN:
        case WM_SYSKEYDOWN: {
            if (state && state->focus_manager && wparam != VK_SHIFT && wparam != VK_CONTROL && wparam != VK_MENU) {
                Widgets::KeyEvent ke;
                ke.kind = (lparam & (1 << 30)) ? Widgets::KeyEventKind::repeat : Widgets::KeyEventKind::down;
                ke.key_code = windowsKeyCodeToKeyCode(wparam);
                ke.modifiers = windowsModifiersToKeyModifiers();
                ke.character = 0;
                state->focus_manager->handleKeyEvent(ke);
            }
            return 0;
        }

        case WM_KEYUP:
        case WM_SYSKEYUP: {
            if (state && state->focus_manager) {
                Widgets::KeyEvent ke;
                ke.kind = Widgets::KeyEventKind::up;
                ke.key_code = windowsKeyCodeToKeyCode(wparam);
                ke.modifiers = windowsModifiersToKeyModifiers();
                ke.character = 0;
                state->focus_manager->handleKeyEvent(ke);
            }
            return 0;
        }

        case WM_CHAR: {
            if (state && state->focus_manager) {
                Widgets::KeyEvent ke;
                ke.kind = Widgets::KeyEventKind::down;
                ke.key_code = Widgets::KeyCode::unknown;
                ke.modifiers = windowsModifiersToKeyModifiers();
                ke.character = static_cast<uint32_t>(wparam);
                state->focus_manager->handleKeyEvent(ke);
            }
            return 0;
        }

        default:
            return DefWindowProc(hwnd, msg, wparam, lparam);
    }
}

// ---------------------------------------------------------------------------
// Window creation helper (identical to the desktop Win32 backend -- see
// this file's header doc comment for why Gaming.Desktop.x64 still uses a
// normal HWND rather than a CoreWindow: confirmed against Microsoft's own
// GDK sample docs, which initialize with "a handle to a presentation
// window" passed as an HWND)
// ---------------------------------------------------------------------------

static HWND createWindow(HINSTANCE hinstance, int width, int height, const std::string& title, void* user_data)
{
    WNDCLASSEX wc = {};
    wc.cbSize = sizeof(wc);
    wc.style = CS_HREDRAW | CS_VREDRAW | CS_OWNDC;
    wc.lpfnWndProc = windowProc;
    wc.hInstance = hinstance;
    wc.hIcon = LoadIcon(nullptr, IDI_APPLICATION);
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.lpszClassName = "CampelloWidgetsGdkWindow";
    wc.hIconSm = LoadIcon(nullptr, IDI_APPLICATION);

    if (!RegisterClassEx(&wc)) {
        return nullptr;
    }

    DWORD style = WS_OVERLAPPEDWINDOW;
    if (!gResizable) {
        style &= ~(WS_THICKFRAME | WS_MAXIMIZEBOX);
    }

    RECT rect = { 0, 0, width, height };
    AdjustWindowRect(&rect, style, FALSE);

    int window_width = rect.right - rect.left;
    int window_height = rect.bottom - rect.top;

    int screen_width = GetSystemMetrics(SM_CXSCREEN);
    int screen_height = GetSystemMetrics(SM_CYSCREEN);
    int x = (screen_width - window_width) / 2;
    int y = (screen_height - window_height) / 2;

    HWND hwnd = CreateWindowEx(
        0,
        "CampelloWidgetsGdkWindow",
        title.c_str(),
        style,
        x, y, window_width, window_height,
        nullptr, nullptr, hinstance, user_data
    );

    return hwnd;
}

static void updateSafeAreaInsets(WindowState* state)
{
    if (!state || !state->renderer) return;
    // No safe-area concept on this target yet (same as the desktop
    // backend) -- Xbox TV overscan safe areas are a real future
    // consideration but out of scope for Gaming.Desktop.x64.
    state->renderer->setViewInsets(Widgets::EdgeInsets::zero());
}

// ---------------------------------------------------------------------------
// Main runApp implementation
// ---------------------------------------------------------------------------

namespace systems::leal::campello_widgets
{
    namespace GPU     = ::systems::leal::campello_gpu;
    namespace Widgets = ::systems::leal::campello_widgets;

int runApp(const std::string& title, int width, int height, WidgetRef root_widget)
{
    return runApp(title, width, height, std::move(root_widget), true);
}

int runApp(const std::string& title, int width, int height, WidgetRef root_widget, bool resizable)
{
    // As early as possible in the entry point, per Microsoft's own
    // guidance -- everything else below (window/device creation, PLM
    // registration) depends on the Gaming Runtime already being up.
    if (FAILED(XGameRuntimeInitialize())) {
        MessageBox(nullptr, "XGameRuntimeInitialize failed", "Error", MB_OK | MB_ICONERROR);
        return 1;
    }

    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);

    gRootWidget = std::move(root_widget);
    gTitle = title;
    gWidth = width;
    gHeight = height;
    gResizable = resizable;

    HINSTANCE hinstance = GetModuleHandle(nullptr);

    WindowState state;
    gWindowState = &state;

    state.hwnd = createWindow(hinstance, width, height, title, &state);
    if (!state.hwnd) {
        MessageBox(nullptr, "Failed to create window", "Error", MB_OK | MB_ICONERROR);
        XGameRuntimeUninitialize();
        return 1;
    }

    state.device = GPU::Device::createDefaultDevice(state.hwnd);
    if (!state.device) {
        MessageBox(nullptr, "Failed to create GPU device", "Error", MB_OK | MB_ICONERROR);
        DestroyWindow(state.hwnd);
        XGameRuntimeUninitialize();
        return 1;
    }

    state.dispatcher = std::make_shared<Widgets::PointerDispatcher>();
    Widgets::PointerDispatcher::setActiveDispatcher(state.dispatcher.get());

    state.focus_manager = std::make_shared<Widgets::FocusManager>();
    Widgets::FocusManager::setActiveManager(state.focus_manager.get());

    state.text_input_manager = std::make_unique<Widgets::TextInputManager>();
    Widgets::TextInputManager::setActiveManager(state.text_input_manager.get());

    state.ticker_scheduler = std::make_unique<Widgets::TickerScheduler>();
    Widgets::TickerScheduler::setActive(state.ticker_scheduler.get());

    // No vsync-thread/FrameScheduler callback wiring here (unlike the
    // desktop backend) -- the main loop below renders every iteration and
    // relies on campello_gpu's Present(1, 0) to pace to vsync.

    // PLM: suspend/resume. See the WM_USER handler in windowProc() and this
    // file's header doc comment for the deferred-suspend protocol.
    state.plm_suspend_complete = CreateEventEx(nullptr, nullptr, 0, EVENT_MODIFY_STATE | SYNCHRONIZE);
    state.plm_signal_resume    = CreateEventEx(nullptr, nullptr, 0, EVENT_MODIFY_STATE | SYNCHRONIZE);
    if (!state.plm_suspend_complete || !state.plm_signal_resume) {
        MessageBox(nullptr, "Failed to create PLM events", "Error", MB_OK | MB_ICONERROR);
        return 1;
    }

    HWND plmContextHwnd = state.hwnd;
    if (RegisterAppStateChangeNotification([](BOOLEAN quiesced, PVOID context)
        {
            HWND hwnd = reinterpret_cast<HWND>(context);
            if (quiesced) {
                // Runs on a GDK-internal thread -- self-post to the main
                // thread and block here until it's actually handled the
                // suspend (that's how a PLM suspend callback defers the
                // actual OS suspend: exiting this callback signals "ready").
                PostMessage(hwnd, kPlmSuspendMessage, 0, 0);
                WindowState* s = reinterpret_cast<WindowState*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));
                if (s && s->plm_suspend_complete) {
                    ResetEvent(s->plm_suspend_complete);
                    WaitForSingleObject(s->plm_suspend_complete, INFINITE);
                }
            } else {
                WindowState* s = reinterpret_cast<WindowState*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));
                if (s && s->plm_signal_resume) SetEvent(s->plm_signal_resume);
            }
        }, plmContextHwnd, &state.plm_registration))
    {
        MessageBox(nullptr, "RegisterAppStateChangeNotification failed", "Error", MB_OK | MB_ICONERROR);
        return 1;
    }

    UINT dpi = GetDpiForWindow(state.hwnd);
    Widgets::MediaQueryData mediaData;
    mediaData.device_pixel_ratio = static_cast<float>(dpi) / 96.0f;
    mediaData.platform_brightness = getDefaultBrightness();
    RECT client_rect;
    GetClientRect(state.hwnd, &client_rect);
    mediaData.logical_size = Widgets::Size{
        static_cast<float>(client_rect.right - client_rect.left),
        static_cast<float>(client_rect.bottom - client_rect.top) };
    state.media_data = mediaData;
    state.user_root_widget = gRootWidget;

    auto wrappedRoot = std::make_shared<Widgets::MediaQuery>(mediaData, gRootWidget);

    Widgets::ThreadChecker::instance().bindToCurrentThread();

    state.root_element = wrappedRoot->createElement();
    state.root_element->mount(nullptr);

    auto* roe = state.root_element->findDescendantRenderObjectElement();
    if (!roe) {
        MessageBox(nullptr, "Widget tree produced no RenderObjectElement", "Error", MB_OK | MB_ICONERROR);
        return 1;
    }

    auto render_box = std::dynamic_pointer_cast<Widgets::RenderBox>(roe->sharedRenderObject());
    if (!render_box) {
        MessageBox(nullptr, "Root render object is not a RenderBox", "Error", MB_OK | MB_ICONERROR);
        return 1;
    }

    state.dispatcher->setRoot(render_box);

    const Widgets::Color bgColor = Widgets::Color::white();
    state.renderer = std::make_shared<Widgets::Renderer>(state.device, render_box, bgColor);

    state.draw_backend = std::make_unique<Widgets::D3DDrawBackend>(
        state.device, bgColor, GPU::PixelFormat::rgba8unorm);
    state.renderer->setDrawBackend(std::move(state.draw_backend));

    updateSafeAreaInsets(&state);

    ShowWindow(state.hwnd, SW_SHOW);
    UpdateWindow(state.hwnd);

    // Main loop — continuous, not idle-blocked (see this file's header doc
    // comment): PeekMessage drains pending input/window messages without
    // blocking, then every iteration renders and presents a frame.
    // Present(1, 0) inside renderFrame() (via campello_gpu) blocks until
    // the next vsync, which is what actually paces this loop -- there is
    // no separate sleep/wait here.
    MSG msg = {};
    bool running = true;
    while (running) {
        while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
            if (msg.message == WM_QUIT) { running = false; break; }
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
        if (!running) break;

        renderFrame(&state);
    }

    UnregisterAppStateChangeNotification(state.plm_registration);
    if (state.plm_suspend_complete) CloseHandle(state.plm_suspend_complete);
    if (state.plm_signal_resume)    CloseHandle(state.plm_signal_resume);

    Widgets::PointerDispatcher::setActiveDispatcher(nullptr);
    Widgets::FocusManager::setActiveManager(nullptr);
    Widgets::TextInputManager::setActiveManager(nullptr);
    Widgets::TickerScheduler::setActive(nullptr);

    // See the desktop Win32 backend's identical comment: torn down here,
    // while the device is still alive, to avoid ImageCache's static
    // destructor dropping a cached Texture into an already-destroyed D3D12
    // device at true process exit.
    Widgets::ImageLoader::instance().shutdown();
    Widgets::ImageCache::instance().clear();

    gWindowState = nullptr;

    XGameRuntimeUninitialize();

    return 0;
}

} // namespace systems::leal::campello_widgets
