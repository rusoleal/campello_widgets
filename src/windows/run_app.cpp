#include <campello_widgets/windows/run_app.hpp>
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

#include <campello_gpu/device.hpp>
#include <campello_gpu/texture_view.hpp>

#include "d3d_draw_backend.hpp"

#include <windows.h>
#include <windowsx.h>

#include <memory>
#include <chrono>

namespace GPU     = systems::leal::campello_gpu;
namespace Widgets = systems::leal::campello_widgets;

// ---------------------------------------------------------------------------
// Internal state
// ---------------------------------------------------------------------------
namespace {
    Widgets::WidgetRef gRootWidget;
    std::string        gTitle;
    int                gWidth  = 800;
    int                gHeight = 600;
    bool               gResizable = true;
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
    
    // For tracking pointer position
    bool mouse_tracking = false;
};

static WindowState* gWindowState = nullptr;

// ---------------------------------------------------------------------------
// Key code translation
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
                    // Renderer will pick up new size on next frame
                    // D3D backend handles resize via swap chain
                }
            }
            return 0;
        }

        // Mouse events
        case WM_MOUSEMOVE: {
            if (state && state->dispatcher) {
                int x = GET_X_LPARAM(lparam);
                int y = GET_Y_LPARAM(lparam);
                
                if (!state->mouse_tracking) {
                    // Start tracking mouse leave
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
                    { static_cast<float>(x), static_cast<float>(y) },
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
                int x = GET_X_LPARAM(lparam);
                int y = GET_Y_LPARAM(lparam);
                SetCapture(hwnd);
                state->dispatcher->handlePointerEvent({
                    Widgets::PointerEventKind::down,
                    0,
                    { static_cast<float>(x), static_cast<float>(y) },
                    1.0f
                });
            }
            return 0;
        }

        case WM_LBUTTONUP:
        case WM_RBUTTONUP:
        case WM_MBUTTONUP: {
            if (state && state->dispatcher) {
                int x = GET_X_LPARAM(lparam);
                int y = GET_Y_LPARAM(lparam);
                ReleaseCapture();
                state->dispatcher->handlePointerEvent({
                    Widgets::PointerEventKind::up,
                    0,
                    { static_cast<float>(x), static_cast<float>(y) },
                    0.0f
                });
            }
            return 0;
        }

        case WM_MOUSEWHEEL: {
            if (state && state->dispatcher) {
                int x = GET_X_LPARAM(lparam);
                int y = GET_Y_LPARAM(lparam);
                float delta = static_cast<float>(GET_WHEEL_DELTA_WPARAM(wparam)) / WHEEL_DELTA;
                
                Widgets::PointerEvent e;
                e.kind = Widgets::PointerEventKind::scroll;
                e.pointer_id = 0;
                e.position = { static_cast<float>(x), static_cast<float>(y) };
                e.pressure = 0.0f;
                e.scroll_delta_x = 0.0f;
                e.scroll_delta_y = delta * 40.0f;  // Convert to logical pixels
                state->dispatcher->handlePointerEvent(e);
            }
            return 0;
        }

        // Keyboard events
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
// Window creation helper
// ---------------------------------------------------------------------------

static HWND createWindow(HINSTANCE hinstance, int width, int height, const std::string& title, void* user_data)
{
    // Register window class
    WNDCLASSEX wc = {};
    wc.cbSize = sizeof(wc);
    wc.style = CS_HREDRAW | CS_VREDRAW | CS_OWNDC;
    wc.lpfnWndProc = windowProc;
    wc.hInstance = hinstance;
    wc.hIcon = LoadIcon(nullptr, IDI_APPLICATION);
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.lpszClassName = "CampelloWidgetsWindow";
    wc.hIconSm = LoadIcon(nullptr, IDI_APPLICATION);

    if (!RegisterClassEx(&wc)) {
        return nullptr;
    }

    // Calculate window size including borders
    DWORD style = WS_OVERLAPPEDWINDOW;
    if (!gResizable) {
        style &= ~(WS_THICKFRAME | WS_MAXIMIZEBOX);
    }
    
    RECT rect = { 0, 0, width, height };
    AdjustWindowRect(&rect, style, FALSE);

    int window_width = rect.right - rect.left;
    int window_height = rect.bottom - rect.top;

    // Center on screen
    int screen_width = GetSystemMetrics(SM_CXSCREEN);
    int screen_height = GetSystemMetrics(SM_CYSCREEN);
    int x = (screen_width - window_width) / 2;
    int y = (screen_height - window_height) / 2;

    // Create window
    HWND hwnd = CreateWindowEx(
        0,
        "CampelloWidgetsWindow",
        title.c_str(),
        style,
        x, y, window_width, window_height,
        nullptr, nullptr, hinstance, user_data
    );

    return hwnd;
}

// ---------------------------------------------------------------------------
// Safe area helper for Windows
// ---------------------------------------------------------------------------

static void updateSafeAreaInsets(WindowState* state)
{
    if (!state || !state->renderer || !state->hwnd) return;

    // Get client rect (actual content area)
    RECT client_rect;
    GetClientRect(state->hwnd, &client_rect);

    // Get window rect (including borders/title bar)
    RECT window_rect;
    GetWindowRect(state->hwnd, &window_rect);

    // Calculate non-client area (borders, title bar)
    // These are technically outside the content, but for full-screen immersive
    // apps we may want to consider them. For now, Windows has no "safe area"
    // concept like mobile platforms with notches.
    Widgets::EdgeInsets insets = Widgets::EdgeInsets::zero();
    
    // On Windows, we could query for taskbar occlusion if needed
    // For now, no insets are needed on desktop Windows
    
    state->renderer->setViewInsets(insets);
}

// ---------------------------------------------------------------------------
// Main runApp implementation
// ---------------------------------------------------------------------------

namespace systems::leal::campello_widgets
{

int runApp(const std::string& title, int width, int height, WidgetRef root_widget)
{
    return runApp(title, width, height, std::move(root_widget), true);
}

int runApp(const std::string& title, int width, int height, WidgetRef root_widget, bool resizable)
{
    gRootWidget = std::move(root_widget);
    gTitle = title;
    gWidth = width;
    gHeight = height;
    gResizable = resizable;

    HINSTANCE hinstance = GetModuleHandle(nullptr);

    // Create window state
    WindowState state;
    gWindowState = &state;

    // Create window
    state.hwnd = createWindow(hinstance, width, height, title, &state);
    if (!state.hwnd) {
        MessageBox(nullptr, "Failed to create window", "Error", MB_OK | MB_ICONERROR);
        return 1;
    }

    // Create GPU device (pass HWND as platform data)
    state.device = GPU::Device::createDefaultDevice(state.hwnd);
    if (!state.device) {
        MessageBox(nullptr, "Failed to create GPU device", "Error", MB_OK | MB_ICONERROR);
        DestroyWindow(state.hwnd);
        return 1;
    }

    // Create dispatcher and focus manager before mounting
    state.dispatcher = std::make_shared<Widgets::PointerDispatcher>();
    Widgets::PointerDispatcher::setActiveDispatcher(state.dispatcher.get());

    state.focus_manager = std::make_shared<Widgets::FocusManager>();
    Widgets::FocusManager::setActiveManager(state.focus_manager.get());

    state.ticker_scheduler = std::make_unique<Widgets::TickerScheduler>();
    Widgets::TickerScheduler::setActive(state.ticker_scheduler.get());

    // Wrap root widget with MediaQuery
    UINT dpi = GetDpiForWindow(state.hwnd);
    Widgets::MediaQueryData mediaData;
    mediaData.device_pixel_ratio = static_cast<float>(dpi) / 96.0f;
    
    auto wrappedRoot = Widgets::make<Widgets::MediaQuery>(
        mediaData, gRootWidget);

    // Mount widget tree
    state.root_element = wrappedRoot->createElement();
    state.root_element->mount(nullptr);

    auto* roe = state.root_element->findDescendantRenderObjectElement();
    if (!roe) {
        MessageBox(nullptr, "Widget tree produced no RenderObjectElement", "Error", MB_OK | MB_ICONERROR);
        return 1;
    }

    auto render_box = std::dynamic_pointer_cast<Widgets::RenderBox>(
        roe->sharedRenderObject());
    if (!render_box) {
        MessageBox(nullptr, "Root render object is not a RenderBox", "Error", MB_OK | MB_ICONERROR);
        return 1;
    }

    state.dispatcher->setRoot(render_box);

    // Create renderer
    const Widgets::Color bgColor = Widgets::Color::white();
    state.renderer = std::make_shared<Widgets::Renderer>(
        state.device, render_box, bgColor);

    // Create D3D draw backend
    state.draw_backend = std::make_unique<Widgets::D3DDrawBackend>(
        state.device, bgColor, state.hwnd);
    state.renderer->setDrawBackend(std::move(state.draw_backend));

    // Initial safe area setup
    updateSafeAreaInsets(&state);

    // Show window
    ShowWindow(state.hwnd, SW_SHOW);
    UpdateWindow(state.hwnd);

    // Message loop
    bool running = true;
    while (running) {
        MSG msg;
        while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
            if (msg.message == WM_QUIT) {
                running = false;
                break;
            }
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }

        if (!running) break;

        // Get client size
        RECT client_rect;
        GetClientRect(state.hwnd, &client_rect);
        int client_width = client_rect.right - client_rect.left;
        int client_height = client_rect.bottom - client_rect.top;

        if (client_width > 0 && client_height > 0) {
            // Update device pixel ratio from window DPI
            // Standard DPI is 96; DPR = DPI / 96
            UINT dpi = GetDpiForWindow(state.hwnd);
            float dpr = static_cast<float>(dpi) / 96.0f;
            state.renderer->setDevicePixelRatio(dpr);

            // Update draw backend viewport
            if (state.draw_backend) {
                state.draw_backend->setViewport(
                    static_cast<float>(client_width),
                    static_cast<float>(client_height));
            }

            // Get swapchain texture from device
            auto color_view = state.device->getSwapchainTextureView();
            if (color_view) {
                state.renderer->renderFrame(
                    color_view,
                    static_cast<float>(client_width),
                    static_cast<float>(client_height));
            }
        }

        // Tick schedulers
        const auto now_tp = std::chrono::steady_clock::now().time_since_epoch();
        const uint64_t ms = 
            std::chrono::duration_cast<std::chrono::milliseconds>(now_tp).count();
        
        if (auto* d = Widgets::PointerDispatcher::activeDispatcher()) d->tick(ms);
        if (auto* ts = Widgets::TickerScheduler::active()) ts->tick(ms);

        // Small sleep to avoid 100% CPU when idle
        Sleep(1);
    }

    // Cleanup
    Widgets::PointerDispatcher::setActiveDispatcher(nullptr);
    Widgets::FocusManager::setActiveManager(nullptr);
    Widgets::TickerScheduler::setActive(nullptr);

    gWindowState = nullptr;
    return 0;
}

} // namespace systems::leal::campello_widgets
