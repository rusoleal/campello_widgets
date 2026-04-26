#include <campello_widgets/android/run_app.hpp>
#include <campello_widgets/campello_widgets.hpp>
#include <campello_widgets/widgets/element.hpp>
#include <campello_widgets/widgets/render_object_element.hpp>
#include <campello_widgets/ui/renderer.hpp>
#include <campello_widgets/ui/render_box.hpp>
#include <campello_widgets/ui/pointer_event.hpp>
#include <campello_widgets/ui/pointer_dispatcher.hpp>
#include <campello_widgets/ui/focus_manager.hpp>
#include <campello_widgets/ui/ticker.hpp>
#include <campello_widgets/ui/frame_scheduler.hpp>
#include <campello_widgets/ui/text_input_manager.hpp>
#include <campello_widgets/ui/key_event.hpp>

#include <campello_gpu/device.hpp>
#include <campello_gpu/texture_view.hpp>
#include <campello_gpu/constants/pixel_format.hpp>

#include <android_native_app_glue.h>
#include <android/choreographer.h>
#include <android/configuration.h>
#include <android/log.h>
#include <android/input.h>

#include "vulkan_draw_backend.hpp"

#include <memory>
#include <atomic>
#include <string>

#define LOG_TAG "campello_widgets"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO,  LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

// Namespace aliases - using global qualification to work correctly in Unity Build
namespace GPU     = ::systems::leal::campello_gpu;
namespace Widgets = ::systems::leal::campello_widgets;

// ---------------------------------------------------------------------------
// WidgetSession — owns all per-window campello_widgets state
// ---------------------------------------------------------------------------

struct WidgetSession
{
    std::shared_ptr<GPU::Device>               device;
    std::shared_ptr<Widgets::Renderer>         renderer;
    std::shared_ptr<Widgets::Element>          root_element;
    std::shared_ptr<Widgets::PointerDispatcher> dispatcher;
    std::shared_ptr<Widgets::FocusManager>       focus_manager;
    std::unique_ptr<Widgets::TickerScheduler>    ticker_scheduler;
    std::unique_ptr<Widgets::TextInputManager>   text_input_manager;
    android_app*                               app = nullptr;  // For accessing contentRect
    Widgets::WidgetRef                         user_root_widget;
    Widgets::MediaQueryData                    media_data;
};

// Forward declaration — defined after createSession.
static void rebuildMediaQuery(WidgetSession* session);

// ---------------------------------------------------------------------------
// Window metrics helper (logical size + view insets + MediaQuery rebuild)
// ---------------------------------------------------------------------------

/**
 * @brief Updates logical window size, view insets, and MediaQueryData.
 *
 * Reads the physical window size and content rect from ANativeWindow, converts
 * everything to logical pixels using the current DPR, updates the renderer's
 * view insets, and rebuilds the root MediaQuery widget if anything changed.
 */
static void updateWindowMetrics(WidgetSession* session)
{
    if (!session || !session->app) return;
    if (!session->app->window) return;

    android_app* app = session->app;

    int32_t window_width  = ANativeWindow_getWidth(app->window);
    int32_t window_height = ANativeWindow_getHeight(app->window);

    const ARect& content = app->contentRect;

    // Calculate insets in physical pixels
    Widgets::EdgeInsets physical_insets;
    physical_insets.left   = static_cast<float>(content.left);
    physical_insets.top    = static_cast<float>(content.top);
    physical_insets.right  = static_cast<float>(window_width - content.right);
    physical_insets.bottom = static_cast<float>(window_height - content.bottom);

    if (physical_insets.left < 0.0f)   physical_insets.left = 0.0f;
    if (physical_insets.top < 0.0f)    physical_insets.top = 0.0f;
    if (physical_insets.right < 0.0f)  physical_insets.right = 0.0f;
    if (physical_insets.bottom < 0.0f) physical_insets.bottom = 0.0f;

    // Convert to logical pixels
    float dpr = session->media_data.device_pixel_ratio;
    if (dpr <= 0.0f) dpr = 1.0f;

    Widgets::EdgeInsets logical_insets;
    logical_insets.left   = physical_insets.left   / dpr;
    logical_insets.top    = physical_insets.top    / dpr;
    logical_insets.right  = physical_insets.right  / dpr;
    logical_insets.bottom = physical_insets.bottom / dpr;

    // Update renderer
    if (session->renderer)
    {
        session->renderer->setViewInsets(logical_insets);
    }

    // Update MediaQueryData
    Widgets::MediaQueryData newData = session->media_data;
    newData.logical_size = Widgets::Size{
        static_cast<float>(window_width) / dpr,
        static_cast<float>(window_height) / dpr};
    newData.view_insets  = logical_insets;

    if (newData != session->media_data)
    {
        session->media_data = newData;
        rebuildMediaQuery(session);
    }
}

