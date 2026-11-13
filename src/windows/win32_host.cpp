// Windows host (Phase 17, multi-window): native windows, input routing,
// per-window rendering through a campello_gpu Surface, the vsync thread and
// the UI-thread task queue. See win32_host.hpp.

#include "win32_host.hpp"

#include <campello_widgets/campello_widgets.hpp>
#include <campello_widgets/ui/view.hpp>
#include <campello_widgets/ui/renderer.hpp>
#include <campello_widgets/ui/render_box.hpp>
#include <campello_widgets/ui/pointer_event.hpp>
#include <campello_widgets/ui/pointer_dispatcher.hpp>
#include <campello_widgets/ui/key_event.hpp>
#include <campello_widgets/ui/focus_manager.hpp>
#include <campello_widgets/ui/text_input_manager.hpp>
#include <campello_widgets/ui/hardware_keyboard.hpp>

#include <campello_gpu/device.hpp>
#include <campello_gpu/surface.hpp>
#include <campello_gpu/constants/pixel_format.hpp>

#include "d3d_draw_backend.hpp"

#include <windowsx.h>
#include <dwmapi.h>
#include <imm.h>

#include <algorithm>
#include <atomic>
#include <cstdio>
#include <deque>
#include <mutex>

namespace systems::leal::campello_widgets::win32
{
    namespace GPU = ::systems::leal::campello_gpu;

    namespace
    {
        constexpr wchar_t kWindowClass[] = L"CampelloWidgetsWindow";
        constexpr wchar_t kPopupClass[]  = L"CampelloWidgetsPopup";
        constexpr wchar_t kHostClass[]   = L"CampelloWidgetsHost";

        constexpr UINT WM_APP_FRAME = WM_APP + 1;  // vsync: draw windows that asked for a frame
        constexpr UINT WM_APP_TASK  = WM_APP + 2;  // postTask()

        std::shared_ptr<GPU::Device> g_device;
        HWND                         g_host_hwnd = nullptr;

        // Vsync thread: DwmFlush() blocks until the next DWM composition, so
        // frames are produced on the compositor's clock without blocking the
        // message loop. One thread for all windows; each window only draws
        // when it asked for a frame.
        std::atomic<bool> g_vsync_running{false};
        std::atomic<bool> g_frame_posted{false};
        HANDLE            g_vsync_event  = nullptr;  // auto-reset
        HANDLE            g_vsync_thread = nullptr;

        std::function<void()>             g_window_destroyed_hook;

        std::mutex                        g_task_mutex;
        std::deque<std::function<void()>> g_tasks;

        std::vector<Win32Window*>& registry()
        {
            static std::vector<Win32Window*> windows;
            return windows;
        }

        DWORD WINAPI vsyncThreadProc(LPVOID)
        {
            while (g_vsync_running.load(std::memory_order_acquire))
            {
                if (WaitForSingleObject(g_vsync_event, 200) == WAIT_TIMEOUT) continue;
                if (!g_vsync_running.load(std::memory_order_acquire)) break;
                DwmFlush();
                // One pending frame message at a time; the UI thread clears
                // the flag when it handles it.
                if (!g_frame_posted.exchange(true) && g_host_hwnd)
                    PostMessageW(g_host_hwnd, WM_APP_FRAME, 0, 0);
            }
            return 0;
        }

        void signalVsync()
        {
            if (g_vsync_event) SetEvent(g_vsync_event);
        }

        void drawRequestedFrames()
        {
            g_frame_posted.store(false);
            // A copy: a frame may open or close windows.
            const std::vector<Win32Window*> windows = registry();
            for (Win32Window* w : windows)
                if (Win32Window::isLive(w) && w->needsFrame()) w->drawFrame();
        }

        LRESULT CALLBACK hostProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
        {
            switch (msg)
            {
            case WM_APP_FRAME:
                drawRequestedFrames();
                return 0;
            case WM_APP_TASK:
                drainTasks();
                return 0;
            default:
                return DefWindowProcW(hwnd, msg, wparam, lparam);
            }
        }

        LRESULT CALLBACK windowProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
        {
            if (msg == WM_NCCREATE)
            {
                auto* cs = reinterpret_cast<CREATESTRUCTW*>(lparam);
                SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(cs->lpCreateParams));
            }
            auto* window = reinterpret_cast<Win32Window*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
            if (window && Win32Window::isLive(window))
            {
                window->attachHwnd(hwnd);
                return window->handleMessage(msg, wparam, lparam);
            }
            return DefWindowProcW(hwnd, msg, wparam, lparam);
        }

