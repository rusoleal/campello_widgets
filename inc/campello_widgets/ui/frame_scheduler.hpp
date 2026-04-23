#pragma once

#include <functional>

namespace systems::leal::campello_widgets
{

/**
 * @brief Thin scheduler that requests exactly one display frame from the
 *        platform whenever the widget tree becomes dirty.
 *
 * This mirrors Flutter's SchedulerBinding.scheduleFrame() contract:
 *
 *   1. The platform entry point registers a callback once at startup via
 *      setCallback().  On macOS the callback calls [MTKView setNeedsDisplay:YES].
 *
 *   2. Any code that makes the widget tree dirty — setState(), markNeedsPaint(),
 *      or a new animation ticker subscription — calls scheduleFrame().
 *
 *   3. The platform callback is inherently idempotent (calling setNeedsDisplay:YES
 *      while a frame is already pending is a no-op), so scheduleFrame() may be
 *      called as many times as needed without queuing multiple redraws.
 *
 *   4. After the frame is drawn the platform display link goes quiet.  No more
 *      frames are produced until the next scheduleFrame() call — exactly like
 *      Flutter's idle behaviour.
 */
class FrameScheduler
{
public:
    /// Register the platform-specific "request one frame" callback.
    /// Called once at application startup from the platform entry point
    /// (e.g. run_app.mm on macOS).
    static void setCallback(std::function<void()> callback);

    /// Request that the platform draw one frame.
    /// Safe to call many times per event cycle — the underlying platform
    /// primitive (setNeedsDisplay:YES, invalidate(), etc.) is idempotent.
    static void scheduleFrame();

private:
    static std::function<void()> s_callback_;
};

} // namespace systems::leal::campello_widgets
