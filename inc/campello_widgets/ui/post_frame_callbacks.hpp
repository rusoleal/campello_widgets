#pragma once

#include <functional>
#include <mutex>
#include <vector>

namespace systems::leal::campello_widgets
{

    /**
     * @brief One-shot callbacks that run once the current (or next) frame's
     * build+layout+paint pass has finished -- mirrors Flutter's
     * `WidgetsBinding.instance.addPostFrameCallback`.
     *
     * Unlike `FrameScheduler` (a persistent "please redraw" latch, invoked
     * over and over by every `scheduleFrame()` call and never consumed),
     * this is drain-on-run: `runPending()` swaps out the pending list under
     * lock, then invokes each callback outside the lock -- both to avoid
     * deadlocking a callback that calls `schedule()` again, and to
     * correctly defer any such re-scheduled callback to the *next*
     * `runPending()` call rather than running it immediately, since the
     * swap already happened before dispatch begins.
     *
     * Pumped once per frame by `Renderer::buildFrame()`, on every exit path
     * (including when nothing needed painting) -- see its call sites.
     */
    class PostFrameCallbacks
    {
    public:
        /** @brief Runs `callback` once, after the current or next frame finishes its paint pass. */
        static void schedule(std::function<void()> callback);

        /** @brief Invoked by Renderer::buildFrame(). Swaps out and runs all pending callbacks. */
        static void runPending();

    private:
        static std::mutex                         s_mutex_;
        static std::vector<std::function<void()>> s_pending_;
    };

} // namespace systems::leal::campello_widgets
