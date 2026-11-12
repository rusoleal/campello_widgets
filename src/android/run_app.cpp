// Android platform layer (Phase 17 M6): a host for NativeActivity without
// android_native_app_glue.
//
// Every activity instance of the app (the launcher one, and one per extra
// window opened through WindowController) calls runApp() from its
// ANativeActivity_onCreate, and gets an ActivityHost. All NativeActivity
// callbacks, the input queues and AChoreographer's vsync arrive on the Java
// main thread, which is therefore the one UI thread: one widget tree for all
// windows, as on desktop. Each activity's ANativeWindow gets its own
// campello_gpu Surface, Renderer and RasterThread on the shared device.
//
// The tree lives as long as the process: unlike with the glue (which tore
// the whole session down on APP_CMD_TERM_WINDOW), backgrounding the app or
// destroying the main activity keeps all state; the next main activity
// shows it again.

#include <campello_widgets/android/run_app.hpp>
#include <campello_widgets/campello_widgets.hpp>
#include <campello_widgets/widgets/element.hpp>
#include <campello_widgets/widgets/render_object_element.hpp>
#include <campello_widgets/widgets/window.hpp>
#include <campello_widgets/ui/renderer.hpp>
#include <campello_widgets/ui/render_box.hpp>
#include <campello_widgets/ui/pointer_event.hpp>
#include <campello_widgets/ui/pointer_dispatcher.hpp>
#include <campello_widgets/ui/focus_manager.hpp>
#include <campello_widgets/ui/ticker.hpp>
#include <campello_widgets/ui/frame_scheduler.hpp>
#include <campello_widgets/ui/text_input_manager.hpp>
#include <campello_widgets/ui/view.hpp>
#include <campello_widgets/ui/key_event.hpp>
#include <campello_widgets/ui/thread_checker.hpp>
#include <campello_widgets/ui/raster_thread.hpp>
#include <campello_widgets/ui/platform_windowing.hpp>

#include <campello_gpu/device.hpp>
#include <campello_gpu/surface.hpp>
#include <campello_gpu/constants/pixel_format.hpp>

#include <android/native_activity.h>
#include <android/choreographer.h>
#include <android/configuration.h>
#include <android/log.h>
#include <android/input.h>
#include <android/looper.h>
#include <jni.h>

#include <dlfcn.h>

#include "../gpu/vulkan/vulkan_draw_backend.hpp"
#include "android_text_rasterizer.hpp"
#include "android_clipboard.hpp"

#include <algorithm>
#include <atomic>
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <thread>
#include <vector>
#include <unistd.h>

#define LOG_TAG "campello_widgets"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO,  LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

// Namespace aliases - using global qualification to work correctly in Unity Build
namespace GPU     = ::systems::leal::campello_gpu;
namespace Widgets = ::systems::leal::campello_widgets;

// ---------------------------------------------------------------------------
// JNI helpers (all called on the Java main thread, which has an env)
// ---------------------------------------------------------------------------

static JNIEnv* mainEnv(ANativeActivity* activity)
{
    JNIEnv* env = nullptr;
    if (activity->vm->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_1_6) != JNI_OK)
        return nullptr;
    return env;
}

// Clears (and logs) a pending Java exception; true if there was one.
static bool clearJavaException(JNIEnv* env, const char* what)
{
    if (!env->ExceptionCheck()) return false;
    LOGE("Java exception in %s", what);
    env->ExceptionDescribe();
    env->ExceptionClear();
    return true;
}

static void showSoftInput(ANativeActivity* activity)
{
    JNIEnv* env = mainEnv(activity);
    if (!env) return;
    jobject act = activity->clazz;

    jclass activityClass = env->FindClass("android/app/NativeActivity");
    jmethodID getWindow = env->GetMethodID(activityClass, "getWindow", "()Landroid/view/Window;");
    jobject window = env->CallObjectMethod(act, getWindow);

    jclass windowClass = env->FindClass("android/view/Window");
    jmethodID getDecorView = env->GetMethodID(windowClass, "getDecorView", "()Landroid/view/View;");
    jobject decorView = env->CallObjectMethod(window, getDecorView);

    jmethodID getSystemService = env->GetMethodID(activityClass, "getSystemService",
                                                   "(Ljava/lang/String;)Ljava/lang/Object;");
    jstring serviceName = env->NewStringUTF("input_method");
    jobject imm = env->CallObjectMethod(act, getSystemService, serviceName);

    jclass immClass = env->FindClass("android/view/inputmethod/InputMethodManager");
    jmethodID showSoftInputId = env->GetMethodID(immClass, "showSoftInput",
                                                "(Landroid/view/View;I)Z");
    env->CallBooleanMethod(imm, showSoftInputId, decorView, 0);
    clearJavaException(env, "showSoftInput");

    env->DeleteLocalRef(serviceName);
    env->DeleteLocalRef(imm);
    env->DeleteLocalRef(decorView);
    env->DeleteLocalRef(window);
    env->DeleteLocalRef(immClass);
    env->DeleteLocalRef(windowClass);
    env->DeleteLocalRef(activityClass);
}

static void hideSoftInput(ANativeActivity* activity)
{
    JNIEnv* env = mainEnv(activity);
    if (!env) return;
    jobject act = activity->clazz;

    jclass activityClass = env->FindClass("android/app/NativeActivity");
    jmethodID getWindow = env->GetMethodID(activityClass, "getWindow", "()Landroid/view/Window;");
    jobject window = env->CallObjectMethod(act, getWindow);

    jclass windowClass = env->FindClass("android/view/Window");
    jmethodID getDecorView = env->GetMethodID(windowClass, "getDecorView", "()Landroid/view/View;");
    jobject decorView = env->CallObjectMethod(window, getDecorView);

    jclass viewClass = env->FindClass("android/view/View");
    jmethodID getWindowToken = env->GetMethodID(viewClass, "getWindowToken", "()Landroid/os/IBinder;");
    jobject windowToken = env->CallObjectMethod(decorView, getWindowToken);

    jmethodID getSystemService = env->GetMethodID(activityClass, "getSystemService",
                                                   "(Ljava/lang/String;)Ljava/lang/Object;");
    jstring serviceName = env->NewStringUTF("input_method");
    jobject imm = env->CallObjectMethod(act, getSystemService, serviceName);

    jclass immClass = env->FindClass("android/view/inputmethod/InputMethodManager");
    jmethodID hideSoftInputId = env->GetMethodID(immClass, "hideSoftInputFromWindow",
                                                "(Landroid/os/IBinder;I)Z");
    env->CallBooleanMethod(imm, hideSoftInputId, windowToken, 0);
    clearJavaException(env, "hideSoftInput");

    env->DeleteLocalRef(serviceName);
    env->DeleteLocalRef(imm);
    env->DeleteLocalRef(windowToken);
    env->DeleteLocalRef(decorView);
    env->DeleteLocalRef(window);
    env->DeleteLocalRef(immClass);
    env->DeleteLocalRef(viewClass);
    env->DeleteLocalRef(windowClass);
    env->DeleteLocalRef(activityClass);
}

