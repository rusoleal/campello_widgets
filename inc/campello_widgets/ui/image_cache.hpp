#pragma once

#include <campello_widgets/ui/image_provider.hpp>

#include <memory>
#include <unordered_map>
#include <mutex>
#include <chrono>

namespace systems::leal::campello_widgets
{

    /**
     * @brief LRU cache for loaded images.
     * 
     * Similar to Flutter's ImageCache, this manages memory usage by:
     * - Limiting maximum number of cached images
     * - Limiting maximum total memory usage
     * - Evicting least-recently-used entries when limits exceeded
     */
    class ImageCache
    {
    public:
        struct Entry {
            std::shared_ptr<LoadedImage> image;
            std::chrono::steady_clock::time_point last_access;
            size_t memory_usage = 0;
        };

        struct Limits {
            size_t max_images = 1000;           // Maximum number of cached images
            size_t max_memory_bytes = 100 * 1024 * 1024;  // 100 MB default
            std::chrono::seconds max_age = std::chrono::seconds(300);  // 5 minutes
        };

        static ImageCache& instance();

        /**
         * @brief Configure cache limits.
         */
        void setLimits(const Limits& limits);
        const Limits& limits() const { return limits_; }

        /**
         * @brief Get a cached image if available.
         * @return nullptr if not in cache.
         */
        std::shared_ptr<LoadedImage> get(const std::string& key);

        /**
         * @brief Add an image to the cache.
         */
        void put(const std::string& key, std::shared_ptr<LoadedImage> image);

        /**
         * @brief Remove a specific entry from the cache.
         */
        void evict(const std::string& key);

        /**
         * @brief Clear all cached images.
         */
        void clear();

        /**
         * @brief Get current cache statistics.
         */
        struct Stats {
            size_t entry_count = 0;
            size_t total_memory_bytes = 0;
            size_t hits = 0;
            size_t misses = 0;
        };
        Stats stats() const;

        /**
         * @brief Call periodically to evict expired entries.
         */
        void sweep();

    private:
        ImageCache() = default;
        ~ImageCache() = default;
        ImageCache(const ImageCache&) = delete;
        ImageCache& operator=(const ImageCache&) = delete;

        void evictLRU();
        void updateMemoryUsage();
        size_t calculateMemoryUsage(const LoadedImage& image) const;

        mutable std::mutex mutex_;
        std::unordered_map<std::string, Entry> cache_;
        Limits limits_;
        Stats stats_;
        size_t current_memory_bytes_ = 0;
    };

} // namespace systems::leal::campello_widgets
