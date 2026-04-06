#include <campello_widgets/ui/image_loader.hpp>
#include <campello_widgets/ui/image_cache.hpp>

#include <iostream>

namespace systems::leal::campello_widgets
{

    namespace {
        void log(const char* msg) {
            std::cerr << "[ImageLoader] " << msg << "\n";
        }
    }

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
        }
        
        pending_count_ = 0;
    }

    std::future<ImageLoadResult> ImageLoader::loadAsync(
        ImageProviderRef provider,
        const ImageConfiguration& config)
    {
        std::cerr << "[ImageLoader] loadAsync called\n";
        
        // Ensure initialized
        if (threads_.empty()) {
            std::cerr << "[ImageLoader] Initializing thread pool\n";
            initialize();
        }

        auto task = std::make_shared<Task>();
        task->cache_key = provider->cacheKey();
        task->provider = provider;
        task->config = config;

        // Check cache first
        if (auto cached = ImageCache::instance().get(task->cache_key)) {
            std::cerr << "[ImageLoader] Cache hit, returning immediately\n";
            ImageLoadResult result;
            result.status = ImageLoadStatus::completed;
            result.image = cached;
            task->promise.set_value(std::move(result));
            return task->promise.get_future();
        }

        std::cerr << "[ImageLoader] Queueing task for " << task->cache_key << "\n";
        std::future<ImageLoadResult> future = task->promise.get_future();

        {
            std::lock_guard<std::mutex> lock(queue_mutex_);
            task_queue_.push(task);
            pending_count_++;
        }

        condition_.notify_one();
        std::cerr << "[ImageLoader] Task queued, notifying worker\n";
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
        std::cerr << "[ImageLoader] Worker thread started\n";
        while (true) {
            std::shared_ptr<Task> task;

            {
                std::unique_lock<std::mutex> lock(queue_mutex_);
                std::cerr << "[ImageLoader] Worker waiting for task...\n";
                condition_.wait(lock, [this]() {
                    return shutdown_ || !task_queue_.empty();
                });

                if (shutdown_ && task_queue_.empty()) {
                    std::cerr << "[ImageLoader] Worker shutting down\n";
                    return;
                }

                if (task_queue_.empty()) {
                    continue;
                }

                task = task_queue_.front();
                task_queue_.pop();
                pending_count_--;
                std::cerr << "[ImageLoader] Worker got task: " << task->cache_key << "\n";
            }

            if (task->cancelled.load()) {
                ImageLoadResult result;
                result.status = ImageLoadStatus::failed;
                result.error_message = "Cancelled";
                task->promise.set_value(std::move(result));
                continue;
            }

            // Execute the load
            std::cerr << "[ImageLoader] Executing task...\n";
            auto result = executeTask(*task);
            std::cerr << "[ImageLoader] Task complete, status=" << (int)result.status << "\n";
            task->promise.set_value(std::move(result));
        }
    }

    ImageLoadResult ImageLoader::executeTask(const Task& task)
    {
        std::cerr << "[ImageLoader] executeTask: " << task.cache_key << "\n";
        
        // Double-check cache (might have been loaded by another thread)
        if (auto cached = ImageCache::instance().get(task.cache_key)) {
            std::cerr << "[ImageLoader] Cache hit in executeTask\n";
            ImageLoadResult result;
            result.status = ImageLoadStatus::completed;
            result.image = cached;
            return result;
        }

        try {
            std::cerr << "[ImageLoader] Calling provider->load()...\n";
            auto image = task.provider->load(task.config);
            std::cerr << "[ImageLoader] Provider load complete, image=" << (image ? "yes" : "no") 
                      << " size=" << image->width << "x" << image->height << "\n";
            
            // Add to cache
            ImageCache::instance().put(task.cache_key, image);
            
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