// The String extra `key` of the activity's launch intent, or "".
static std::string intentStringExtra(ANativeActivity* activity, const char* key)
{
    JNIEnv* env = mainEnv(activity);
    if (!env) return {};
    std::string result;
    jclass activityClass = env->GetObjectClass(activity->clazz);
    jmethodID getIntent = env->GetMethodID(activityClass, "getIntent", "()Landroid/content/Intent;");
    jobject intent = env->CallObjectMethod(activity->clazz, getIntent);
    if (intent && !clearJavaException(env, "getIntent"))
    {
        jclass intentClass = env->GetObjectClass(intent);
        jmethodID getStringExtra = env->GetMethodID(intentClass, "getStringExtra",
                                                    "(Ljava/lang/String;)Ljava/lang/String;");
        jstring jkey = env->NewStringUTF(key);
        auto value = static_cast<jstring>(env->CallObjectMethod(intent, getStringExtra, jkey));
        if (value && !clearJavaException(env, "getStringExtra"))
        {
            const char* chars = env->GetStringUTFChars(value, nullptr);
            result = chars ? chars : "";
            env->ReleaseStringUTFChars(value, chars);
            env->DeleteLocalRef(value);
        }
        env->DeleteLocalRef(jkey);
        env->DeleteLocalRef(intentClass);
        env->DeleteLocalRef(intent);
    }
    env->DeleteLocalRef(activityClass);
    return result;
}

// Starts another instance of `from`'s activity class as a separate window:
// its own task (NEW_DOCUMENT | MULTIPLE_TASK), carrying `window_id`. When
// the app is already in multi-window mode (split screen, freeform), it goes
// next to it (LAUNCH_ADJACENT); from a full-screen app it opens full screen
// on top (LAUNCH_ADJACENT there makes some launchers, e.g. MIUI, enter split
// screen with the new window parked off screen). In a freeform (desktop
// windowing) display, `bounds` (physical pixels, empty = system default)
// places it.
static bool startWindowActivity(ANativeActivity* from, const std::string& window_id,
                                const ARect& bounds)
{
    JNIEnv* env = mainEnv(from);
    if (!env) return false;

    constexpr jint FLAG_ACTIVITY_LAUNCH_ADJACENT = 0x00001000;
    constexpr jint FLAG_ACTIVITY_NEW_DOCUMENT    = 0x00080000;
    constexpr jint FLAG_ACTIVITY_MULTIPLE_TASK   = 0x08000000;

    jclass activityClass = env->GetObjectClass(from->clazz);  // e.g. android.app.NativeActivity
    jclass intentClass   = env->FindClass("android/content/Intent");
    jmethodID intentInit = env->GetMethodID(intentClass, "<init>",
                                            "(Landroid/content/Context;Ljava/lang/Class;)V");
    jobject intent = env->NewObject(intentClass, intentInit, from->clazz, activityClass);
    jmethodID putExtra = env->GetMethodID(intentClass, "putExtra",
        "(Ljava/lang/String;Ljava/lang/String;)Landroid/content/Intent;");
    jstring key = env->NewStringUTF("cw_window_id");
    jstring value = env->NewStringUTF(window_id.c_str());
    jobject self = env->CallObjectMethod(intent, putExtra, key, value);
    if (self) env->DeleteLocalRef(self);
    jmethodID isInMultiWindowMode = env->GetMethodID(activityClass, "isInMultiWindowMode", "()Z");
    const bool multi_window = env->CallBooleanMethod(from->clazz, isInMultiWindowMode) == JNI_TRUE;
    clearJavaException(env, "isInMultiWindowMode");
    jmethodID addFlags = env->GetMethodID(intentClass, "addFlags", "(I)Landroid/content/Intent;");
    self = env->CallObjectMethod(intent, addFlags,
        FLAG_ACTIVITY_NEW_DOCUMENT | FLAG_ACTIVITY_MULTIPLE_TASK |
        (multi_window ? FLAG_ACTIVITY_LAUNCH_ADJACENT : 0));
    if (self) env->DeleteLocalRef(self);

    jobject bundle = nullptr;
    if (bounds.right > bounds.left && bounds.bottom > bounds.top)
    {
        jclass optionsClass = env->FindClass("android/app/ActivityOptions");
        jmethodID makeBasic = env->GetStaticMethodID(optionsClass, "makeBasic",
                                                     "()Landroid/app/ActivityOptions;");
        jobject options = env->CallStaticObjectMethod(optionsClass, makeBasic);
        jclass rectClass = env->FindClass("android/graphics/Rect");
        jmethodID rectInit = env->GetMethodID(rectClass, "<init>", "(IIII)V");
        jobject rect = env->NewObject(rectClass, rectInit, bounds.left, bounds.top,
                                      bounds.right, bounds.bottom);
        jmethodID setLaunchBounds = env->GetMethodID(optionsClass, "setLaunchBounds",
            "(Landroid/graphics/Rect;)Landroid/app/ActivityOptions;");
        self = env->CallObjectMethod(options, setLaunchBounds, rect);
        if (self) env->DeleteLocalRef(self);
        jmethodID toBundle = env->GetMethodID(optionsClass, "toBundle", "()Landroid/os/Bundle;");
        bundle = env->CallObjectMethod(options, toBundle);
        clearJavaException(env, "ActivityOptions");
        env->DeleteLocalRef(rect);
        env->DeleteLocalRef(rectClass);
        env->DeleteLocalRef(options);
        env->DeleteLocalRef(optionsClass);
    }

    jmethodID startActivity = env->GetMethodID(activityClass, "startActivity",
                                               "(Landroid/content/Intent;Landroid/os/Bundle;)V");
    env->CallVoidMethod(from->clazz, startActivity, intent, bundle);
    const bool failed = clearJavaException(env, "startActivity");

    if (bundle) env->DeleteLocalRef(bundle);
    env->DeleteLocalRef(value);
    env->DeleteLocalRef(key);
    env->DeleteLocalRef(intent);
    env->DeleteLocalRef(intentClass);
    env->DeleteLocalRef(activityClass);
    return !failed;
}

