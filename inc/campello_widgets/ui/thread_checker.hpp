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
    static ThreadChecker& instance() noexcept;

    /// Records the current thread as the UI thread. Called once at startup.
    void bindToCurrentThread() noexcept;

    /// Returns true if the current thread matches the bound UI thread.
    bool isOnBoundThread() const noexcept;

    /**
     * @brief Crashes with a helpful message if not on the UI thread.
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