// ---------------------------------------------------------------------------
// JNI helpers for soft keyboard show / hide
// ---------------------------------------------------------------------------

static JNIEnv* getJniEnv(android_app* app, bool* out_attached)
{
    JNIEnv* env = nullptr;
    *out_attached = false;
    if (app->activity->vm->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_1_6) == JNI_OK)
        return env;
    if (app->activity->vm->AttachCurrentThread(&env, nullptr) == JNI_OK)
        *out_attached = true;
    return env;
}

static void showSoftInput(android_app* app)
{
    bool attached = false;
    JNIEnv* env = getJniEnv(app, &attached);
    if (!env) return;

    jobject activity = app->activity->clazz;

    jclass activityClass = env->FindClass("android/app/NativeActivity");
    jmethodID getWindow = env->GetMethodID(activityClass, "getWindow", "()Landroid/view/Window;");
    jobject window = env->CallObjectMethod(activity, getWindow);

    jclass windowClass = env->FindClass("android/view/Window");
    jmethodID getDecorView = env->GetMethodID(windowClass, "getDecorView", "()Landroid/view/View;");
    jobject decorView = env->CallObjectMethod(window, getDecorView);

    jmethodID getSystemService = env->GetMethodID(activityClass, "getSystemService",
                                                   "(Ljava/lang/String;)Ljava/lang/Object;");
    jstring serviceName = env->NewStringUTF("input_method");
    jobject imm = env->CallObjectMethod(activity, getSystemService, serviceName);

    jclass immClass = env->FindClass("android/view/inputmethod/InputMethodManager");
    jmethodID showSoftInput = env->GetMethodID(immClass, "showSoftInput",
                                                "(Landroid/view/View;I)Z");
    env->CallBooleanMethod(imm, showSoftInput, decorView, 0);

    env->DeleteLocalRef(serviceName);
    env->DeleteLocalRef(imm);
    env->DeleteLocalRef(decorView);
    env->DeleteLocalRef(window);

    if (attached)
        app->activity->vm->DetachCurrentThread();
}

static void hideSoftInput(android_app* app)
{
    bool attached = false;
    JNIEnv* env = getJniEnv(app, &attached);
    if (!env) return;

    jobject activity = app->activity->clazz;

    jclass activityClass = env->FindClass("android/app/NativeActivity");
    jmethodID getWindow = env->GetMethodID(activityClass, "getWindow", "()Landroid/view/Window;");
    jobject window = env->CallObjectMethod(activity, getWindow);

    jclass windowClass = env->FindClass("android/view/Window");
    jmethodID getDecorView = env->GetMethodID(windowClass, "getDecorView", "()Landroid/view/View;");
    jobject decorView = env->CallObjectMethod(window, getDecorView);

    jclass viewClass = env->FindClass("android/view/View");
    jmethodID getWindowToken = env->GetMethodID(viewClass, "getWindowToken", "()Landroid/os/IBinder;");
    jobject windowToken = env->CallObjectMethod(decorView, getWindowToken);

    jmethodID getSystemService = env->GetMethodID(activityClass, "getSystemService",
                                                   "(Ljava/lang/String;)Ljava/lang/Object;");
    jstring serviceName = env->NewStringUTF("input_method");
    jobject imm = env->CallObjectMethod(activity, getSystemService, serviceName);

    jclass immClass = env->FindClass("android/view/inputmethod/InputMethodManager");
    jmethodID hideSoftInput = env->GetMethodID(immClass, "hideSoftInputFromWindow",
                                                "(Landroid/os/IBinder;I)Z");
    env->CallBooleanMethod(imm, hideSoftInput, windowToken, 0);

    env->DeleteLocalRef(serviceName);
    env->DeleteLocalRef(imm);
    env->DeleteLocalRef(windowToken);
    env->DeleteLocalRef(decorView);
    env->DeleteLocalRef(window);

    if (attached)
        app->activity->vm->DetachCurrentThread();
}

