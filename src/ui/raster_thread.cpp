#include <campello_widgets/ui/raster_thread.hpp>
#include <campello_widgets/ui/thread_checker.hpp>

namespace systems::leal::campello_widgets
{

    RasterThread::RasterThread(RasterFn raster_fn)
        : raster_fn_(std::move(raster_fn))
    {
    }

    RasterThread::~RasterThread()
    {
        stop();
    }

    void RasterThread::start()
    {
        if (running_) return;
        running_ = true;
        worker_  = std::thread(&RasterThread::workerLoop, this);
    }

    void RasterThread::stop()
    {
        if (!running_) return;

        {
            std::lock_guard<std::mutex> lock(mutex_);
            stop_requested_ = true;
        }
        slot_filled_cv_.notify_all();
        slot_empty_cv_.notify_all();

        if (worker_.joinable())
            worker_.join();

        running_ = false;
    }

    void RasterThread::submit(FramePackage package)
    {
        std::unique_lock<std::mutex> lock(mutex_);
        // Wait until the worker has picked up any previous package (mailbox empty).
        slot_empty_cv_.wait(lock, [&] { return !pending_.has_value() || stop_requested_; });
        if (stop_requested_) return;
        pending_ = std::move(package);
        lock.unlock();
        slot_filled_cv_.notify_one();
    }

    bool RasterThread::isRunning() const noexcept
    {
        std::lock_guard<std::mutex> lock(mutex_);
        return running_;
    }

    void RasterThread::workerLoop()
    {
        ThreadChecker::rasterInstance().bindToCurrentThread();

        for (;;)
        {
            FramePackage pkg;
            {
                std::unique_lock<std::mutex> lock(mutex_);
                slot_filled_cv_.wait(lock, [&] { return pending_.has_value() || stop_requested_; });
                if (stop_requested_ && !pending_.has_value())
                    return;
                pkg = std::move(*pending_);
                pending_.reset(); // "pickup" happens here, before raster_fn_ runs
            }
            slot_empty_cv_.notify_one(); // unblocks a UI thread waiting in submit()
            raster_fn_(pkg);             // may run concurrently with the UI thread
                                          // already building the next frame
        }
    }

} // namespace systems::leal::campello_widgets
