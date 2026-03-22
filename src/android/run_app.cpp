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

#include <campello_gpu/device.hpp>
#include <campello_gpu/texture_view.hpp>
#include <campello_gpu/constants/pixel_format.hpp>

#include <game-activity/native_app_glue/android_native_app_glue.h>
#include <android/log.h>

#include <memory>
#include <chrono>

#define LOG_TAG "campello_widgets"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO,  LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

namespace GPU     = systems::leal::campello_gpu;
namespace Widgets = systems::leal::campello_widgets;

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
};

// ---------------------------------------------------------------------------
// Touch event processing
// ---------------------------------------------------------------------------

static void handleMotionEvents(android_app* app, WidgetSession* session)
{
    android_input_buffer* buf = android_app_swap_input_buffers(app);
    if (!buf) return;

    for (uint64_t i = 0; i < buf->motionEventsCount; ++i)
    {
        const GameActivityMotionEvent& ev = buf->motionEvents[i];

        const int32_t action_code =
            ev.action & AMOTION_EVENT_ACTION_MASK;
        const int32_t pointer_index =
            (ev.action & AMOTION_EVENT_ACTION_POINTER_INDEX_MASK)
            >> AMOTION_EVENT_ACTION_POINTER_INDEX_SHIFT;

        switch (action_code)
        {
        case AMOTION_EVENT_ACTION_DOWN:
        case AMOTION_EVENT_ACTION_POINTER_DOWN:
        {
            const auto& p = ev.pointers[pointer_index];
            session->dispatcher->handlePointerEvent({
                Widgets::PointerEventKind::down,
                p.id,
                { p.axisValues[AMOTION_AXIS_X], p.axisValues[AMOTION_AXIS_Y] },
                1.0f});
            break;
        }

        case AMOTION_EVENT_ACTION_MOVE:
        {
            // MOVE carries all currently active pointers.
            for (int32_t j = 0; j < ev.pointerCount; ++j)
            {
                const auto& p = ev.pointers[j];
                session->dispatcher->handlePointerEvent({
                    Widgets::PointerEventKind::move,
                    p.id,
                    { p.axisValues[AMOTION_AXIS_X], p.axisValues[AMOTION_AXIS_Y] },
                    1.0f});
            }
            break;
        }

        case AMOTION_EVENT_ACTION_UP:
        case AMOTION_EVENT_ACTION_POINTER_UP:
        {
            const auto& p = ev.pointers[pointer_index];
            session->dispatcher->handlePointerEvent({
                Widgets::PointerEventKind::up,
                p.id,
                { p.axisValues[AMOTION_AXIS_X], p.axisValues[AMOTION_AXIS_Y] },
                0.0f});
            break;
        }

        case AMOTION_EVENT_ACTION_CANCEL:
        {
            // Cancel all active pointers.
            for (int32_t j = 0; j < ev.pointerCount; ++j)
            {
                const auto& p = ev.pointers[j];
                session->dispatcher->handlePointerEvent({
                    Widgets::PointerEventKind::cancel,
                    p.id,
                    { p.axisValues[AMOTION_AXIS_X], p.axisValues[AMOTION_AXIS_Y] },
                    0.0f});
            }
            break;
        }

        default:
            break;
        }
    }

    android_app_clear_motion_events(buf);
}

// ---------------------------------------------------------------------------
// Window lifecycle
// ---------------------------------------------------------------------------

static bool motion_event_filter(const GameActivityMotionEvent* ev)
{
    const auto source_class = ev->source & AINPUT_SOURCE_CLASS_MASK;
    return source_class == AINPUT_SOURCE_CLASS_POINTER;
}

static std::unique_ptr<WidgetSession> createSession(
    android_app* app, const Widgets::WidgetRef& root_widget)
{
    auto session = std::make_unique<WidgetSession>();

    // Create dispatcher and focus manager before mounting.
    session->dispatcher = std::make_shared<Widgets::PointerDispatcher>();
    Widgets::PointerDispatcher::setActiveDispatcher(session->dispatcher.get());

    session->focus_manager = std::make_shared<Widgets::FocusManager>();
    Widgets::FocusManager::setActiveManager(session->focus_manager.get());

    session->ticker_scheduler = std::make_unique<Widgets::TickerScheduler>();
    Widgets::TickerScheduler::setActive(session->ticker_scheduler.get());

    // Mount widget tree.
    session->root_element = root_widget->createElement();
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

    LOGI("campello_widgets session created");
    return session;
}

// ---------------------------------------------------------------------------
// runApp
// ---------------------------------------------------------------------------

namespace systems::leal::campello_widgets
{

void runApp(android_app* app, WidgetRef root_widget)
{
    android_app_set_motion_event_filter(app, motion_event_filter);

    std::unique_ptr<WidgetSession> session;

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
                TickerScheduler::setActive(nullptr);
                session_ptr->reset();
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
        // Poll events.
        int          events;
        android_poll_source* source;
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
        }

        if (!session || !session->renderer) continue;

        // Process touch input.
        handleMotionEvents(app, session.get());

        // Render frame.
        // TODO(Phase 10): obtain swapchain TextureView from campello_gpu surface.
        // For now, the frame loop ticks the dispatcher (long-press timers, etc.)
        // but does not submit GPU work until a draw backend is configured.
        const auto now_tp  = std::chrono::steady_clock::now().time_since_epoch();
        const uint64_t ms  =
            std::chrono::duration_cast<std::chrono::milliseconds>(now_tp).count();
        if (auto* d = PointerDispatcher::activeDispatcher()) d->tick(ms);
        if (auto* ts = TickerScheduler::active()) ts->tick(ms);
    }

    // Cleanup.
    if (session)
    {
        PointerDispatcher::setActiveDispatcher(nullptr);
        FocusManager::setActiveManager(nullptr);
        TickerScheduler::setActive(nullptr);
    }
}

} // namespace systems::leal::campello_widgets