// ---------------------------------------------------------------------------
// Key code translation
// ---------------------------------------------------------------------------

static Widgets::KeyCode androidKeyCodeToKeyCode(int32_t keyCode)
{
    switch (keyCode)
    {
        case AKEYCODE_A: return Widgets::KeyCode::a;
        case AKEYCODE_B: return Widgets::KeyCode::b;
        case AKEYCODE_C: return Widgets::KeyCode::c;
        case AKEYCODE_D: return Widgets::KeyCode::d;
        case AKEYCODE_E: return Widgets::KeyCode::e;
        case AKEYCODE_F: return Widgets::KeyCode::f;
        case AKEYCODE_G: return Widgets::KeyCode::g;
        case AKEYCODE_H: return Widgets::KeyCode::h;
        case AKEYCODE_I: return Widgets::KeyCode::i;
        case AKEYCODE_J: return Widgets::KeyCode::j;
        case AKEYCODE_K: return Widgets::KeyCode::k;
        case AKEYCODE_L: return Widgets::KeyCode::l;
        case AKEYCODE_M: return Widgets::KeyCode::m;
        case AKEYCODE_N: return Widgets::KeyCode::n;
        case AKEYCODE_O: return Widgets::KeyCode::o;
        case AKEYCODE_P: return Widgets::KeyCode::p;
        case AKEYCODE_Q: return Widgets::KeyCode::q;
        case AKEYCODE_R: return Widgets::KeyCode::r;
        case AKEYCODE_S: return Widgets::KeyCode::s;
        case AKEYCODE_T: return Widgets::KeyCode::t;
        case AKEYCODE_U: return Widgets::KeyCode::u;
        case AKEYCODE_V: return Widgets::KeyCode::v;
        case AKEYCODE_W: return Widgets::KeyCode::w;
        case AKEYCODE_X: return Widgets::KeyCode::x;
        case AKEYCODE_Y: return Widgets::KeyCode::y;
        case AKEYCODE_Z: return Widgets::KeyCode::z;
        case AKEYCODE_0: return Widgets::KeyCode::digit_0;
        case AKEYCODE_1: return Widgets::KeyCode::digit_1;
        case AKEYCODE_2: return Widgets::KeyCode::digit_2;
        case AKEYCODE_3: return Widgets::KeyCode::digit_3;
        case AKEYCODE_4: return Widgets::KeyCode::digit_4;
        case AKEYCODE_5: return Widgets::KeyCode::digit_5;
        case AKEYCODE_6: return Widgets::KeyCode::digit_6;
        case AKEYCODE_7: return Widgets::KeyCode::digit_7;
        case AKEYCODE_8: return Widgets::KeyCode::digit_8;
        case AKEYCODE_9: return Widgets::KeyCode::digit_9;
        case AKEYCODE_SPACE:        return Widgets::KeyCode::space;
        case AKEYCODE_ENTER:        return Widgets::KeyCode::enter;
        case AKEYCODE_TAB:          return Widgets::KeyCode::tab;
        case AKEYCODE_DEL:          return Widgets::KeyCode::backspace;
        case AKEYCODE_FORWARD_DEL:  return Widgets::KeyCode::delete_forward;
        case AKEYCODE_ESCAPE:       return Widgets::KeyCode::escape;
        case AKEYCODE_DPAD_LEFT:    return Widgets::KeyCode::left;
        case AKEYCODE_DPAD_RIGHT:   return Widgets::KeyCode::right;
        case AKEYCODE_DPAD_UP:      return Widgets::KeyCode::up;
        case AKEYCODE_DPAD_DOWN:    return Widgets::KeyCode::down;
        case AKEYCODE_MOVE_HOME:    return Widgets::KeyCode::home;
        case AKEYCODE_MOVE_END:     return Widgets::KeyCode::end;
        case AKEYCODE_PAGE_UP:      return Widgets::KeyCode::page_up;
        case AKEYCODE_PAGE_DOWN:    return Widgets::KeyCode::page_down;
        case AKEYCODE_F1:  return Widgets::KeyCode::f1;
        case AKEYCODE_F2:  return Widgets::KeyCode::f2;
        case AKEYCODE_F3:  return Widgets::KeyCode::f3;
        case AKEYCODE_F4:  return Widgets::KeyCode::f4;
        case AKEYCODE_F5:  return Widgets::KeyCode::f5;
        case AKEYCODE_F6:  return Widgets::KeyCode::f6;
        case AKEYCODE_F7:  return Widgets::KeyCode::f7;
        case AKEYCODE_F8:  return Widgets::KeyCode::f8;
        case AKEYCODE_F9:  return Widgets::KeyCode::f9;
        case AKEYCODE_F10: return Widgets::KeyCode::f10;
        case AKEYCODE_F11: return Widgets::KeyCode::f11;
        case AKEYCODE_F12: return Widgets::KeyCode::f12;
        case AKEYCODE_SHIFT_LEFT:   return Widgets::KeyCode::left_shift;
        case AKEYCODE_SHIFT_RIGHT:  return Widgets::KeyCode::right_shift;
        case AKEYCODE_CTRL_LEFT:    return Widgets::KeyCode::left_ctrl;
        case AKEYCODE_CTRL_RIGHT:   return Widgets::KeyCode::right_ctrl;
        case AKEYCODE_ALT_LEFT:     return Widgets::KeyCode::left_alt;
        case AKEYCODE_ALT_RIGHT:    return Widgets::KeyCode::right_alt;
        case AKEYCODE_META_LEFT:    return Widgets::KeyCode::left_meta;
        case AKEYCODE_META_RIGHT:   return Widgets::KeyCode::right_meta;
        case AKEYCODE_CAPS_LOCK:    return Widgets::KeyCode::caps_lock;
        default: return Widgets::KeyCode::unknown;
    }
}

