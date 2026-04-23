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
#include <campello_widgets/ui/frame_scheduler.hpp>
#include <campello_widgets/ui/text_input_manager.hpp>

#include <campello_gpu/device.hpp>
#include <campello_gpu/texture_view.hpp>

#include "d3d_draw_backend.hpp"

#include <windows.h>
#include <windowsx.h>
#include <dwmapi.h>
#include <imm.h>

#include <chrono>
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
    
    // For tracking pointer position
    bool mouse_tracking = false;
};

static WindowState* gWindowState = nullptr;

// ---------------------------------------------------------------------------
// Vsync thread — aligns WM_PAINT to the DWM composition clock
// ---------------------------------------------------------------------------
//
// DwmFlush() blocks the calling thread until the next DWM vsync (~16 ms at
// 60 Hz, ~8 ms at 120 Hz).  Running it on a dedicated thread keeps the main
// message pump free for input while still delivering vsync-gated frames.
//
// Protocol:
//   1. FrameScheduler::scheduleFrame() calls SetEvent(gVsyncEvent).
//   2. The vsync thread wakes from WaitForSingleObject, calls DwmFlush(),
//      then posts InvalidateRect — WM_PAINT fires at the vsync boundary.
//   3. WM_PAINT renders the frame; tickers call scheduleFrame() → repeat.
//
// The auto-reset event coalesces multiple scheduleFrame() calls that arrive
// within the same vsync interval into exactly one WM_PAINT.

static std::atomic<bool> gVsyncRunning{false};
static HANDLE            gVsyncEvent  = nullptr;   // auto-reset
static HANDLE            gVsyncThread = nullptr;

static DWORD WINAPI vsyncThreadProc(LPVOID param)
{
    HWND hwnd = static_cast<HWND>(param);
    while (gVsyncRunning.load(std::memory_order_relaxed))
    {
        // Block until scheduleFrame() signals a dirty frame (or 200 ms to
        // re-check the running flag on shutdown).
        const DWORD result = WaitForSingleObject(gVsyncEvent, 200);
        if (result == WAIT_TIMEOUT) continue;
        if (!gVsyncRunning.load(std::memory_order_relaxed)) break;

        // Align to the next DWM vsync boundary.
        DwmFlush();

        // Post WM_PAINT to the main thread.  InvalidateRect is safe to call
        // from any thread.
        if (gVsyncRunning.load(std::memory_order_relaxed))
            InvalidateRect(hwnd, nullptr, FALSE);
    }
    return 0;
}

// ---------------------------------------------------------------------------
// IME helpers
// ---------------------------------------------------------------------------

static std::string utf16ToUtf8(const wchar_t* wstr, int len)
{
    if (len <= 0 || !wstr) return {};
    int size_needed = WideCharToMultiByte(CP_UTF8, 0, wstr, len, nullptr, 0, nullptr, nullptr);
    if (size_needed <= 0) return {};
    std::string result(size_needed, '\0');
    WideCharToMultiByte(CP_UTF8, 0, wstr, len, result.data(), size_needed, nullptr, nullptr);
    return result;
}

/**
 * @brief Positions the IME candidate window near the current cursor/composition.
 *
 * Called during WM_IME_STARTCOMPOSITION and WM_IME_COMPOSITION so the candidate
 * list appears just below the caret rather than at the top-left of the window.
 */