        std::wstring utf8ToWide(const std::string& s)
        {
            if (s.empty()) return {};
            const int n = MultiByteToWideChar(CP_UTF8, 0, s.data(), (int)s.size(), nullptr, 0);
            std::wstring w(n, L'\0');
            MultiByteToWideChar(CP_UTF8, 0, s.data(), (int)s.size(), w.data(), n);
            return w;
        }

        std::string wideToUtf8(const wchar_t* w, int len)
        {
            if (!w || len <= 0) return {};
            const int n = WideCharToMultiByte(CP_UTF8, 0, w, len, nullptr, 0, nullptr, nullptr);
            if (n <= 0) return {};
            std::string s(n, '\0');
            WideCharToMultiByte(CP_UTF8, 0, w, len, s.data(), n, nullptr, nullptr);
            return s;
        }

        float dpiScale(UINT dpi) { return static_cast<float>(dpi ? dpi : 96) / 96.0f; }

        KeyCode windowsKeyCodeToKeyCode(WPARAM wparam)
        {
            if (wparam >= 'A' && wparam <= 'Z')
                return static_cast<KeyCode>(static_cast<int>(KeyCode::a) + static_cast<int>(wparam - 'A'));
            switch (wparam)
            {
            case '0': return KeyCode::digit_0;
            case '1': return KeyCode::digit_1;
            case '2': return KeyCode::digit_2;
            case '3': return KeyCode::digit_3;
            case '4': return KeyCode::digit_4;
            case '5': return KeyCode::digit_5;
            case '6': return KeyCode::digit_6;
            case '7': return KeyCode::digit_7;
            case '8': return KeyCode::digit_8;
            case '9': return KeyCode::digit_9;
            case VK_SPACE:   return KeyCode::space;
            case VK_RETURN:  return KeyCode::enter;
            case VK_ESCAPE:  return KeyCode::escape;
            case VK_BACK:    return KeyCode::backspace;
            case VK_TAB:     return KeyCode::tab;
            case VK_LEFT:    return KeyCode::left;
            case VK_RIGHT:   return KeyCode::right;
            case VK_UP:      return KeyCode::up;
            case VK_DOWN:    return KeyCode::down;
            case VK_HOME:    return KeyCode::home;
            case VK_END:     return KeyCode::end;
            case VK_PRIOR:   return KeyCode::page_up;
            case VK_NEXT:    return KeyCode::page_down;
            case VK_DELETE:  return KeyCode::delete_forward;
            case VK_SHIFT:   return KeyCode::left_shift;
            case VK_CONTROL: return KeyCode::left_ctrl;
            case VK_MENU:    return KeyCode::left_alt;
            case VK_CAPITAL: return KeyCode::caps_lock;
            case VK_F1:  return KeyCode::f1;
            case VK_F2:  return KeyCode::f2;
            case VK_F3:  return KeyCode::f3;
            case VK_F4:  return KeyCode::f4;
            case VK_F5:  return KeyCode::f5;
            case VK_F6:  return KeyCode::f6;
            case VK_F7:  return KeyCode::f7;
            case VK_F8:  return KeyCode::f8;
            case VK_F9:  return KeyCode::f9;
            case VK_F10: return KeyCode::f10;
            case VK_F11: return KeyCode::f11;
            case VK_F12: return KeyCode::f12;
            default:     return KeyCode::unknown;
            }
        }

        uint32_t currentModifiers()
        {
            uint32_t mods = KeyModifiers::none;
            if (GetKeyState(VK_SHIFT)   & 0x8000) mods |= KeyModifiers::shift;
            if (GetKeyState(VK_CONTROL) & 0x8000) mods |= KeyModifiers::ctrl;
            if (GetKeyState(VK_MENU)    & 0x8000) mods |= KeyModifiers::alt;
            if (GetKeyState(VK_LWIN)    & 0x8000) mods |= KeyModifiers::meta;
            if (GetKeyState(VK_RWIN)    & 0x8000) mods |= KeyModifiers::meta;
            return mods;
        }

        // Positions the IME composition and candidate windows at the caret.
        void updateImeCompositionWindow(HWND hwnd, TextInputManager& tim, float dpr)
        {
            if (!tim.hasInputTarget()) return;
            auto* controller = tim.activeController();
            if (!controller) return;
            const int offset = controller->isComposing() ? controller->composingStart()
                                                         : controller->selectionEnd();
            const auto rect = tim.getCharacterRect(offset);
            if (rect[2] <= 0.0f || rect[3] <= 0.0f) return;

            // The rect is in logical pixels; the IME wants physical client coordinates.
            POINT pt = { static_cast<LONG>(rect[0] * dpr), static_cast<LONG>((rect[1] + rect[3]) * dpr) };
            HIMC himc = ImmGetContext(hwnd);
            if (!himc) return;
            COMPOSITIONFORM cf = {};
            cf.dwStyle      = CFS_POINT;
            cf.ptCurrentPos = pt;
            ImmSetCompositionWindow(himc, &cf);
            CANDIDATEFORM cand = {};
            cand.dwIndex      = 0;
            cand.dwStyle      = CFS_CANDIDATEPOS;
            cand.ptCurrentPos = pt;
            ImmSetCandidateWindow(himc, &cand);
            ImmReleaseContext(hwnd, himc);
        }