static uint32_t androidMetaStateToKeyModifiers(int32_t metaState)
{
    uint32_t mods = Widgets::KeyModifiers::none;
    if (metaState & AMETA_SHIFT_ON)   mods |= Widgets::KeyModifiers::shift;
    if (metaState & AMETA_CTRL_ON)    mods |= Widgets::KeyModifiers::ctrl;
    if (metaState & AMETA_ALT_ON)     mods |= Widgets::KeyModifiers::alt;
    if (metaState & AMETA_META_ON)    mods |= Widgets::KeyModifiers::meta;
    return mods;
}

static uint32_t androidKeyCodeToCharacter(int32_t keyCode, int32_t metaState)
{
    // Only produce characters for down/repeat events; let the caller filter.
    const bool shift = (metaState & AMETA_SHIFT_ON) != 0;

    if (keyCode >= AKEYCODE_A && keyCode <= AKEYCODE_Z)
    {
        char c = static_cast<char>('a' + (keyCode - AKEYCODE_A));
        if (shift) c = static_cast<char>('A' + (keyCode - AKEYCODE_A));
        return static_cast<uint32_t>(c);
    }
    if (keyCode >= AKEYCODE_0 && keyCode <= AKEYCODE_9)
    {
        if (shift)
        {
            const char shifted[] = {')', '!', '@', '#', '$', '%', '^', '&', '*', '('};
            return static_cast<uint32_t>(shifted[keyCode - AKEYCODE_0]);
        }
        return static_cast<uint32_t>('0' + (keyCode - AKEYCODE_0));
    }

    switch (keyCode)
    {
        case AKEYCODE_SPACE:        return ' ';
        case AKEYCODE_PERIOD:       return shift ? '>' : '.';
        case AKEYCODE_COMMA:        return shift ? '<' : ',';
        case AKEYCODE_SLASH:        return shift ? '?' : '/';
        case AKEYCODE_BACKSLASH:    return shift ? '|' : '\\';
        case AKEYCODE_SEMICOLON:    return shift ? ':' : ';';
        case AKEYCODE_APOSTROPHE:   return shift ? '"' : '\'';
        case AKEYCODE_LEFT_BRACKET: return shift ? '{' : '[';
        case AKEYCODE_RIGHT_BRACKET:return shift ? '}' : ']';
        case AKEYCODE_GRAVE:        return shift ? '~' : '`';
        case AKEYCODE_EQUALS:       return shift ? '+' : '=';
        case AKEYCODE_MINUS:        return shift ? '_' : '-';
        case AKEYCODE_PLUS:         return '+';
        case AKEYCODE_STAR:         return '*';
        case AKEYCODE_POUND:        return '#';
        default: return 0;
    }
}