static void updateImeCompositionWindow(HWND hwnd, Widgets::TextInputManager* tim)
{
    if (!tim || !tim->hasInputTarget()) return;

    auto* controller = tim->activeController();
    if (!controller) return;

    // Get the rect at the cursor position (or composing start if composing)
    int byte_offset = controller->isComposing()
        ? controller->composingStart()
        : controller->selectionEnd();

    auto rect = tim->getCharacterRect(byte_offset);
    if (rect[2] <= 0.0f || rect[3] <= 0.0f) return; // No valid rect

    // Convert from client coordinates to screen coordinates
    POINT pt = { static_cast<LONG>(rect[0]), static_cast<LONG>(rect[1] + rect[3]) };
    ClientToScreen(hwnd, &pt);

    HIMC hIMC = ImmGetContext(hwnd);
    if (!hIMC) return;

    COMPOSITIONFORM cf = {};
    cf.dwStyle = CFS_POINT;
    cf.ptCurrentPos.x = pt.x;
    cf.ptCurrentPos.y = pt.y;
    ImmSetCompositionWindow(hIMC, &cf);

    // Also set the candidate window position (the list of suggestions)
    CANDIDATEFORM cand = {};
    cand.dwIndex = 0;
    cand.dwStyle = CFS_CANDIDATEPOS;
    cand.ptCurrentPos.x = pt.x;
    cand.ptCurrentPos.y = pt.y;
    ImmSetCandidateWindow(hIMC, &cand);

    ImmReleaseContext(hwnd, hIMC);
}

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
                    // Request a repaint so the widget tree lays out at the new size.
                    InvalidateRect(hwnd, nullptr, FALSE);
                }
            }
            return 0;
        }

        case WM_PAINT: {
            PAINTSTRUCT ps;
            BeginPaint(hwnd, &ps);
            if (state && state->renderer && state->device) {
                RECT client_rect;
                GetClientRect(hwnd, &client_rect);
                const int w = client_rect.right  - client_rect.left;
                const int h = client_rect.bottom - client_rect.top;
                if (w > 0 && h > 0) {
                    UINT dpi = GetDpiForWindow(hwnd);
                    state->renderer->setDevicePixelRatio(
                        static_cast<float>(dpi) / 96.0f);
                    auto color_view = state->device->getSwapchainTextureView();
                    if (color_view) {
                        auto now = std::chrono::steady_clock::now();
                        uint64_t now_ms = static_cast<uint64_t>(
                            std::chrono::duration_cast<std::chrono::milliseconds>(
                                now.time_since_epoch()).count());
                        if (auto* d  = Widgets::PointerDispatcher::activeDispatcher()) d->tick(now_ms);
                        if (auto* ts = Widgets::TickerScheduler::active())            ts->tick(now_ms);

                        state->renderer->renderFrame(
                            color_view,
                            static_cast<float>(w),
                            static_cast<float>(h));
                    }
                }
            }
            EndPaint(hwnd, &ps);
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

        // IME composition events
        case WM_IME_STARTCOMPOSITION: {
            if (state && state->text_input_manager) {
                state->text_input_manager->beginComposing();
                updateImeCompositionWindow(hwnd, state->text_input_manager.get());
            }
            return 0;
        }

        case WM_IME_COMPOSITION: {
            if (!state || !state->text_input_manager) return 0;

            HIMC hIMC = ImmGetContext(hwnd);
            if (!hIMC) return 0;

            if (lparam & GCS_COMPSTR) {
                LONG size = ImmGetCompositionStringW(hIMC, GCS_COMPSTR, nullptr, 0);
                if (size > 0) {
                    std::vector<wchar_t> buf(size / sizeof(wchar_t));
                    ImmGetCompositionStringW(hIMC, GCS_COMPSTR, buf.data(), size);
                    std::string utf8 = utf16ToUtf8(buf.data(), static_cast<int>(buf.size()));
                    if (!utf8.empty())
                        state->text_input_manager->updateComposingText(utf8);
                }
            }

            if (lparam & GCS_RESULTSTR) {
                LONG size = ImmGetCompositionStringW(hIMC, GCS_RESULTSTR, nullptr, 0);
                if (size > 0) {
                    std::vector<wchar_t> buf(size / sizeof(wchar_t));
                    ImmGetCompositionStringW(hIMC, GCS_RESULTSTR, buf.data(), size);
                    std::string utf8 = utf16ToUtf8(buf.data(), static_cast<int>(buf.size()));
                    state->text_input_manager->updateComposingText(utf8);
                    state->text_input_manager->commitComposing();
                } else {
                    state->text_input_manager->cancelComposing();
                }
            }

            ImmReleaseContext(hwnd, hIMC);

            // Reposition the candidate window whenever the composition updates
            // (cursor may have moved within the composing text).
            updateImeCompositionWindow(hwnd, state->text_input_manager.get());
            return 0;
        }

        case WM_IME_ENDCOMPOSITION: {
            if (state && state->text_input_manager) {
                if (state->text_input_manager->isComposing())
                    state->text_input_manager->cancelComposing();
            }
            return 0;
        }

        case WM_IME_SETCONTEXT: {
            // lparam == TRUE  → IME window is being activated
            // lparam == FALSE → IME window is being deactivated
            // We don't need special handling here, but we pass it through to
            // DefWindowProc so the system IME UI (candidate window, etc.) is
            // created/destroyed correctly.
            return DefWindowProc(hwnd, msg, wparam, lparam);
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
                // Suppress WM_CHAR while an IME composition is active to avoid
                // duplicate text insertion (the IME delivers text via WM_IME_COMPOSITION).
                if (state->text_input_manager && state->text_input_manager->isComposing())
                    return 0;

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
    // Namespace aliases for use inside this namespace block
    namespace GPU     = ::systems::leal::campello_gpu;
    namespace Widgets = ::systems::leal::campello_widgets;

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

    state.text_input_manager = std::make_unique<Widgets::TextInputManager>();
    Widgets::TextInputManager::setActiveManager(state.text_input_manager.get());

    state.ticker_scheduler = std::make_unique<Widgets::TickerScheduler>();
    Widgets::TickerScheduler::setActive(state.ticker_scheduler.get());

    // Start the vsync thread before mounting the widget tree so that
    // markNeedsPaint() calls during tree construction already reach it.
    gVsyncEvent  = CreateEvent(nullptr, FALSE, FALSE, nullptr);
    gVsyncRunning.store(true, std::memory_order_relaxed);
    gVsyncThread = CreateThread(nullptr, 0, vsyncThreadProc,
                                static_cast<LPVOID>(state.hwnd), 0, nullptr);

    // FrameScheduler signals the vsync thread; it calls DwmFlush() then
    // InvalidateRect so WM_PAINT fires exactly at the vsync boundary.
    Widgets::FrameScheduler::setCallback([] {
        SetEvent(gVsyncEvent);
    });

    // Wrap root widget with MediaQuery
    UINT dpi = GetDpiForWindow(state.hwnd);
    Widgets::MediaQueryData mediaData;
    mediaData.device_pixel_ratio = static_cast<float>(dpi) / 96.0f;
    
    auto wrappedRoot = std::make_shared<Widgets::MediaQuery>(
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

    // Show window — triggers the initial WM_PAINT via the pending InvalidateRect.
    ShowWindow(state.hwnd, SW_SHOW);
    UpdateWindow(state.hwnd);

    // Message loop — GetMessage blocks when the queue is empty, bringing idle
    // CPU to ~0%.  Frames are produced only when FrameScheduler::scheduleFrame()
    // is called (via setState / markNeedsPaint / ticker), which posts InvalidateRect
    // → WM_PAINT.  This mirrors Flutter's on-demand vsync-driven render loop.
    MSG msg;
    while (GetMessage(&msg, nullptr, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    // Stop the vsync thread cleanly before teardown.
    gVsyncRunning.store(false, std::memory_order_relaxed);
    SetEvent(gVsyncEvent);   // unblock WaitForSingleObject
    WaitForSingleObject(gVsyncThread, INFINITE);
    CloseHandle(gVsyncThread);
    CloseHandle(gVsyncEvent);
    gVsyncThread = nullptr;
    gVsyncEvent  = nullptr;

    // Cleanup
    Widgets::PointerDispatcher::setActiveDispatcher(nullptr);
    Widgets::FocusManager::setActiveManager(nullptr);
    Widgets::TextInputManager::setActiveManager(nullptr);
    Widgets::TickerScheduler::setActive(nullptr);

    gWindowState = nullptr;
    return 0;
}

} // namespace systems::leal::campello_widgets