// Finishes the activity and removes its task from Recents (a window's
// own NEW_DOCUMENT task).
static void finishAndRemoveTask(ANativeActivity* activity)
{
    JNIEnv* env = mainEnv(activity);
    if (!env) { ANativeActivity_finish(activity); return; }
    jclass activityClass = env->GetObjectClass(activity->clazz);
    jmethodID finish = env->GetMethodID(activityClass, "finishAndRemoveTask", "()V");
    env->CallVoidMethod(activity->clazz, finish);
    if (clearJavaException(env, "finishAndRemoveTask")) ANativeActivity_finish(activity);
    env->DeleteLocalRef(activityClass);
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
        // TV remote "OK"/center button -- activates the focused control the
        // same way Enter does (see RenderGestureDetector::handleFocusKey()).
        case AKEYCODE_DPAD_CENTER:  return Widgets::KeyCode::enter;
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

static Widgets::PointerDeviceKind deviceKindForMotionEvent(
    const AInputEvent* event, size_t pointer_index)
{
    switch (AMotionEvent_getToolType(event, pointer_index))
    {
        case AMOTION_EVENT_TOOL_TYPE_STYLUS: return Widgets::PointerDeviceKind::stylus;
        case AMOTION_EVENT_TOOL_TYPE_ERASER: return Widgets::PointerDeviceKind::invertedStylus;
        case AMOTION_EVENT_TOOL_TYPE_MOUSE:  return Widgets::PointerDeviceKind::mouse;
        default:                             return Widgets::PointerDeviceKind::touch;
    }
}

// AMOTION_EVENT_AXIS_TILT/_ORIENTATION only ever report nonzero for a
// stylus/eraser tool — a finger or mouse reads back 0 here, which already
// matches PointerEvent::tilt's "0.0 == no tilt data" convention, so no
// separate tool-type check is needed before calling these.

static float tiltForMotionEvent(const AInputEvent* event, size_t pointer_index)
{
    // AMOTION_EVENT_AXIS_TILT: magnitude only, 0 (perpendicular) to pi/2
    // (flat) — already matches PointerEvent::tilt's convention directly.
    return AMotionEvent_getAxisValue(event, AMOTION_EVENT_AXIS_TILT, pointer_index);
}

static float tiltOrientationForMotionEvent(const AInputEvent* event, size_t pointer_index)
{
    // AMOTION_EVENT_AXIS_ORIENTATION is already -pi..pi, same range as
    // PointerEvent::tilt_orientation — though, per that field's doc
    // comment, the two platforms' zero-direction reference does not
    // necessarily agree.
    return AMotionEvent_getAxisValue(event, AMOTION_EVENT_AXIS_ORIENTATION, pointer_index);
}

// AMotionEvent_getButtonState() is a per-event (not per-pointer) bitmask,
// and only meaningful for a connected mouse/stylus with buttons -- plain
// touch reports no bits set at all, which correctly falls through to
// `primary` here (a finger tap is not "some other button").
static Widgets::PointerButton buttonForMotionEvent(const AInputEvent* event)
{
    const int32_t buttons = AMotionEvent_getButtonState(event);
    if (buttons & AMOTION_EVENT_BUTTON_SECONDARY) return Widgets::PointerButton::secondary;
    if (buttons & AMOTION_EVENT_BUTTON_TERTIARY)  return Widgets::PointerButton::tertiary;
    return Widgets::PointerButton::primary;
}


// ---------------------------------------------------------------------------
// App and activity state
// ---------------------------------------------------------------------------

namespace {

struct ActivityHost;
class AndroidPlatformWindow;

// Process-wide: the widget tree, the main window's View that hosts it, the
// GPU device. Created by the first runApp() call, kept for the process.
struct AndroidApp
{
    std::unique_ptr<Widgets::View>              view;  // the main window's View
    std::shared_ptr<GPU::Device>                device;
    std::unique_ptr<Widgets::TickerScheduler>   ticker;
    std::unique_ptr<Widgets::PlatformWindowing> windowing;
    std::shared_ptr<Widgets::Element>           root_element;
    std::shared_ptr<Widgets::RenderBox>         root_box;
    Widgets::MediaQueryData                     media;
    Widgets::WidgetRef                          root_widget;
    ActivityHost*                               main_host = nullptr;
    std::vector<ActivityHost*>                  hosts;
    AChoreographer*                             choreographer = nullptr;
    bool                                        frame_pending = false;

    // postTask(): a pipe watched by the main looper.
    int                                         task_pipe[2] = {-1, -1};
    std::vector<std::function<void()>>          tasks;
};
AndroidApp* gApp = nullptr;

bool isLiveHost(ActivityHost* host)
{
    return gApp && std::find(gApp->hosts.begin(), gApp->hosts.end(), host) != gApp->hosts.end();
}

void postFrame();

void postMainTask(std::function<void()> task)
{
    if (!gApp || !task) return;
    gApp->tasks.push_back(std::move(task));
    const char byte = 1;
    (void)!write(gApp->task_pipe[1], &byte, 1);
}

int onTaskPipe(int fd, int /*events*/, void* /*data*/)
{
    char buf[64];
    while (read(fd, buf, sizeof buf) == static_cast<ssize_t>(sizeof buf)) {}
    auto tasks = std::move(gApp->tasks);
    gApp->tasks.clear();
    for (auto& task : tasks) task();
    return 1;  // keep watching
}

Widgets::Brightness brightnessOf(AConfiguration* config)
{
    return AConfiguration_getUiModeNight(config) == ACONFIGURATION_UI_MODE_NIGHT_YES
        ? Widgets::Brightness::dark : Widgets::Brightness::light;
}

// One NativeActivity instance: its native window, input queue and
// configuration, and (while it has a window) the Surface, Renderer and
// RasterThread drawing the View it hosts.
struct ActivityHost
{
    explicit ActivityHost(ANativeActivity* a) : activity(a), config(AConfiguration_new())
    {
        updateConfig();
    }

    ~ActivityHost()
    {
        unbindView();
        AConfiguration_delete(config);
    }

    ANativeActivity*       activity;
    AConfiguration*        config;
    Widgets::View*         view    = nullptr;  // hosted View (not owned)
    AndroidPlatformWindow* owner   = nullptr;  // for extra windows
    bool                   is_main = false;

    ANativeWindow* window = nullptr;
    AInputQueue*   queue  = nullptr;
    ARect          content{};
    float          dpr = 1.0f;

    std::shared_ptr<Widgets::RenderBox>     root;
    std::shared_ptr<GPU::Surface>           surface;
    std::shared_ptr<Widgets::Renderer>      renderer;
    std::unique_ptr<Widgets::RasterThread>  raster;  // stopped before renderer/surface go
    bool                                    needs_frame = true;

    std::function<void()> on_metrics_changed;

    void bindView(Widgets::View* v)
    {
        unbindView();
        view = v;
        if (!view) return;
        ActivityHost* self = this;
        view->setFrameRequestCallback([self] {
            self->needs_frame = true;
            postFrame();
        });
        ANativeActivity* act = activity;
        view->textInputManager().setOnInputTargetChanged([act](bool has_target) {
            if (has_target) showSoftInput(act);
            else            hideSoftInput(act);
        });
    }

    void unbindView()
    {
        stopRendering();
        if (view && Widgets::View::isAlive(view))
        {
            view->setFrameRequestCallback(nullptr);
            view->textInputManager().setOnInputTargetChanged(nullptr);
        }
        view = nullptr;
        root.reset();
    }

    void setRoot(std::shared_ptr<Widgets::RenderBox> r)
    {
        stopRendering();
        root = std::move(r);
        if (view) view->pointerDispatcher().setRoot(root);
        startRendering();
    }

    void startRendering()
    {
        if (renderer || !window || !view || !root || !gApp->device) return;
        surface = gApp->device->createSurface(window);
        if (!surface)
        {
            LOGE("no Vulkan surface for this window");
            return;
        }
        // The draw backend's pipelines must match the surface's real format
        // (not every Vulkan surface offers BGRA8).
        const Widgets::Color background = Widgets::Color::black();
        renderer = std::make_shared<Widgets::Renderer>(gApp->device, root, background);
        renderer->setDrawBackend(std::make_unique<Widgets::VulkanDrawBackend>(
            gApp->device, background, surface->getPixelFormat()));
        renderer->setDevicePixelRatio(dpr);
        renderer->setViewInsets(insets());
        view->setRenderer(renderer.get());

        // Rasterize (including the blocking GPU submit) on a dedicated
        // thread, so a GPU or compositor stall never blocks the main thread,
        // which handles input and vsync for every window.
        raster = std::make_unique<Widgets::RasterThread>(
            [renderer = renderer](const Widgets::FramePackage& pkg) { renderer->rasterFrame(pkg); });
        raster->start();

        needs_frame = true;
        postFrame();
    }

    void stopRendering()
    {
        if (raster) raster.reset();  // stops and joins: nothing in flight below
        if (view && Widgets::View::isAlive(view)) view->setRenderer(nullptr);
        renderer.reset();
        surface.reset();
    }

    void updateConfig()
    {
        AConfiguration_fromAssetManager(config, activity->assetManager);
        int32_t density = AConfiguration_getDensity(config);
        if (density <= 0) density = ACONFIGURATION_DENSITY_DEFAULT;
        dpr = static_cast<float>(density) / 160.0f;  // mdpi baseline
    }

    // Safe area from the content rect, logical pixels.
    Widgets::EdgeInsets insets() const
    {
        Widgets::EdgeInsets in;
        if (!window) return in;
        const int32_t w = ANativeWindow_getWidth(window);
        const int32_t h = ANativeWindow_getHeight(window);
        in.left   = std::max(0.0f, static_cast<float>(content.left)) / dpr;
        in.top    = std::max(0.0f, static_cast<float>(content.top)) / dpr;
        in.right  = std::max(0.0f, static_cast<float>(w - content.right)) / dpr;
        in.bottom = std::max(0.0f, static_cast<float>(h - content.bottom)) / dpr;
        // Before the first onContentRectChanged the rect is empty: no insets.
        if (content.right <= content.left || content.bottom <= content.top) return {};
        return in;
    }

    Widgets::WindowMetrics metrics() const
    {
        Widgets::WindowMetrics m;
        m.device_pixel_ratio = dpr;
        if (window)
            m.logical_size = Widgets::Size{ ANativeWindow_getWidth(window) / dpr,
                                            ANativeWindow_getHeight(window) / dpr };
        m.padding = insets();
        return m;
    }

    void metricsChanged()
    {
        if (renderer)
        {
            renderer->setDevicePixelRatio(dpr);
            renderer->setViewInsets(insets());
            // Repaint everything, even if no widget changed: after a resize
            // or rotation the surface is (re)created at the new size, and a
            // frame drawn just before that (at the new size, into the old
            // swapchain) would otherwise stay on screen, cropped.
            renderer->notePaintRequested();
        }
        needs_frame = true;
        auto callback = on_metrics_changed;
        if (callback) callback();
        postFrame();
    }

    void frame()
    {
        if (!needs_frame || !renderer || !raster || !window) return;
        needs_frame = false;
        const float w = static_cast<float>(ANativeWindow_getWidth(window));
        const float h = static_cast<float>(ANativeWindow_getHeight(window));
        auto package = renderer->buildFrame(w, h);
        if (!package) return;
        // This window's surface: acquired and presented on its raster thread.
        package->surface = surface;
        raster->submit(std::move(*package));
    }
};

// ---------------------------------------------------------------------------
// Input: each activity's AInputQueue, on the main looper
// ---------------------------------------------------------------------------

int32_t handleInputEvent(ActivityHost* host, AInputEvent* event)
{
    if (!host->view) return 0;
    Widgets::View::Scope scope(host->view);
    Widgets::PointerDispatcher& dispatcher = host->view->pointerDispatcher();

    const int32_t event_type = AInputEvent_getType(event);
    if (event_type == AINPUT_EVENT_TYPE_MOTION)
    {
        const int32_t source = AInputEvent_getSource(event);
        if ((source & AINPUT_SOURCE_CLASS_MASK) != AINPUT_SOURCE_CLASS_POINTER)
            return 0;

        const int32_t action = AMotionEvent_getAction(event);
        const int32_t action_code = action & AMOTION_EVENT_ACTION_MASK;
        const int32_t pointer_index = (action & AMOTION_EVENT_ACTION_POINTER_INDEX_MASK)
            >> AMOTION_EVENT_ACTION_POINTER_INDEX_SHIFT;

        // Raw physical pixels → logical points, relative to the content
        // (the renderer paints the tree inset by the safe area).
        const float dpr = host->dpr > 0.0f ? host->dpr : 1.0f;
        const Widgets::EdgeInsets in = host->insets();
        auto pos = [&](size_t j) {
            return Widgets::Offset{ AMotionEvent_getX(event, j) / dpr - in.left,
                                    AMotionEvent_getY(event, j) / dpr - in.top };
        };

        switch (action_code)
        {
        case AMOTION_EVENT_ACTION_DOWN:
        case AMOTION_EVENT_ACTION_POINTER_DOWN:
        {
            const size_t idx = static_cast<size_t>(pointer_index);
            dispatcher.handlePointerEvent({
                .kind        = Widgets::PointerEventKind::down,
                .pointer_id  = AMotionEvent_getPointerId(event, idx),
                .position    = pos(idx),
                .pressure    = AMotionEvent_getPressure(event, idx),
                .device_kind = deviceKindForMotionEvent(event, idx),
                .button      = buttonForMotionEvent(event),
                .tilt        = tiltForMotionEvent(event, idx),
                .tilt_orientation = tiltOrientationForMotionEvent(event, idx)});
            break;
        }

        case AMOTION_EVENT_ACTION_MOVE:
        {
            const size_t pointer_count = AMotionEvent_getPointerCount(event);
            for (size_t j = 0; j < pointer_count; ++j)
            {
                dispatcher.handlePointerEvent({
                    .kind        = Widgets::PointerEventKind::move,
                    .pointer_id  = AMotionEvent_getPointerId(event, j),
                    .position    = pos(j),
                    .pressure    = AMotionEvent_getPressure(event, j),
                    .device_kind = deviceKindForMotionEvent(event, j),
                    .tilt        = tiltForMotionEvent(event, j),
                    .tilt_orientation = tiltOrientationForMotionEvent(event, j)});
            }
            break;
        }

        // A hovering stylus (e.g. S-Pen) or mouse: moves without contact,
        // which drive MouseRegion's enter/exit (pressure 0 = hover).
        case AMOTION_EVENT_ACTION_HOVER_ENTER:
        case AMOTION_EVENT_ACTION_HOVER_MOVE:
        {
            const size_t pointer_count = AMotionEvent_getPointerCount(event);
            for (size_t j = 0; j < pointer_count; ++j)
            {
                dispatcher.handlePointerEvent({
                    .kind        = Widgets::PointerEventKind::move,
                    .pointer_id  = AMotionEvent_getPointerId(event, j),
                    .position    = pos(j),
                    .pressure    = 0.0f,
                    .device_kind = deviceKindForMotionEvent(event, j),
                    .tilt        = tiltForMotionEvent(event, j),
                    .tilt_orientation = tiltOrientationForMotionEvent(event, j)});
            }
            break;
        }

        // Left proximity: one move far outside so the last hover exits.
        case AMOTION_EVENT_ACTION_HOVER_EXIT:
        {
            const size_t idx = static_cast<size_t>(pointer_index);
            dispatcher.handlePointerEvent({
                .kind        = Widgets::PointerEventKind::move,
                .pointer_id  = AMotionEvent_getPointerId(event, idx),
                .position    = { -1.0e6f, -1.0e6f },
                .pressure    = 0.0f,
                .device_kind = deviceKindForMotionEvent(event, idx)});
            break;
        }

        case AMOTION_EVENT_ACTION_UP:
        case AMOTION_EVENT_ACTION_POINTER_UP:
        {
            const size_t idx = static_cast<size_t>(pointer_index);
            dispatcher.handlePointerEvent({
                .kind        = Widgets::PointerEventKind::up,
                .pointer_id  = AMotionEvent_getPointerId(event, idx),
                .position    = pos(idx),
                .pressure    = 0.0f,
                .device_kind = deviceKindForMotionEvent(event, idx),
                .button      = buttonForMotionEvent(event)});
            break;
        }

        case AMOTION_EVENT_ACTION_CANCEL:
        {
            const size_t pointer_count = AMotionEvent_getPointerCount(event);
            for (size_t j = 0; j < pointer_count; ++j)
            {
                dispatcher.handlePointerEvent({
                    .kind        = Widgets::PointerEventKind::cancel,
                    .pointer_id  = AMotionEvent_getPointerId(event, j),
                    .position    = pos(j),
                    .pressure    = 0.0f,
                    .device_kind = deviceKindForMotionEvent(event, j)});
            }
            break;
        }

        default:
            break;
        }
        return 1;
    }

    if (event_type == AINPUT_EVENT_TYPE_KEY)
    {
        const int32_t action      = AKeyEvent_getAction(event);
        const int32_t keyCode     = AKeyEvent_getKeyCode(event);
        const int32_t metaState   = AKeyEvent_getMetaState(event);
        const int32_t repeatCount = AKeyEvent_getRepeatCount(event);

        // Back in an extra window: let the system close it (its activity
        // finishes, which closes the window). The main window keeps
        // consuming it, as before.
        if (keyCode == AKEYCODE_BACK && !host->is_main) return 0;

        Widgets::KeyEventKind kind;
        if (action == AKEY_EVENT_ACTION_DOWN)
            kind = repeatCount > 0 ? Widgets::KeyEventKind::repeat : Widgets::KeyEventKind::down;
        else if (action == AKEY_EVENT_ACTION_UP)
            kind = Widgets::KeyEventKind::up;
        else
            return 0;

        Widgets::KeyEvent ke;
        ke.kind      = kind;
        ke.key_code  = androidKeyCodeToKeyCode(keyCode);
        ke.modifiers = androidMetaStateToKeyModifiers(metaState);
        ke.character = kind == Widgets::KeyEventKind::up ? 0 : androidKeyCodeToCharacter(keyCode, metaState);
        host->view->focusManager().handleKeyEvent(ke);
        return 1;
    }
    return 0;
}

int onInputQueue(int /*fd*/, int /*events*/, void* data)
{
    auto* host = static_cast<ActivityHost*>(data);
    if (!isLiveHost(host) || !host->queue) return 1;
    AInputEvent* event = nullptr;
    while (host->queue && AInputQueue_getEvent(host->queue, &event) >= 0)
    {
        // The IME gets first go at key events (soft keyboard).
        if (AInputQueue_preDispatchEvent(host->queue, event)) continue;
        const int32_t handled = handleInputEvent(host, event);
        AInputQueue_finishEvent(host->queue, event, handled);
    }
    return 1;
}

// ---------------------------------------------------------------------------
// Frames: AChoreographer vsync on the main thread, all windows
// ---------------------------------------------------------------------------

// AChoreographer_postFrameCallback64() (API 29+) is resolved with dlsym so
// this builds against older platform levels: the plain variant's `long`
// timestamp overflows on 32-bit ABIs (armeabi-v7a), freezing every ticker.
using AChoreographerFrameCallback64 = void (*)(int64_t, void*);
using PostFrameCallback64Fn = void (*)(AChoreographer*, AChoreographerFrameCallback64, void*);

PostFrameCallback64Fn resolvePostFrameCallback64()
{
    static PostFrameCallback64Fn fn = reinterpret_cast<PostFrameCallback64Fn>(
        dlsym(RTLD_DEFAULT, "AChoreographer_postFrameCallback64"));
    return fn;
}

void onVsync(int64_t /*frameTimeNanos*/, void* /*data*/)
{
    if (!gApp) return;
    gApp->frame_pending = false;
    // A frame may open or close windows: iterate over a snapshot.
    const std::vector<ActivityHost*> hosts = gApp->hosts;
    for (ActivityHost* host : hosts)
        if (isLiveHost(host)) host->frame();
}

void onVsyncLegacy(long frameTimeNanos, void* data)
{
    onVsync(static_cast<int64_t>(frameTimeNanos), data);
}

void postFrame()
{
    if (!gApp || !gApp->choreographer || gApp->frame_pending) return;
    gApp->frame_pending = true;
    if (auto* post64 = resolvePostFrameCallback64())
        post64(gApp->choreographer, onVsync, nullptr);
    else
        AChoreographer_postFrameCallback(gApp->choreographer, onVsyncLegacy, nullptr);
}

// ---------------------------------------------------------------------------
// The main window: hosts the tree's root View
// ---------------------------------------------------------------------------

void updateMainMediaQuery()
{
    ActivityHost* host = gApp->main_host;
    if (!host || !gApp->root_element || !host->window) return;
    const Widgets::WindowMetrics m = host->metrics();
    Widgets::MediaQueryData data = gApp->media;
    data.device_pixel_ratio  = m.device_pixel_ratio;
    data.logical_size        = m.logical_size;
    data.view_insets         = m.padding;
    data.platform_brightness = brightnessOf(host->config);
    if (!(data != gApp->media)) return;
    gApp->media = data;
    Widgets::View::Scope scope(gApp->view.get());
    gApp->root_element->update(Widgets::mw<Widgets::MediaQuery>(gApp->media, gApp->root_widget));
    Widgets::FrameScheduler::scheduleFrame();
}

// Mounts the tree the first time the main window has a size.
void mountTreeIfNeeded(ActivityHost* host)
{
    if (gApp->root_element || !host->window) return;
    const Widgets::WindowMetrics m = host->metrics();
    Widgets::MediaQueryData media;
    media.device_pixel_ratio  = m.device_pixel_ratio;
    media.platform_brightness = brightnessOf(host->config);
    media.logical_size        = m.logical_size;
    media.view_insets         = m.padding;
    gApp->media = media;

    gApp->root_element = Widgets::mw<Widgets::MediaQuery>(media, gApp->root_widget)->createElement();
    {
        Widgets::View::Scope scope(gApp->view.get());
        gApp->root_element->mount(nullptr);
    }
    auto* roe = gApp->root_element->findDescendantRenderObjectElement();
    if (!roe) { LOGE("widget tree produced no RenderObjectElement"); return; }
    gApp->root_box = std::dynamic_pointer_cast<Widgets::RenderBox>(roe->sharedRenderObject());
    if (!gApp->root_box) LOGE("root render object is not a RenderBox");
    LOGI("campello_widgets tree mounted (DPR=%.2f, size=%.0fx%.0f)",
         media.device_pixel_ratio, media.logical_size.width, media.logical_size.height);
}

void becomeMain(ActivityHost* host)
{
    host->is_main    = true;
    gApp->main_host  = host;
    host->bindView(gApp->view.get());
    host->on_metrics_changed = [] { updateMainMediaQuery(); };
}

// ---------------------------------------------------------------------------
// Extra windows: activities of their own (PlatformWindowing)
// ---------------------------------------------------------------------------

std::map<std::string, AndroidPlatformWindow*>& pendingWindows()
{
    static std::map<std::string, AndroidPlatformWindow*> windows;
    return windows;
}

std::vector<AndroidPlatformWindow*>& liveWindows()
{
    static std::vector<AndroidPlatformWindow*> windows;
    return windows;
}

class AndroidPlatformWindow final : public Widgets::PlatformWindow
{
public:
    AndroidPlatformWindow(std::string title, Widgets::Size requested_size)
        : view_(std::make_unique<Widgets::View>())
        , title_(std::move(title))
        , requested_size_(requested_size)
    {
        static int next_id = 1;
        id_ = std::to_string(getpid()) + "-" + std::to_string(next_id++);
        liveWindows().push_back(this);
    }

    ~AndroidPlatformWindow() override
    {
        auto& live = liveWindows();
        live.erase(std::remove(live.begin(), live.end(), this), live.end());
        pendingWindows().erase(id_);
        if (ActivityHost* host = host_)
        {
            detach();
            // Closing the window closes its activity (and its Recents task).
            if (!host_destroyed_) finishAndRemoveTask(host->activity);
        }
    }

    const std::string& windowId() const noexcept { return id_; }
    void setPending() { pendingWindows()[id_] = this; }

    void attach(ActivityHost* host)
    {
        pendingWindows().erase(id_);
        host_ = host;
        host->owner = this;
        host->bindView(view_.get());
        std::weak_ptr<int> alive = alive_;
        AndroidPlatformWindow* self = this;
        host->on_metrics_changed = [self, alive] {
            if (alive.expired()) return;
            auto callback = self->on_metrics_changed;
            if (callback) callback();
        };
        applyTitle();
        if (root_) host->setRoot(root_);
    }

    // Its activity is being destroyed (the user closed the window, pressed
    // back, or swiped its task away): tell the app; by default it destroys us.
    void activityDestroyed()
    {
        host_destroyed_ = true;
        detach();
        fireCloseRequested();
    }

    // --- PlatformWindow
    Widgets::View& view() override { return *view_; }

    void setRootRenderBox(std::shared_ptr<Widgets::RenderBox> root) override
    {
        root_ = std::move(root);
        if (host_) host_->setRoot(root_);
    }

    void setTitle(const std::string& title) override
    {
        title_ = title;
        applyTitle();
    }

    // Android decides window sizes (only freeform windows take launch bounds).
    void setSize(Widgets::Size logical_size) override { requested_size_ = logical_size; }

    Widgets::WindowMetrics metrics() const override
    {
        if (host_ && host_->window) return host_->metrics();
        Widgets::WindowMetrics m;  // not started yet
        m.logical_size       = requested_size_;
        m.device_pixel_ratio = gApp && gApp->main_host ? gApp->main_host->dpr : 1.0f;
        return m;
    }

    void show() override {}  // started (and shown) by createWindow()

    void requestClose() override
    {
        std::weak_ptr<int> alive = alive_;
        AndroidPlatformWindow* self = this;
        postMainTask([self, alive] { if (!alive.expired()) self->fireCloseRequested(); });
    }

private:
    void fireCloseRequested()
    {
        auto callback = on_close_requested;  // may destroy `this`
        if (callback) callback();
    }

    void detach()
    {
        if (!host_) return;
        host_->on_metrics_changed = nullptr;
        host_->unbindView();
        host_->owner = nullptr;
        host_ = nullptr;
    }

    void applyTitle()
    {
        if (!host_) return;
        JNIEnv* env = mainEnv(host_->activity);
        if (!env) return;
        jclass activityClass = env->GetObjectClass(host_->activity->clazz);
        jmethodID setTitleId = env->GetMethodID(activityClass, "setTitle", "(Ljava/lang/CharSequence;)V");
        jstring title = env->NewStringUTF(title_.c_str());
        env->CallVoidMethod(host_->activity->clazz, setTitleId, title);
        clearJavaException(env, "setTitle");
        env->DeleteLocalRef(title);
        env->DeleteLocalRef(activityClass);
    }

    std::unique_ptr<Widgets::View>      view_;  // first: outlives everything bound to it
    std::string                         id_;
    std::string                         title_;
    Widgets::Size                       requested_size_;
    ActivityHost*                       host_ = nullptr;
    bool                                host_destroyed_ = false;
    std::shared_ptr<Widgets::RenderBox> root_;
    std::shared_ptr<int>                alive_ = std::make_shared<int>(0);  // for posted tasks
};

bool isLiveWindow(AndroidPlatformWindow* window)
{
    auto& live = liveWindows();
    return std::find(live.begin(), live.end(), window) != live.end();
}

class AndroidPlatformWindowing final : public Widgets::PlatformWindowing
{
public:
    std::unique_ptr<Widgets::PlatformWindow> createWindow(const Widgets::WindowOptions& options) override
    {
        // Started from any live activity (the main one if possible).
        ActivityHost* from = gApp->main_host ? gApp->main_host
                           : (gApp->hosts.empty() ? nullptr : gApp->hosts.front());
        if (!from) return nullptr;

        auto window = std::make_unique<AndroidPlatformWindow>(options.title, options.size);
        ARect bounds{};
        if (options.position)
        {
            const float dpr = from->dpr;
            bounds.left   = static_cast<int32_t>(options.position->x * dpr);
            bounds.top    = static_cast<int32_t>(options.position->y * dpr);
            bounds.right  = bounds.left + static_cast<int32_t>(options.size.width * dpr);
            bounds.bottom = bounds.top + static_cast<int32_t>(options.size.height * dpr);
        }
        window->setPending();
        if (!startWindowActivity(from->activity, window->windowId(), bounds)) return nullptr;
        return window;
    }

    void postTask(std::function<void()> task) override { postMainTask(std::move(task)); }
};

// ---------------------------------------------------------------------------
// NativeActivity callbacks (Java main thread)
// ---------------------------------------------------------------------------

ActivityHost* hostOf(ANativeActivity* activity)
{
    auto* host = static_cast<ActivityHost*>(activity->instance);
    return isLiveHost(host) ? host : nullptr;
}

void onDestroy(ANativeActivity* activity)
{
    ActivityHost* host = hostOf(activity);
    if (!host) return;
    if (host->is_main)
    {
        // The tree stays mounted; the next main activity shows it again.
        host->on_metrics_changed = nullptr;
        host->unbindView();
        gApp->main_host = nullptr;
    }
    else if (host->owner)
    {
        host->owner->activityDestroyed();
    }
    auto& hosts = gApp->hosts;
    hosts.erase(std::remove(hosts.begin(), hosts.end(), host), hosts.end());
    activity->instance = nullptr;
    delete host;
}

void onNativeWindowCreated(ANativeActivity* activity, ANativeWindow* window)
{
    ActivityHost* host = hostOf(activity);
    if (!host) return;
    host->window = window;
    host->updateConfig();
    if (host->is_main)
    {
        mountTreeIfNeeded(host);
        host->setRoot(gApp->root_box);
    }
    else
    {
        host->startRendering();
    }
    host->metricsChanged();
}

void onNativeWindowResized(ANativeActivity* activity, ANativeWindow* window)
{
    ActivityHost* host = hostOf(activity);
    if (!host) return;
    LOGI("window %p resized to %dx%d", static_cast<void*>(window),
         ANativeWindow_getWidth(window), ANativeWindow_getHeight(window));
    if (host->surface)
        host->surface->resize(static_cast<uint32_t>(ANativeWindow_getWidth(window)),
                              static_cast<uint32_t>(ANativeWindow_getHeight(window)));
    host->metricsChanged();
}

void onNativeWindowRedrawNeeded(ANativeActivity* activity, ANativeWindow* /*window*/)
{
    if (ActivityHost* host = hostOf(activity))
    {
        host->needs_frame = true;
        postFrame();
    }
}

void onNativeWindowDestroyed(ANativeActivity* activity, ANativeWindow* /*window*/)
{
    ActivityHost* host = hostOf(activity);
    if (!host) return;
    // Must not return while the raster thread may still present to it.
    host->stopRendering();
    host->window = nullptr;
}

void onInputQueueCreated(ANativeActivity* activity, AInputQueue* queue)
{
    ActivityHost* host = hostOf(activity);
    if (!host) return;
    host->queue = queue;
    AInputQueue_attachLooper(queue, ALooper_forThread(), 1, onInputQueue, host);
}

void onInputQueueDestroyed(ANativeActivity* activity, AInputQueue* queue)
{
    AInputQueue_detachLooper(queue);
    if (ActivityHost* host = hostOf(activity)) host->queue = nullptr;
}

void onContentRectChanged(ANativeActivity* activity, const ARect* rect)
{
    ActivityHost* host = hostOf(activity);
    if (!host || !rect) return;
    host->content = *rect;
    host->metricsChanged();
}

void onConfigurationChanged(ANativeActivity* activity)
{
    ActivityHost* host = hostOf(activity);
    if (!host) return;
    host->updateConfig();
    host->metricsChanged();
}

void* onSaveInstanceState(ANativeActivity* /*activity*/, size_t* out_size)
{
    *out_size = 0;
    return nullptr;
}

// NativeActivity doesn't route stdout/stderr to logcat: pipe both through
// a reader thread into __android_log_write.
void redirectStdioToLogcat()
{
    static int pipe_fds[2];
    if (pipe(pipe_fds) != 0) return;
    dup2(pipe_fds[1], STDOUT_FILENO);
    dup2(pipe_fds[1], STDERR_FILENO);
    // A pipe makes stdout fully buffered: flush per line, as on a terminal.
    setvbuf(stdout, nullptr, _IOLBF, 0);
    std::thread([]() {
        char buf[1024];
        ssize_t n;
        while ((n = read(pipe_fds[0], buf, sizeof(buf) - 1)) > 0) {
            buf[n] = '\0';
            __android_log_write(ANDROID_LOG_INFO, "campello_widgets.stdio", buf);
        }
    }).detach();
}

// Process-wide setup, on the first activity.
void initApp(ANativeActivity* activity, const std::function<Widgets::WidgetRef()>& make_root)
{
    redirectStdioToLogcat();
    gApp = new AndroidApp();

    // JavaVM for the text rasterizer, the Activity (as a Context) for the clipboard.
    Widgets::setAndroidJavaVM(activity->vm);
    Widgets::setAndroidClipboardContext(activity->vm, activity->clazz);

    Widgets::ThreadChecker::instance().bindToCurrentThread();

    // No window of its own: every activity's window gets a Surface on it.
    gApp->device = GPU::Device::createDefaultDevice(nullptr);
    if (!gApp->device) LOGE("failed to create campello_gpu device");

    // The main window's View; makePrimary() also installs its managers as
    // the process-wide fallbacks, for code without a window context.
    gApp->view = std::make_unique<Widgets::View>();
    gApp->view->makePrimary();
    gApp->ticker = std::make_unique<Widgets::TickerScheduler>();
    Widgets::TickerScheduler::setActive(gApp->ticker.get());

    // postTask() and vsync, both on the main looper.
    if (pipe(gApp->task_pipe) == 0)
        ALooper_addFd(ALooper_forThread(), gApp->task_pipe[0], ALOOPER_POLL_CALLBACK,
                      ALOOPER_EVENT_INPUT, onTaskPipe, nullptr);
    gApp->choreographer = AChoreographer_getInstance();
    Widgets::FrameScheduler::setCallback([] { postFrame(); });

    if (gApp->device)
    {
        gApp->windowing = std::make_unique<AndroidPlatformWindowing>();
        Widgets::PlatformWindowing::setInstance(gApp->windowing.get());
    }

    gApp->root_widget = make_root ? make_root() : nullptr;
}

} // namespace

// ---------------------------------------------------------------------------
// runApp
// ---------------------------------------------------------------------------

namespace systems::leal::campello_widgets
{

void runApp(ANativeActivity* activity, void* /*saved_state*/, size_t /*saved_state_size*/,
            std::function<WidgetRef()> make_root)
{
    if (!activity) return;
    if (!gApp) initApp(activity, make_root);

    auto* host = new ActivityHost(activity);
    activity->instance = host;
    gApp->hosts.push_back(host);

    ANativeActivityCallbacks* cb = activity->callbacks;
    cb->onDestroy                  = onDestroy;
    cb->onNativeWindowCreated      = onNativeWindowCreated;
    cb->onNativeWindowResized      = onNativeWindowResized;
    cb->onNativeWindowRedrawNeeded = onNativeWindowRedrawNeeded;
    cb->onNativeWindowDestroyed    = onNativeWindowDestroyed;
    cb->onInputQueueCreated        = onInputQueueCreated;
    cb->onInputQueueDestroyed      = onInputQueueDestroyed;
    cb->onContentRectChanged       = onContentRectChanged;
    cb->onConfigurationChanged     = onConfigurationChanged;
    cb->onSaveInstanceState        = onSaveInstanceState;

    // Which window is this activity?
    const std::string window_id = intentStringExtra(activity, "cw_window_id");
    if (!window_id.empty())
    {
        // One WindowController::create() started.
        auto it = pendingWindows().find(window_id);
        if (it != pendingWindows().end())
        {
            it->second->attach(host);
        }
        else
        {
            // Gone meanwhile, or brought back from Recents after the process
            // died: that window no longer exists.
            LOGI("closing a stale window activity (%s)", window_id.c_str());
            finishAndRemoveTask(activity);
        }
        return;
    }

    if (!gApp->main_host)
    {
        becomeMain(host);
        return;
    }

    // Another launcher instance while the main window exists: offered to
    // the app like iPadOS's user-opened windows; otherwise closed.
    auto window = std::make_unique<AndroidPlatformWindow>("", Widgets::Size{0.0f, 0.0f});
    AndroidPlatformWindow* raw = window.get();
    std::unique_ptr<PlatformWindow> base = std::move(window);
    if (WindowController::offerPlatformWindow(base) && isLiveWindow(raw))
        raw->attach(host);
    else
        finishAndRemoveTask(activity);
}

} // namespace systems::leal::campello_widgets