// ---------------------------------------------------------------------------
// Input event processing (motion + key)
// ---------------------------------------------------------------------------

static int32_t handleAndroidInputEvent(android_app* app, AInputEvent* event)
{
    auto* session_ptr = reinterpret_cast<std::unique_ptr<WidgetSession>*>(app->userData);
    if (!session_ptr || !*session_ptr) return 0;

    WidgetSession* session = session_ptr->get();

    const int32_t event_type = AInputEvent_getType(event);
    if (event_type == AINPUT_EVENT_TYPE_MOTION)
    {
        // Filter to pointer source only (matching old motion_event_filter)
        const int32_t source = AInputEvent_getSource(event);
        if ((source & AINPUT_SOURCE_CLASS_MASK) != AINPUT_SOURCE_CLASS_POINTER)
            return 0;

        const int32_t action = AMotionEvent_getAction(event);
        const int32_t action_code = action & AMOTION_EVENT_ACTION_MASK;
        const int32_t pointer_index = (action & AMOTION_EVENT_ACTION_POINTER_INDEX_MASK)
            >> AMOTION_EVENT_ACTION_POINTER_INDEX_SHIFT;

        switch (action_code)
        {
        case AMOTION_EVENT_ACTION_DOWN:
        case AMOTION_EVENT_ACTION_POINTER_DOWN:
        {
            const int32_t id = AMotionEvent_getPointerId(event, static_cast<size_t>(pointer_index));
            const float x = AMotionEvent_getX(event, static_cast<size_t>(pointer_index));
            const float y = AMotionEvent_getY(event, static_cast<size_t>(pointer_index));
            session->dispatcher->handlePointerEvent({
                Widgets::PointerEventKind::down,
                id,
                { x, y },
                1.0f});
            break;
        }

        case AMOTION_EVENT_ACTION_MOVE:
        {
            const size_t pointer_count = AMotionEvent_getPointerCount(event);
            for (size_t j = 0; j < pointer_count; ++j)
            {
                const int32_t id = AMotionEvent_getPointerId(event, j);
                const float x = AMotionEvent_getX(event, j);
                const float y = AMotionEvent_getY(event, j);
                session->dispatcher->handlePointerEvent({
                    Widgets::PointerEventKind::move,
                    id,
                    { x, y },
                    1.0f});
            }
            break;
        }

        case AMOTION_EVENT_ACTION_UP:
        case AMOTION_EVENT_ACTION_POINTER_UP:
        {
            const int32_t id = AMotionEvent_getPointerId(event, static_cast<size_t>(pointer_index));
            const float x = AMotionEvent_getX(event, static_cast<size_t>(pointer_index));
            const float y = AMotionEvent_getY(event, static_cast<size_t>(pointer_index));
            session->dispatcher->handlePointerEvent({
                Widgets::PointerEventKind::up,
                id,
                { x, y },
                0.0f});
            break;
        }

        case AMOTION_EVENT_ACTION_CANCEL:
        {
            const size_t pointer_count = AMotionEvent_getPointerCount(event);
            for (size_t j = 0; j < pointer_count; ++j)
            {
                const int32_t id = AMotionEvent_getPointerId(event, j);
                const float x = AMotionEvent_getX(event, j);
                const float y = AMotionEvent_getY(event, j);
                session->dispatcher->handlePointerEvent({
                    Widgets::PointerEventKind::cancel,
                    id,
                    { x, y },
                    0.0f});
            }
            break;
        }

        default:
            break;
        }

        return 1;
    }
    else if (event_type == AINPUT_EVENT_TYPE_KEY)
    {
        if (!session->focus_manager) return 0;

        const int32_t action = AKeyEvent_getAction(event);
        const int32_t keyCode = AKeyEvent_getKeyCode(event);
        const int32_t metaState = AKeyEvent_getMetaState(event);
        const int32_t repeatCount = AKeyEvent_getRepeatCount(event);

        Widgets::KeyEventKind kind;
        if (action == AKEY_EVENT_ACTION_DOWN)
        {
            kind = (repeatCount > 0)
                ? Widgets::KeyEventKind::repeat
                : Widgets::KeyEventKind::down;
        }
        else if (action == AKEY_EVENT_ACTION_UP)
        {
            kind = Widgets::KeyEventKind::up;
        }
        else
        {
            return 0;
        }

        if (kind == Widgets::KeyEventKind::up)
        {
            Widgets::KeyEvent ke;
            ke.kind      = kind;
            ke.key_code  = androidKeyCodeToKeyCode(keyCode);
            ke.modifiers = androidMetaStateToKeyModifiers(metaState);
            ke.character = 0;
            session->focus_manager->handleKeyEvent(ke);
        }
        else
        {
            Widgets::KeyEvent ke;
            ke.kind      = kind;
            ke.key_code  = androidKeyCodeToKeyCode(keyCode);
            ke.modifiers = androidMetaStateToKeyModifiers(metaState);
            ke.character = androidKeyCodeToCharacter(keyCode, metaState);
            session->focus_manager->handleKeyEvent(ke);
        }

        return 1;
    }

    return 0;
}

