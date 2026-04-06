#include <campello_widgets/ui/image_cache.hpp>

namespace systems::leal::campello_widgets
{

    ImageCache& ImageCache::instance()
    {
        static ImageCache instance;
        return instance;
    }

    void ImageCache::setLimits(const Limits& limits)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        limits_ = limits;
        
        // Evict if over new limits
        while (cache_.size() > limits_.max_images || 
               current_memory_bytes_ > limits_.max_memory_bytes) {
            evictLRU();
        }
    }

    std::shared_ptr<LoadedImage> ImageCache::get(const std::string& key)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        
        auto it = cache_.find(key);
        if (it == cache_.end()) {
            stats_.misses++;
            return nullptr;
        }

        // Check if expired
        auto now = std::chrono::steady_clock::now();
        auto age = std::chrono::duration_cast<std::chrono::seconds>(
            now - it->second.last_access);
        
        if (age > limits_.max_age) {
            cache_.erase(it);
            stats_.misses++;
            return nullptr;
        }

        // Update last access time
        it->second.last_access = now;
        stats_.hits++;
        
        return it->second.image;
    }

    void ImageCache::put(const std::string& key, std::shared_ptr<LoadedImage> image)
    {
        if (!image || !image->texture) {
            return;
        }

        std::lock_guard<std::mutex> lock(mutex_);

        // Calculate memory usage
        size_t memory = calculateMemoryUsage(*image);

        // Evict entries if necessary
        while ((!cache_.empty() && 
                (cache_.size() >= limits_.max_images || 
                 current_memory_bytes_ + memory > limits_.max_memory_bytes)) &&
               cache_.size() > 0) {
            evictLRU();
        }

        // Insert new entry
        Entry entry;
        entry.image = std::move(image);
        entry.last_access = std::chrono::steady_clock::now();
        entry.memory_usage = memory;

        auto it = cache_.find(key);
        if (it != cache_.end()) {
            // Replace existing - subtract old memory
            current_memory_bytes_ -= it->second.memory_usage;
        }

        cache_[key] = std::move(entry);
        current_memory_bytes_ += memory;
    }

    void ImageCache::evict(const std::string& key)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        
        auto it = cache_.find(key);
        if (it != cache_.end()) {
            current_memory_bytes_ -= it->second.memory_usage;
            cache_.erase(it);
        }
    }

    void ImageCache::clear()
    {
        std::lock_guard<std::mutex> lock(mutex_);
        cache_.clear();
        current_memory_bytes_ = 0;
    }

    ImageCache::Stats ImageCache::stats() const
    {
        std::lock_guard<std::mutex> lock(mutex_);
        Stats s = stats_;
        s.entry_count = cache_.size();
        s.total_memory_bytes = current_memory_bytes_;
        return s;
    }

    void ImageCache::sweep()
    {
        std::lock_guard<std::mutex> lock(mutex_);
        
        auto now = std::chrono::steady_clock::now();
        
        for (auto it = cache_.begin(); it != cache_.end();) {
            auto age = std::chrono::duration_cast<std::chrono::seconds>(
                now - it->second.last_access);
            
            if (age > limits_.max_age) {
                current_memory_bytes_ -= it->second.memory_usage;
                it = cache_.erase(it);
            } else {
                ++it;
            }
        }
    }

    void ImageCache::evictLRU()
    {
        if (cache_.empty()) {
            return;
        }

        // Find oldest entry
        auto oldest = cache_.begin();
        for (auto it = cache_.begin(); it != cache_.end(); ++it) {
            if (it->second.last_access < oldest->second.last_access) {
                oldest = it;
            }
        }

        current_memory_bytes_ -= oldest->second.memory_usage;
        cache_.erase(oldest);
    }

    size_t ImageCache::calculateMemoryUsage(const LoadedImage& image) const
    {
        // Estimate memory usage: width * height * 4 bytes for RGBA
        return static_cast<size_t>(image.width) * 
               static_cast<size_t>(image.height) * 4;
    }

} // namespace systems::leal::campello_widgets