        HCURSOR systemCursor(SystemMouseCursor cursor)
        {
            switch (cursor)
            {
            case SystemMouseCursor::pointer:   return LoadCursorA(nullptr, IDC_HAND);
            case SystemMouseCursor::text:      return LoadCursorA(nullptr, IDC_IBEAM);
            case SystemMouseCursor::forbidden: return LoadCursorA(nullptr, IDC_NO);
            case SystemMouseCursor::resize_ns: return LoadCursorA(nullptr, IDC_SIZENS);
            case SystemMouseCursor::resize_ew: return LoadCursorA(nullptr, IDC_SIZEWE);
            case SystemMouseCursor::arrow:
            default:                           return LoadCursorA(nullptr, IDC_ARROW);
            }
        }
    } // namespace

    // ======================================================================
    // App-level host
    // ======================================================================

    bool initHost(std::shared_ptr<GPU::Device> device)
    {
        g_device = std::move(device);
        HINSTANCE inst = GetModuleHandleW(nullptr);

        WNDCLASSEXW wc = {};
        wc.cbSize        = sizeof(wc);
        wc.style         = CS_DBLCLKS;
        wc.lpfnWndProc   = windowProc;
        wc.hInstance     = inst;
        wc.hIcon         = LoadIconA(nullptr, IDI_APPLICATION);
        wc.hIconSm       = LoadIconA(nullptr, IDI_APPLICATION);
        wc.hCursor       = nullptr;  // WM_SETCURSOR picks the window's cursor
        wc.hbrBackground = nullptr;  // the swapchain covers the client area
        wc.lpszClassName = kWindowClass;
        RegisterClassExW(&wc);

        wc.style         = CS_DBLCLKS | CS_DROPSHADOW;  // menus, dropdowns
        wc.lpszClassName = kPopupClass;
        RegisterClassExW(&wc);

        WNDCLASSEXW hc = {};
        hc.cbSize        = sizeof(hc);
        hc.lpfnWndProc   = hostProc;
        hc.hInstance     = inst;
        hc.lpszClassName = kHostClass;
        RegisterClassExW(&hc);
        g_host_hwnd = CreateWindowExW(0, kHostClass, L"", 0, 0, 0, 0, 0, HWND_MESSAGE,
                                      nullptr, inst, nullptr);
        if (!g_host_hwnd) return false;

        g_vsync_event = CreateEventW(nullptr, FALSE, FALSE, nullptr);
        g_vsync_running.store(true, std::memory_order_release);
        g_vsync_thread = CreateThread(nullptr, 0, vsyncThreadProc, nullptr, 0, nullptr);

        // Cursors follow the window being handled (its View is ambient while
        // its events and frames run).
        registerCursorHandler([](SystemMouseCursor cursor) {
            if (Win32Window* w = Win32Window::forView(View::current())) w->setCursor(cursor);
        });
        return true;
    }

    void shutdownHost()
    {
        registerCursorHandler(nullptr);
        if (g_vsync_thread)
        {
            g_vsync_running.store(false, std::memory_order_release);
            SetEvent(g_vsync_event);
            WaitForSingleObject(g_vsync_thread, INFINITE);
            CloseHandle(g_vsync_thread);
            g_vsync_thread = nullptr;
        }
        if (g_vsync_event) { CloseHandle(g_vsync_event); g_vsync_event = nullptr; }
        drainTasks();
        if (g_host_hwnd) { DestroyWindow(g_host_hwnd); g_host_hwnd = nullptr; }
        g_device.reset();
    }

    std::shared_ptr<GPU::Device> device() { return g_device; }

    void postTask(std::function<void()> task)
    {
        if (!task) return;
        {
            std::lock_guard<std::mutex> lock(g_task_mutex);
            g_tasks.push_back(std::move(task));
        }
        if (g_host_hwnd) PostMessageW(g_host_hwnd, WM_APP_TASK, 0, 0);
    }

    void drainTasks()
    {
        while (true)
        {
            std::function<void()> task;
            {
                std::lock_guard<std::mutex> lock(g_task_mutex);
                if (g_tasks.empty()) return;
                task = std::move(g_tasks.front());
                g_tasks.pop_front();
            }
            task();
        }
    }

    float systemScale() { return dpiScale(GetDpiForSystem()); }

    void setWindowDestroyedHook(std::function<void()> hook) { g_window_destroyed_hook = std::move(hook); }

    // ======================================================================
    // Win32Window
    // ======================================================================

    const std::vector<Win32Window*>& Win32Window::all() { return registry(); }

    bool Win32Window::isLive(const Win32Window* window)
    {
        const auto& r = registry();
        return window && std::find(r.begin(), r.end(), window) != r.end();
    }

    Win32Window* Win32Window::forView(const View* view)
    {
        if (!view) return nullptr;
        for (Win32Window* w : registry()) if (&w->view_ == view) return w;
        return nullptr;
    }

    Win32Window* Win32Window::forHwnd(HWND hwnd)
    {
        for (Win32Window* w : registry()) if (w->hwnd_ == hwnd) return w;
        return nullptr;
    }

    Win32Window::Win32Window(View& view, const WindowConfig& config)
        : view_(view), kind_(config.kind), owner_(config.owner)
    {
        registry().push_back(this);
        cursor_ = LoadCursorA(nullptr, IDC_ARROW);

        const bool popup = kind_ == WindowKind::popup;
        DWORD style   = WS_OVERLAPPEDWINDOW;
        DWORD exstyle = 0;
        if (kind_ == WindowKind::dialog)
            style = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_THICKFRAME;
        if (!config.resizable) style &= ~(WS_THICKFRAME | WS_MAXIMIZEBOX);
        if (popup)
        {
            style   = WS_POPUP;
            exstyle = WS_EX_TOOLWINDOW;
        }

        // Size and place in physical pixels at the DPI of where it will
        // appear: the owner's monitor, else the primary one.
        const UINT dpi   = owner_ ? GetDpiForWindow(owner_) : GetDpiForSystem();
        const float scale = dpiScale(dpi);
        RECT rect = { 0, 0, static_cast<LONG>(config.size.width * scale + 0.5f),
                            static_cast<LONG>(config.size.height * scale + 0.5f) };
        AdjustWindowRectExForDpi(&rect, style, FALSE, exstyle, dpi);

        int x = CW_USEDEFAULT, y = CW_USEDEFAULT;
        if (config.position)
        {
            POINT p = { static_cast<LONG>(config.position->x * scale),
                        static_cast<LONG>(config.position->y * scale) };
            // A popup is placed relative to its owner's client area.
            if (popup && owner_) ClientToScreen(owner_, &p);
            x = p.x;
            y = p.y;
            positioned_ = true;
        }

        const std::wstring title = utf8ToWide(config.title);
        hwnd_ = CreateWindowExW(exstyle, popup ? kPopupClass : kWindowClass, title.c_str(), style,
                                x, y, rect.right - rect.left, rect.bottom - rect.top,
                                owner_, nullptr, GetModuleHandleW(nullptr), this);
        if (!hwnd_)
        {
            std::fprintf(stderr, "[campello_widgets] CreateWindowEx failed (%lu)\n", GetLastError());
            return;
        }

        if (g_device) surface_ = g_device->createSurface(hwnd_);
        if (!surface_)
            std::fprintf(stderr, "[campello_widgets] could not create a GPU surface for a window\n");

        view_.setFrameRequestCallback([this] { requestFrame(); });
    }

    Win32Window::~Win32Window()
    {
        view_.setFrameRequestCallback(nullptr);
        if (view_.renderer() == renderer_.get()) view_.setRenderer(nullptr);
        auto& r = registry();
        r.erase(std::remove(r.begin(), r.end(), this), r.end());  // late messages: no-ops
        renderer_.reset();
        surface_.reset();
        if (hwnd_)
        {
            SetWindowLongPtrW(hwnd_, GWLP_USERDATA, 0);
            DestroyWindow(hwnd_);
        }
        postTask([] { if (auto hook = g_window_destroyed_hook) hook(); });
    }

    void Win32Window::setRoot(std::shared_ptr<RenderBox> root, Color background)
    {
        if (view_.renderer() == renderer_.get()) view_.setRenderer(nullptr);
        renderer_.reset();
        root_ = std::move(root);
        view_.pointerDispatcher().setRoot(root_);
        if (!root_ || !g_device) return;

        renderer_ = std::make_shared<Renderer>(g_device, root_, background);
        // Surfaces present in DXGI_FORMAT_R8G8B8A8_UNORM.
        renderer_->setDrawBackend(std::make_unique<D3DDrawBackend>(
            g_device, background, GPU::PixelFormat::rgba8unorm));
        renderer_->setDevicePixelRatio(devicePixelRatio());
        renderer_->setViewInsets(EdgeInsets::zero());  // desktop windows have no safe area
        updateRefreshRate();
        view_.setRenderer(renderer_.get());
        requestFrame();
    }

    void Win32Window::show(bool activate)
    {
        if (!hwnd_) return;
        if (!positioned_)
        {
            // Centre on the owner, or on the work area of the monitor it's on.
            RECT wr = {};
            GetWindowRect(hwnd_, &wr);
            RECT target = {};
            if (owner_ && IsWindowVisible(owner_))
            {
                GetWindowRect(owner_, &target);
            }
            else
            {
                MONITORINFO mi = { sizeof(mi) };
                GetMonitorInfoW(MonitorFromWindow(owner_ ? owner_ : hwnd_, MONITOR_DEFAULTTOPRIMARY), &mi);
                target = mi.rcWork;
            }
            const int w = wr.right - wr.left, h = wr.bottom - wr.top;
            SetWindowPos(hwnd_, nullptr,
                         target.left + ((target.right - target.left) - w) / 2,
                         target.top + ((target.bottom - target.top) - h) / 2,
                         0, 0, SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
            positioned_ = true;
        }
        if (IsIconic(hwnd_)) ShowWindow(hwnd_, SW_RESTORE);
        ShowWindow(hwnd_, activate ? SW_SHOW : SW_SHOWNOACTIVATE);
        if (activate)
        {
            SetForegroundWindow(hwnd_);
            SetActiveWindow(hwnd_);
        }
        requestFrame();
    }

    void Win32Window::setTitle(const std::string& title)
    {
        if (hwnd_) SetWindowTextW(hwnd_, utf8ToWide(title).c_str());
    }

    void Win32Window::setLogicalSize(Size size)
    {
        if (!hwnd_) return;
        const UINT  dpi   = GetDpiForWindow(hwnd_);
        const float scale = dpiScale(dpi);
        RECT rect = { 0, 0, static_cast<LONG>(size.width * scale + 0.5f),
                            static_cast<LONG>(size.height * scale + 0.5f) };
        AdjustWindowRectExForDpi(&rect, static_cast<DWORD>(GetWindowLongPtrW(hwnd_, GWL_STYLE)), FALSE,
                                 static_cast<DWORD>(GetWindowLongPtrW(hwnd_, GWL_EXSTYLE)), dpi);
        SetWindowPos(hwnd_, nullptr, 0, 0, rect.right - rect.left, rect.bottom - rect.top,
                     SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
    }

    float Win32Window::devicePixelRatio() const
    {
        return hwnd_ ? dpiScale(GetDpiForWindow(hwnd_)) : 1.0f;
    }

    Size Win32Window::logicalSize() const
    {
        if (!hwnd_) return {0.0f, 0.0f};
        RECT rc = {};
        GetClientRect(hwnd_, &rc);
        const float dpr = devicePixelRatio();
        return { static_cast<float>(rc.right - rc.left) / dpr, static_cast<float>(rc.bottom - rc.top) / dpr };
    }

    void Win32Window::requestFrame()
    {
        needs_frame_ = true;
        signalVsync();
    }

    void Win32Window::drawFrame(bool force)
    {
        if (!force && !needs_frame_) return;
        needs_frame_ = false;
        if (!renderer_ || !surface_ || in_frame_ || !hwnd_ || IsIconic(hwnd_)) return;

        RECT rc = {};
        GetClientRect(hwnd_, &rc);
        const int w = rc.right - rc.left, h = rc.bottom - rc.top;
        if (w <= 0 || h <= 0) return;

        // The shared buildScope() may rebuild other windows' elements, and
        // even remove this window (a popup closing itself): hold everything
        // the frame uses, and touch `this` again only if it is still alive.
        std::shared_ptr<Renderer>     renderer = renderer_;
        std::shared_ptr<GPU::Surface> surface  = surface_;
        in_frame_ = true;
        {
            View::Scope scope(&view_);
            renderer->setDevicePixelRatio(devicePixelRatio());
            auto package = renderer->buildFrame(static_cast<float>(w), static_cast<float>(h));
            if (package)
            {
                package->surface = surface;
                renderer->rasterFrame(*package);
            }
        }
        if (isLive(this)) in_frame_ = false;
    }

    void Win32Window::setCursor(SystemMouseCursor cursor)
    {
        cursor_ = systemCursor(cursor);
        if (mouse_inside_) SetCursor(cursor_);
    }

    float Win32Window::refreshHz() const
    {
        MONITORINFOEXW mi = {};
        mi.cbSize = sizeof(mi);
        if (!GetMonitorInfoW(MonitorFromWindow(hwnd_, MONITOR_DEFAULTTONEAREST), &mi)) return 60.0f;
        DEVMODEW dm = {};
        dm.dmSize = sizeof(dm);
        if (!EnumDisplaySettingsW(mi.szDevice, ENUM_CURRENT_SETTINGS, &dm) || dm.dmDisplayFrequency <= 1)
            return 60.0f;
        return static_cast<float>(dm.dmDisplayFrequency);
    }

    void Win32Window::updateRefreshRate()
    {
        if (renderer_ && hwnd_) renderer_->setDisplayRefreshHz(refreshHz());
    }

    // A window blocked by a modal one: input never reaches the View (it
    // drops it too); a click or key brings the modal window forward instead.
    // Normally the owner is disabled while its modal is open (EnableWindow),
    // so Windows already does this; this covers windows that aren't the
    // modal's owner.
    bool Win32Window::scopedInputAllowed(UINT msg)
    {
        if (!view_.isInputBlocked()) return true;
        switch (msg)
        {
        case WM_LBUTTONDOWN: case WM_RBUTTONDOWN: case WM_MBUTTONDOWN:
        case WM_KEYDOWN: case WM_SYSKEYDOWN:
            if (Win32Window* modal = forView(view_.inputBlocker()))
                SetForegroundWindow(modal->hwnd());
            MessageBeep(MB_OK);
            break;
        default:
            break;
        }
        return false;
    }

    void Win32Window::dispatchPointer(UINT msg, WPARAM wparam, LPARAM lparam)
    {
        const float dpr = devicePixelRatio();
        PointerEvent e;
        e.pointer_id  = 0;
        e.device_kind = PointerDeviceKind::mouse;
        e.position    = { static_cast<float>(GET_X_LPARAM(lparam)) / dpr,
                          static_cast<float>(GET_Y_LPARAM(lparam)) / dpr };
        switch (msg)
        {
        case WM_MOUSEMOVE:
            e.kind     = PointerEventKind::move;
            e.pressure = (wparam & (MK_LBUTTON | MK_RBUTTON | MK_MBUTTON)) ? 1.0f : 0.0f;
            break;
        case WM_LBUTTONDOWN: case WM_RBUTTONDOWN: case WM_MBUTTONDOWN:
        case WM_LBUTTONDBLCLK: case WM_RBUTTONDBLCLK: case WM_MBUTTONDBLCLK:
            e.kind     = PointerEventKind::down;
            e.pressure = 1.0f;
            e.button   = (msg == WM_RBUTTONDOWN || msg == WM_RBUTTONDBLCLK) ? PointerButton::secondary
                       : (msg == WM_MBUTTONDOWN || msg == WM_MBUTTONDBLCLK) ? PointerButton::tertiary
                                                                           : PointerButton::primary;
            SetCapture(hwnd_);
            break;
        case WM_LBUTTONUP: case WM_RBUTTONUP: case WM_MBUTTONUP:
            e.kind     = PointerEventKind::up;
            e.pressure = 0.0f;
            e.button   = (msg == WM_RBUTTONUP) ? PointerButton::secondary
                       : (msg == WM_MBUTTONUP) ? PointerButton::tertiary
                                               : PointerButton::primary;
            if (!(wparam & (MK_LBUTTON | MK_RBUTTON | MK_MBUTTON))) ReleaseCapture();
            break;
        case WM_MOUSEWHEEL: case WM_MOUSEHWHEEL:
        {
            // Wheel positions are in screen coordinates.
            POINT p = { GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam) };
            ScreenToClient(hwnd_, &p);
            e.kind     = PointerEventKind::scroll;
            e.position = { static_cast<float>(p.x) / dpr, static_cast<float>(p.y) / dpr };
            const float notches = static_cast<float>(GET_WHEEL_DELTA_WPARAM(wparam)) / WHEEL_DELTA;
            if (msg == WM_MOUSEWHEEL) e.scroll_delta_y = notches * 40.0f;
            else                      e.scroll_delta_x = -notches * 40.0f;
            break;
        }
        default:
            return;
        }
        view_.pointerDispatcher().handlePointerEvent(e);
    }

    void Win32Window::dispatchKey(UINT msg, WPARAM wparam, LPARAM lparam)
    {
        KeyEvent ke;
        ke.modifiers = currentModifiers();
        ke.character = 0;
        if (msg == WM_KEYDOWN || msg == WM_SYSKEYDOWN)
        {
            // A bare Shift/Ctrl/Alt only updates the modifier state.
            if (wparam == VK_SHIFT || wparam == VK_CONTROL || wparam == VK_MENU)
            {
                HardwareKeyboard::current().updateModifiers(ke.modifiers);
                return;
            }
            ke.kind     = (lparam & (1 << 30)) ? KeyEventKind::repeat : KeyEventKind::down;
            ke.key_code = windowsKeyCodeToKeyCode(wparam);
        }
        else
        {
            ke.kind     = KeyEventKind::up;
            ke.key_code = windowsKeyCodeToKeyCode(wparam);
        }
        view_.focusManager().handleKeyEvent(ke);
    }

    LRESULT Win32Window::handleMessage(UINT msg, WPARAM wparam, LPARAM lparam)
    {
        // Everything this window handles runs with its View ambient.
        View::Scope scope(&view_);

        switch (msg)
        {
        case WM_ERASEBKGND:
            return 1;

        case WM_CLOSE:
            // Never closed here: the app decides (WindowController::
            // on_close_requested, the main window's close policy), on the
            // next turn, outside this window's own message handling.
            {
                HWND hwnd = hwnd_;
                postTask([hwnd] {
                    if (Win32Window* w = forHwnd(hwnd); w && w->on_close) {
                        auto callback = w->on_close;  // may destroy `w`
                        callback();
                    }
                });
            }
            return 0;

        case WM_ACTIVATE:
            if (LOWORD(wparam) == WA_INACTIVE && on_deactivated)
            {
                HWND hwnd = hwnd_;
                postTask([hwnd] {
                    if (Win32Window* w = forHwnd(hwnd); w && w->on_deactivated) {
                        auto callback = w->on_deactivated;
                        callback();
                    }
                });
            }
            return DefWindowProcW(hwnd_, msg, wparam, lparam);

        case WM_SETTINGCHANGE:
            if (on_settings_changed) { auto cb = on_settings_changed; cb(); }
            return DefWindowProcW(hwnd_, msg, wparam, lparam);

        case WM_SIZE:
            if (wparam != SIZE_MINIMIZED)
            {
                if (renderer_) renderer_->setDevicePixelRatio(devicePixelRatio());
                if (on_metrics_changed) { auto cb = on_metrics_changed; cb(); }
                if (!isLive(this)) return 0;
                // Draw right away, so live resizing doesn't show stretched frames.
                requestFrame();
                drawFrame(true);
            }
            return 0;

        case WM_DPICHANGED:
        {
            const RECT* suggested = reinterpret_cast<const RECT*>(lparam);
            SetWindowPos(hwnd_, nullptr, suggested->left, suggested->top,
                         suggested->right - suggested->left, suggested->bottom - suggested->top,
                         SWP_NOZORDER | SWP_NOACTIVATE);
            if (renderer_) renderer_->setDevicePixelRatio(devicePixelRatio());
            updateRefreshRate();
            if (on_metrics_changed) { auto cb = on_metrics_changed; cb(); }
            if (isLive(this)) requestFrame();
            return 0;
        }

        case WM_EXITSIZEMOVE:
        case WM_DISPLAYCHANGE:
            updateRefreshRate();  // possibly on another monitor now
            return DefWindowProcW(hwnd_, msg, wparam, lparam);

        case WM_PAINT:
        {
            PAINTSTRUCT ps;
            BeginPaint(hwnd_, &ps);
            EndPaint(hwnd_, &ps);
            // The swapchain keeps its last image; DWM recomposes it. Draw
            // only if a frame is pending.
            if (needs_frame_) drawFrame();
            return 0;
        }

        case WM_SETCURSOR:
            if (LOWORD(lparam) == HTCLIENT)
            {
                SetCursor(cursor_ ? cursor_ : LoadCursorA(nullptr, IDC_ARROW));
                return TRUE;
            }
            return DefWindowProcW(hwnd_, msg, wparam, lparam);

        case WM_MOUSEMOVE:
            if (!mouse_inside_)
            {
                TRACKMOUSEEVENT tme = {};
                tme.cbSize    = sizeof(tme);
                tme.dwFlags   = TME_LEAVE;
                tme.hwndTrack = hwnd_;
                TrackMouseEvent(&tme);
                mouse_inside_ = true;
            }
            if (scopedInputAllowed(msg)) dispatchPointer(msg, wparam, lparam);
            return 0;

        case WM_MOUSELEAVE:
            mouse_inside_ = false;
            if (GetCapture() != hwnd_)
            {
                // Far outside: hovers in this window end (another window
                // may be under the pointer now).
                PointerEvent e;
                e.kind        = PointerEventKind::move;
                e.pointer_id  = 0;
                e.device_kind = PointerDeviceKind::mouse;
                e.position    = { -1.0e6f, -1.0e6f };
                view_.pointerDispatcher().handlePointerEvent(e);
            }
            return 0;

        case WM_LBUTTONDOWN: case WM_RBUTTONDOWN: case WM_MBUTTONDOWN:
        case WM_LBUTTONDBLCLK: case WM_RBUTTONDBLCLK: case WM_MBUTTONDBLCLK:
        case WM_LBUTTONUP: case WM_RBUTTONUP: case WM_MBUTTONUP:
        case WM_MOUSEWHEEL: case WM_MOUSEHWHEEL:
            if (scopedInputAllowed(msg)) dispatchPointer(msg, wparam, lparam);
            return 0;

        case WM_CAPTURECHANGED:
            return 0;

        case WM_KEYDOWN: case WM_SYSKEYDOWN:
            if (msg == WM_SYSKEYDOWN && wparam == VK_F4)
                return DefWindowProcW(hwnd_, msg, wparam, lparam);  // Alt+F4 -> WM_CLOSE
            if (scopedInputAllowed(msg)) dispatchKey(msg, wparam, lparam);
            return 0;

        case WM_KEYUP: case WM_SYSKEYUP:
            if (scopedInputAllowed(msg)) dispatchKey(msg, wparam, lparam);
            return 0;

        case WM_CHAR:
        {
            if (!scopedInputAllowed(msg)) return 0;
            // The IME delivers composed text through WM_IME_COMPOSITION.
            if (view_.textInputManager().isComposing()) return 0;
            // UTF-16: combine surrogate pairs into one code point.
            static wchar_t high_surrogate = 0;
            const wchar_t ch = static_cast<wchar_t>(wparam);
            uint32_t code_point = ch;
            if (ch >= 0xD800 && ch <= 0xDBFF) { high_surrogate = ch; return 0; }
            if (ch >= 0xDC00 && ch <= 0xDFFF)
            {
                if (!high_surrogate) return 0;
                code_point = 0x10000u + ((static_cast<uint32_t>(high_surrogate) - 0xD800u) << 10) +
                             (static_cast<uint32_t>(ch) - 0xDC00u);
                high_surrogate = 0;
            }
            KeyEvent ke;
            ke.kind      = KeyEventKind::down;
            ke.key_code  = KeyCode::unknown;
            ke.modifiers = currentModifiers();
            ke.character = code_point;
            view_.focusManager().handleKeyEvent(ke);
            return 0;
        }

        case WM_IME_STARTCOMPOSITION:
            view_.textInputManager().beginComposing();
            updateImeCompositionWindow(hwnd_, view_.textInputManager(), devicePixelRatio());
            return 0;

        case WM_IME_COMPOSITION:
        {
            TextInputManager& tim = view_.textInputManager();
            HIMC himc = ImmGetContext(hwnd_);
            if (!himc) return 0;
            if (lparam & GCS_COMPSTR)
            {
                const LONG size = ImmGetCompositionStringW(himc, GCS_COMPSTR, nullptr, 0);
                if (size > 0)
                {
                    std::vector<wchar_t> buf(size / sizeof(wchar_t));
                    ImmGetCompositionStringW(himc, GCS_COMPSTR, buf.data(), size);
                    const std::string utf8 = wideToUtf8(buf.data(), static_cast<int>(buf.size()));
                    if (!utf8.empty()) tim.updateComposingText(utf8);
                }
            }
            if (lparam & GCS_RESULTSTR)
            {
                const LONG size = ImmGetCompositionStringW(himc, GCS_RESULTSTR, nullptr, 0);
                if (size > 0)
                {
                    std::vector<wchar_t> buf(size / sizeof(wchar_t));
                    ImmGetCompositionStringW(himc, GCS_RESULTSTR, buf.data(), size);
                    tim.updateComposingText(wideToUtf8(buf.data(), static_cast<int>(buf.size())));
                    tim.commitComposing();
                }
                else
                {
                    tim.cancelComposing();
                }
            }
            ImmReleaseContext(hwnd_, himc);
            updateImeCompositionWindow(hwnd_, tim, devicePixelRatio());
            return 0;
        }

        case WM_IME_ENDCOMPOSITION:
            if (view_.textInputManager().isComposing()) view_.textInputManager().cancelComposing();
            return 0;

        default:
            return DefWindowProcW(hwnd_, msg, wparam, lparam);
        }
    }

} // namespace systems::leal::campello_widgets::win32
