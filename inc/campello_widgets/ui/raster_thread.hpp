#pragma once

#include <campello_widgets/ui/frame_package.hpp>

#include <condition_variable>
#include <functional>
#include <mutex>
#include <optional>
#include <thread>

namespace systems::leal::campello_widgets
{

    /**
     * @brief Owns the raster worker thread and a depth-1 handoff mailbox.
     *
     * Flutter-style but depth-1, not depth-2: `submit()` blocks the calling
     * (UI) thread until the raster thread has *picked up* (dequeued) the
     * previously submitted package — not until raster has finished
     * processing it. This means the UI thread may start building frame N+1
     * while frame N is still being encoded/submitted/presented on the
     * raster thread, but it can never get more than one frame ahead.
     *
     * Mirrors the existing worker-thread convention used elsewhere in this
     * codebase (see `ImageLoader`: mutex + condition_variable + worker
     * loop), rather than introducing a new pattern.
     *
     * Thread-safety: `submit()` must only be called from one thread (the
     * UI thread). The worker thread is created in `start()` and joined in
     * `stop()` or the destructor.
     */
    class RasterThread
    {
    public:
        using RasterFn = std::function<void(const FramePackage&)>;

        explicit RasterThread(RasterFn raster_fn);
        ~RasterThread();

        RasterThread(const RasterThread&) = delete;
        RasterThread& operator=(const RasterThread&) = delete;

        /// Starts the worker thread. Call once, from the UI thread, after
        /// construction and before the first submit().
        void start();

        /**
         * @brief Blocks until any in-flight package has been picked up,
         *        signals the worker to exit after draining, and joins the
         *        thread.
         *
         * Safe to call from the UI thread during app teardown. No-op if
         * not started or already stopped. Also called automatically by the
         * destructor.
         */
        void stop();

        /**
         * @brief Hands off `package` to the raster thread.
         *
         * Blocks the calling thread until the *previous* package (if any)
         * has been dequeued by the worker — i.e. until the worker has
         * picked it up, NOT until the worker has finished processing it.
         * This is the depth-1 back-pressure mechanism.
         *
         * Must be called from the UI thread only.
         */
        void submit(FramePackage package);

        /// True if start() was called and stop() has not completed.
        bool isRunning() const noexcept;

    private:
        void workerLoop();

        RasterFn    raster_fn_;
        std::thread worker_;

        mutable std::mutex      mutex_;
        std::condition_variable slot_empty_cv_;  // signaled when pending_ becomes empty (picked up)
        std::condition_variable slot_filled_cv_; // signaled when pending_ becomes non-empty
        std::optional<FramePackage> pending_;
        bool stop_requested_ = false;
        bool running_        = false;
    };

} // namespace systems::leal::campello_widgets
