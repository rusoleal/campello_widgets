#include <campello_widgets/ui/image_loader.hpp>
#include <campello_widgets/ui/image_cache.hpp>

#include <iostream>

namespace systems::leal::campello_widgets
{


    ImageLoader::~ImageLoader()
    {
        shutdown();
    }

    ImageLoader& ImageLoader::instance()
    {
        static ImageLoader instance;
        return instance;
    }

    void ImageLoader::initialize(size_t num_threads)
    {
        if (num_threads == 0) {
            num_threads = std::max(1u, std::thread::hardware_concurrency());
        }

        std::lock_guard<std::mutex> lock(queue_mutex_);
        
        if (!threads_.empty()) {
            return;  // Already initialized
        }

        shutdown_ = false;

        for (size_t i = 0; i < num_threads; ++i) {
            threads_.emplace_back([this]() { workerLoop(); });
        }
    }

    void ImageLoader::shutdown()
    {
        {
            std::lock_guard<std::mutex> lock(queue_mutex_);
            shutdown_ = true;
            
            // Mark all pending tasks as cancelled
            auto queue_copy = task_queue_;
            while (!queue_copy.empty()) {
                auto task = queue_copy.front();
                queue_copy.pop();
                task->cancelled.store(true);
            }
        }
        
        condition_.notify_all();

        for (auto& thread : threads_) {
            if (thread.joinable()) {
                thread.join();
            }
        }

        std::lock_guard<std::mutex> lock(queue_mutex_);
        threads_.clear();
        
        // Clear pending queue
        while (!task_queue_.empty()) {
            auto task = task_queue_.front();
            task_queue_.pop();

            ImageLoadResult result;
            result.status = ImageLoadStatus::failed;
            result.error_message = "Loader shut down";
            task->promise.set_value(std::move(result));
            in_flight_.erase(task->cache_key);
        }

        pending_count_ = 0;
    }

    std::shared_future<ImageLoadResult> ImageLoader::loadAsync(
        ImageProviderRef provider,
        const ImageConfiguration& config)
    {
        // Ensure initialized
        if (threads_.empty()) {
            initialize();
        }

        const std::string cache_key = provider->cacheKey();

        // Check cache first
        if (auto cached = ImageCache::instance().get(cache_key)) {
            ImageLoadResult result;
            result.status = ImageLoadStatus::completed;
            result.image = cached;
            std::promise<ImageLoadResult> promise;
            std::shared_future<ImageLoadResult> future = promise.get_future().share();
            promise.set_value(std::move(result));
            return future;
        }

        {
            std::lock_guard<std::mutex> lock(queue_mutex_);
            // Someone else is already loading this exact image — join
            // their load instead of starting a redundant decode + GPU
            // upload. See in_flight_'s doc comment.
            auto it = in_flight_.find(cache_key);
            if (it != in_flight_.end()) {
                return it->second;
            }
        }

        auto task = std::make_shared<Task>();
        task->cache_key = cache_key;
        task->provider = provider;
        task->config = config;

        std::shared_future<ImageLoadResult> future = task->promise.get_future().share();

        {
            std::lock_guard<std::mutex> lock(queue_mutex_);
            in_flight_[cache_key] = future;
            task_queue_.push(task);
            pending_count_++;
        }

        condition_.notify_one();
        return future;
    }

    ImageLoadResult ImageLoader::loadSync(
        ImageProviderRef provider,
        const ImageConfiguration& config)
    {
        // Check cache first
        auto cache_key = provider->cacheKey();
        if (auto cached = ImageCache::instance().get(cache_key)) {
            ImageLoadResult result;
            result.status = ImageLoadStatus::completed;
            result.image = cached;
            return result;
        }

        try {
            auto image = provider->load(config);
            
            // Add to cache
            ImageCache::instance().put(cache_key, image);
            
            ImageLoadResult result;
            result.status = ImageLoadStatus::completed;
            result.image = std::move(image);
            return result;
        } catch (const std::exception& e) {
            ImageLoadResult result;
            result.status = ImageLoadStatus::failed;
            result.error_message = e.what();
            return result;
        }
    }

    void ImageLoader::cancel(const ImageProviderRef& provider)
    {
        std::lock_guard<std::mutex> lock(queue_mutex_);
        
        // Mark tasks matching this provider as cancelled
        auto queue_copy = task_queue_;
        while (!queue_copy.empty()) {
            auto task = queue_copy.front();
            queue_copy.pop();
            
            if (task->provider == provider) {
                task->cancelled.store(true);
            }
        }
    }

    void ImageLoader::cancelAll()
    {
        std::lock_guard<std::mutex> lock(queue_mutex_);
        
        auto queue_copy = task_queue_;
        while (!queue_copy.empty()) {
            auto task = queue_copy.front();
            queue_copy.pop();
            task->cancelled.store(true);
        }
    }

    size_t ImageLoader::pendingCount() const
    {
        return pending_count_.load();
    }

    void ImageLoader::workerLoop()
    {
        while (true) {
            std::shared_ptr<Task> task;

            {
                std::unique_lock<std::mutex> lock(queue_mutex_);
                condition_.wait(lock, [this]() {
                    return shutdown_ || !task_queue_.empty();
                });

                if (shutdown_ && task_queue_.empty()) {
                    return;
                }

                if (task_queue_.empty()) {
                    continue;
                }

                task = task_queue_.front();
                task_queue_.pop();
                pending_count_--;
            }

            if (task->cancelled.load()) {
                ImageLoadResult result;
                result.status = ImageLoadStatus::failed;
                result.error_message = "Cancelled";
                task->promise.set_value(std::move(result));
                {
                    std::lock_guard<std::mutex> lock(queue_mutex_);
                    in_flight_.erase(task->cache_key);
                }
                continue;
            }

            auto result = executeTask(*task);
            task->promise.set_value(std::move(result));
            {
                // Remove now that the future holds a settled result — a
                // later, non-overlapping loadAsync() for this same key
                // should re-check ImageCache rather than join a future
                // that's already done (harmless either way, but this
                // keeps the map from growing with every distinct image
                // ever loaded for the app's whole lifetime).
                std::lock_guard<std::mutex> lock(queue_mutex_);
                in_flight_.erase(task->cache_key);
            }
        }
    }

    ImageLoadResult ImageLoader::executeTask(const Task& task)
    {
        // Double-check cache (might have been loaded by another thread)
        if (auto cached = ImageCache::instance().get(task.cache_key)) {
            ImageLoadResult result;
            result.status = ImageLoadStatus::completed;
            result.image = cached;
            return result;
        }

        try {
            auto image = task.provider->load(task.config);

            // Deliberately NOT cached here: this runs on a worker thread, before
            // any GPU texture exists (texture creation must happen on the main
            // thread — see ImageWidgetState::checkFuture()), and ImageCache::put()
            // requires image->texture to be set. The caller caches the fully-ready
            // image (decoded pixels consumed, texture attached) once it's actually
            // usable.
            ImageLoadResult result;
            result.status = ImageLoadStatus::completed;
            result.image = std::move(image);
            return result;
        } catch (const std::exception& e) {
            std::cerr << "[ImageLoader] Provider load failed: " << e.what() << "\n";
            ImageLoadResult result;
            result.status = ImageLoadStatus::failed;
            result.error_message = e.what();
            return result;
        }
    }

} // namespace systems::leal::campello_widgets