// ---------------------------------------------------------------------------
// Window lifecycle
// ---------------------------------------------------------------------------

static Widgets::Brightness getSystemBrightness(android_app* app)
{
    int32_t nightMode = AConfiguration_getUiModeNight(app->config);
    if (nightMode == ACONFIGURATION_UI_MODE_NIGHT_YES)
        return Widgets::Brightness::dark;
    return Widgets::Brightness::light;
}

static float getDevicePixelRatio(android_app* app)
{
    // Get the density from the configuration
    // ACONFIGURATION_DENSITY_DEFAULT is 160 DPI (mdpi)
    int32_t density = AConfiguration_getDensity(app->config);
    if (density <= 0) density = ACONFIGURATION_DENSITY_DEFAULT;
    
    // DPR = density / 160 (mdpi baseline)
    return static_cast<float>(density) / 160.0f;
}

static std::unique_ptr<WidgetSession> createSession(
    android_app* app, const Widgets::WidgetRef& root_widget)
{
    auto session = std::make_unique<WidgetSession>();
    session->app = app;  // Store for later access to contentRect

    // Create dispatcher and focus manager before mounting.
    session->dispatcher = std::make_shared<Widgets::PointerDispatcher>();
    Widgets::PointerDispatcher::setActiveDispatcher(session->dispatcher.get());

    session->focus_manager = std::make_shared<Widgets::FocusManager>();
    Widgets::FocusManager::setActiveManager(session->focus_manager.get());

    session->ticker_scheduler = std::make_unique<Widgets::TickerScheduler>();
    Widgets::TickerScheduler::setActive(session->ticker_scheduler.get());

    session->text_input_manager = std::make_unique<Widgets::TextInputManager>();
    Widgets::TextInputManager::setActiveManager(session->text_input_manager.get());

    // Show / hide the software keyboard when a TextField gains or loses focus.
    session->text_input_manager->setOnInputTargetChanged([app](bool has_target) {
        if (has_target)
            showSoftInput(app);
        else
            hideSoftInput(app);
    });

    // Wrap root widget with MediaQuery
    Widgets::MediaQueryData mediaData;
    mediaData.device_pixel_ratio = getDevicePixelRatio(app);
    mediaData.platform_brightness = getSystemBrightness(app);
    session->media_data = mediaData;
    session->user_root_widget = root_widget;
    
    auto wrappedRoot = Widgets::mw<Widgets::MediaQuery>(
        mediaData, root_widget);

    // Mount widget tree.
    session->root_element = wrappedRoot->createElement();
    session->root_element->mount(nullptr);

    auto* roe = session->root_element->findDescendantRenderObjectElement();
    if (!roe)
    {
        LOGE("widget tree produced no RenderObjectElement");
        return nullptr;
    }

    auto render_box = std::dynamic_pointer_cast<Widgets::RenderBox>(
        roe->sharedRenderObject());
    if (!render_box)
    {
        LOGE("root render object is not a RenderBox");
        return nullptr;
    }

    session->dispatcher->setRoot(render_box);

    // Create GPU device (ANativeWindow* as platform data).
    session->device = GPU::Device::createDefaultDevice(app->window);
    if (!session->device)
    {
        LOGE("failed to create campello_gpu device");
        return nullptr;
    }

    // Create renderer (no draw backend yet — Android backend is platform-specific;
    // subclass Renderer or supply an IDrawBackend to enable GPU drawing).
    session->renderer = std::make_shared<Widgets::Renderer>(
        session->device, render_box, Widgets::Color::black());

    // Set initial device pixel ratio
    float dpr = getDevicePixelRatio(app);
    session->renderer->setDevicePixelRatio(dpr);

    // Populate logical size, view insets, and push to MediaQuery
    updateWindowMetrics(session.get());

    // Create Vulkan draw backend and attach to renderer
    auto backend = std::make_unique<Widgets::VulkanDrawBackend>(
        session->device, Widgets::Color::black(), GPU::PixelFormat::bgra8unorm);
    session->renderer->setDrawBackend(std::move(backend));

    LOGI("campello_widgets session created (DPR=%.2f, size=%.0fx%.0f)",
         dpr, session->media_data.logical_size.width,
         session->media_data.logical_size.height);
    return session;
}

