#pragma once

#include <thread>

namespace systems::leal::campello_widgets
{

/**
 * @brief Debug-only thread-safety checker.
 *
 * campello_widgets is a single-threaded UI framework. All widget tree
 * manipulation, layout, painting, animation, and input handling must happen
 * on one thread (the "UI" or "main" thread).
 *
 * ThreadChecker captures the thread ID at application startup and asserts
 * at critical framework entry points when called from a different thread.
 *
 * All checks are compiled away in release builds (#ifndef NDEBUG).
 *
 * Usage:
 *   // In platform runApp(), after entering the UI thread:
 *   ThreadChecker::instance().bindToCurrentThread();
 *
 *   // In framework entry points:
 *   ThreadChecker::instance().assertOnBoundThread("scheduleFrame");
 */
class ThreadChecker
{
public:
    /// The UI-thread checker — bound once at startup by the platform's
    /// runApp() entry point. Unchanged by the raster-thread split: every
    /// existing call site (Renderer::buildFrame, RenderObject::markNeeds*,
    /// PointerDispatcher, FocusManager, TickerScheduler, AnimationController)
    /// keeps asserting against this same instance.
    static ThreadChecker& instance() noexcept;

    /**
     * @brief A second, independent checker for the raster thread.
     *
     * Bound by `RasterThread::workerLoop()` on platforms that adopt a
     * dedicated raster thread. Unbound (hasBinding() == false) on
     * platforms that still call `Renderer::rasterFrame()` synchronously
     * from the UI thread via the `renderFrame()` back-compat wrapper —
     * code that wants to assert "I'm on the raster thread" should check
     * `hasBinding()` first so it doesn't fire on those platforms.
     */
    static ThreadChecker& rasterInstance() noexcept;

    /// Records the current thread as the bound thread. Called once at startup.
    void bindToCurrentThread() noexcept;

    /// Returns true if the current thread matches the bound thread.
    bool isOnBoundThread() const noexcept;

    /// Returns true if bindToCurrentThread() has been called on this instance.
    bool hasBinding() const noexcept { return bound_; }

    /**
     * @brief Crashes with a helpful message if not on the bound thread.
     *
     * No-op if bindToCurrentThread() has not yet been called (e.g. in unit
     * tests that don't call runApp()).
     */
    void assertOnBoundThread(const char* context) const noexcept;

private:
    std::thread::id bound_id_{};
    bool bound_ = false;
};

} // namespace systems::leal::campello_widgets
