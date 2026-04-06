#pragma once

#include <campello_widgets/ui/image_provider.hpp>
#include <campello_widgets/ui/image_cache.hpp>

#include <memory>
#include <future>
#include <thread>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <functional>

namespace systems::leal::campello_widgets
{

    /**
     * @brief Status of an image load operation.
     */
    enum class ImageLoadStatus {
        pending,    // Not started yet
        loading,    // Currently loading (network fetch or file decode)
        completed,  // Successfully loaded
        failed      // Error during loading
    };

    /**
     * @brief Result of an image load operation.
     */
    struct ImageLoadResult {
        ImageLoadStatus status = ImageLoadStatus::pending;
        std::shared_ptr<LoadedImage> image;
        std::string error_message;
    };

    /**
     * @brief Thread pool for background image loading and decoding.
     * 
     * Images are loaded asynchronously to avoid blocking the UI thread:
     * 1. File/network I/O happens on worker threads
     * 2. Image decoding (PNG/JPEG/WebP) happens on worker threads  
     * 3. GPU texture upload happens on the main thread (requires graphics context)
     */
    class ImageLoader
    {
    public:
        static ImageLoader& instance();

        /**
         * @brief Initialize the thread pool with specified number of threads.
         * @param num_threads Number of worker threads (0 = hardware concurrency)
         */
        void initialize(size_t num_threads = 0);
        
        /**
         * @brief Shutdown the thread pool and cancel pending tasks.
         */
        void shutdown();

        /**
         * @brief Asynchronously load an image from a provider.
         * 
         * Returns a future that will be resolved when the image is loaded.
         * The image is first checked in the cache, then loaded asynchronously.
         */
        std::future<ImageLoadResult> loadAsync(
            ImageProviderRef provider,
            const ImageConfiguration& config);

        /**
         * @brief Synchronously load an image (blocks calling thread).
         * 
         * Use sparingly - prefer loadAsync for UI responsiveness.
         */
        ImageLoadResult loadSync(
            ImageProviderRef provider,
            const ImageConfiguration& config);

        /**
         * @brief Cancel all pending loads for a specific provider.
         */
        void cancel(const ImageProviderRef& provider);

        /**
         * @brief Cancel all pending loads.
         */
        void cancelAll();

        /**
         * @brief Get number of pending tasks.
         */
        size_t pendingCount() const;

        /**
         * @brief Get number of active worker threads.
         */
        size_t threadCount() const { return threads_.size(); }

    private:
        ImageLoader() = default;
        ~ImageLoader();
        ImageLoader(const ImageLoader&) = delete;
        ImageLoader& operator=(const ImageLoader&) = delete;

        struct Task {
            std::string cache_key;
            ImageProviderRef provider;
            ImageConfiguration config;
            std::promise<ImageLoadResult> promise;
            std::atomic<bool> cancelled{false};
        };

        void workerLoop();
        ImageLoadResult executeTask(const Task& task);
        
        // Decoding functions
        std::shared_ptr<LoadedImage> decodePNG(const uint8_t* data, size_t size, 
                                                const ImageConfiguration& config);
        std::shared_ptr<LoadedImage> decodeJPEG(const uint8_t* data, size_t size,
                                                 const ImageConfiguration& config);
        std::shared_ptr<LoadedImage> decodeWebP(const uint8_t* data, size_t size,
                                                 const ImageConfiguration& config);
        
        // Upload decoded pixels to GPU texture (called after decoding)
        std::shared_ptr<campello_gpu::Texture> uploadToTexture(
            const uint8_t* pixels, int width, int height, int channels,
            std::shared_ptr<campello_gpu::Device> device);

        std::vector<std::thread> threads_;
        std::queue<std::shared_ptr<Task>> task_queue_;
        mutable std::mutex queue_mutex_;
        std::condition_variable condition_;
        std::atomic<bool> shutdown_{false};
        std::atomic<size_t> pending_count_{0};
    };

    /**
     * @brief Helper to decode image format from file extension or magic bytes.
     */
    enum class ImageFormat {
        unknown,
        png,
        jpeg,
        webp,
        gif,
        bmp,
        tga
    };

    ImageFormat detectImageFormat(const std::string& path);
    ImageFormat detectImageFormatFromBytes(const uint8_t* data, size_t size);

} // namespace systems::leal::campello_widgets