// ---------------------------------------------------------------------------
// AChoreographer vsync callback
// ---------------------------------------------------------------------------
//
// Fires on the main thread at the display vsync boundary (≈ 16 ms at 60 Hz).
// AChoreographer_postFrameCallback() is called by FrameScheduler::scheduleFrame();
// the callback itself re-arms only when animations are still running (via the
// tick → scheduleFrame chain), so idle CPU drops to ~0%.
//
// The "frame pending" flag coalesces multiple scheduleFrame() calls that arrive
// within the same vsync interval into exactly one callback registration.

static void rebuildMediaQuery(WidgetSession* session)
{
    if (!session || !session->root_element) return;
    auto newMediaQuery = Widgets::mw<Widgets::MediaQuery>(
        session->media_data, session->user_root_widget);
    session->root_element->update(newMediaQuery);
    Widgets::FrameScheduler::scheduleFrame();
}

static std::atomic<bool> gFramePending{false};
static WidgetSession* gActiveSession = nullptr;

static void onVsyncCallback(long frameTimeNanos, void* data)
{
    // Allow the next scheduleFrame() call to post a new callback.
    gFramePending.store(false, std::memory_order_relaxed);

    // Tick schedulers at the vsync timestamp.
    const uint64_t ms = static_cast<uint64_t>(frameTimeNanos) / 1'000'000ULL;
    if (auto* d  = Widgets::PointerDispatcher::activeDispatcher()) d->tick(ms);
    if (auto* ts = Widgets::TickerScheduler::active())            ts->tick(ms);

    // Render frame
    auto* session = static_cast<WidgetSession*>(data);
    if (session && session->renderer && session->device && session->app && session->app->window)
    {
        auto color_view = session->device->getSwapchainTextureView();
        if (color_view)
        {
            int32_t w = ANativeWindow_getWidth(session->app->window);
            int32_t h = ANativeWindow_getHeight(session->app->window);
            if (auto* backend = session->renderer->drawBackend())
                backend->setViewport(static_cast<float>(w), static_cast<float>(h));
            session->renderer->renderFrame(color_view, static_cast<float>(w), static_cast<float>(h));
        }
    }
}

// ---------------------------------------------------------------------------
// runApp
// ---------------------------------------------------------------------------

namespace systems::leal::campello_widgets
{
    // Namespace aliases for use inside this namespace block
    namespace GPU     = ::systems::leal::campello_gpu;
    namespace Widgets = ::systems::leal::campello_widgets;

void runApp(android_app* app, WidgetRef root_widget)
{
    // Vsync-gated on-demand rendering via AChoreographer (API 24+).
    // AChoreographer_getInstance() must be called on the main thread (the one
    // that owns the ALooper).  Choreographer callbacks are delivered through
    // the same ALooper so ALooper_pollOnce(-1) unblocks at vsync — no busy
    // waiting, zero idle CPU.
    AChoreographer* choreographer = AChoreographer_getInstance();
    FrameScheduler::setCallback([choreographer] {
        // Post at most one pending callback per vsync interval.
        if (!gFramePending.exchange(true, std::memory_order_relaxed))
            AChoreographer_postFrameCallback(choreographer, onVsyncCallback, gActiveSession);
    });

    std::unique_ptr<WidgetSession> session;

    app->onInputEvent = handleAndroidInputEvent;

    app->onAppCmd = [](android_app* a, int32_t cmd)
    {
        auto* session_ptr = reinterpret_cast<std::unique_ptr<WidgetSession>*>(a->userData);

        switch (cmd)
        {
        case APP_CMD_INIT_WINDOW:
            // Window is ready — session is created by the main loop below.
            break;

        case APP_CMD_TERM_WINDOW:
            if (session_ptr && *session_ptr)
            {
                PointerDispatcher::setActiveDispatcher(nullptr);
                FocusManager::setActiveManager(nullptr);
                TextInputManager::setActiveManager(nullptr);
                TickerScheduler::setActive(nullptr);
                gActiveSession = nullptr;
                session_ptr->reset();
            }
            break;

        case APP_CMD_CONTENT_RECT_CHANGED:
            // Safe area / content rect changed (e.g., keyboard showed/hid, rotation)
            if (session_ptr && *session_ptr)
            {
                updateWindowMetrics(session_ptr->get());
            }
            break;

        case APP_CMD_CONFIG_CHANGED:
            // Configuration changed (e.g., density/DPR changed, dark mode toggled,
            // orientation changed)
            if (session_ptr && *session_ptr)
            {
                float dpr = getDevicePixelRatio(a);
                if ((*session_ptr)->renderer)
                {
                    (*session_ptr)->renderer->setDevicePixelRatio(dpr);
                }
                if ((*session_ptr)->media_data.device_pixel_ratio != dpr)
                {
                    (*session_ptr)->media_data.device_pixel_ratio = dpr;
                    LOGI("DPR updated to %.2f", dpr);
                }

                Widgets::Brightness newBrightness = getSystemBrightness(a);
                if ((*session_ptr)->media_data.platform_brightness != newBrightness)
                {
                    (*session_ptr)->media_data.platform_brightness = newBrightness;
                    LOGI("platform brightness changed to %s",
                         newBrightness == Widgets::Brightness::dark ? "dark" : "light");
                }

                // DPR or orientation change affects logical size and insets
                updateWindowMetrics(session_ptr->get());
            }
            break;

        default:
            break;
        }
    };

    app->userData = &session;

    bool running = true;
    while (running)
    {
        // Block until a native event or an ALooper_wake() arrives.
        // FrameScheduler::scheduleFrame() calls ALooper_wake(), so the loop
        // unblocks as soon as setState/markNeedsPaint/ticker fires — then
        // immediately drains remaining events before rendering.  This brings
        // idle CPU to ~0%, matching Flutter's on-demand render loop.
        int          events;
        android_poll_source* source;
        ALooper_pollOnce(-1, nullptr, &events, reinterpret_cast<void**>(&source));
        if (source) source->process(app, source);

        // Drain any additional events that arrived while processing.
        while (ALooper_pollOnce(0, nullptr, &events,
                                reinterpret_cast<void**>(&source)) >= 0)
        {
            if (source) source->process(app, source);
        }

        if (app->destroyRequested)
        {
            running = false;
            break;
        }

        // Create session once the window is available.
        if (app->window && !session)
        {
            session = createSession(app, root_widget);
            if (session)
            {
                gActiveSession = session.get();
            }
        }

        if (!session || !session->renderer) continue;

        // Ticking and rendering are now done in onVsyncCallback, which fires
        // at the hardware vsync via AChoreographer.  FrameScheduler::scheduleFrame()
        // posts the callback; the TickerScheduler re-arms it each frame while
        // animations are active, then goes quiet when idle.
    }

    // Cleanup.
    if (session)
    {
        PointerDispatcher::setActiveDispatcher(nullptr);
        FocusManager::setActiveManager(nullptr);
        TextInputManager::setActiveManager(nullptr);
        TickerScheduler::setActive(nullptr);
    }
}

} // namespace systems::leal::campello_widgets
