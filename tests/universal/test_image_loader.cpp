#include <gtest/gtest.h>
#include <campello_widgets/ui/image_loader.hpp>
#include <campello_widgets/ui/image_provider.hpp>

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <string>

namespace cw = systems::leal::campello_widgets;

// These tests exercise ImageLoader::loadAsync()'s in-flight-load dedup
// directly — no GPU, no Device, no Renderer involved. Decoding runs
// entirely on ImageLoader's worker threads before any texture exists (see
// image_loader.cpp's executeTask()), so this whole path is GPU-free.

namespace
{
    // A provider whose load() blocks until the test releases it, so the
    // test can deterministically hold a decode "in flight" while issuing a
    // second loadAsync() call for the same key — instead of racing real
    // decode timing.
    class BlockingTestProvider : public cw::ImageProvider
    {
    public:
        BlockingTestProvider(std::string key, std::atomic<int>& load_count,
                              std::mutex& mtx, std::condition_variable& cv, bool& released)
            : key_(std::move(key)), load_count_(load_count),
              mtx_(mtx), cv_(cv), released_(released)
        {}

        std::shared_ptr<cw::LoadedImage> load(const cw::ImageConfiguration&) const override
        {
            load_count_.fetch_add(1);
            {
                std::unique_lock<std::mutex> lock(mtx_);
                cv_.wait(lock, [this] { return released_; });
            }

            auto image = std::make_shared<cw::LoadedImage>();
            image->decoded = std::make_shared<cw::DecodedImage>();
            image->decoded->pixels = {255, 0, 0, 255};
            image->decoded->width  = 1;
            image->decoded->height = 1;
            image->decoded->channels = 4;
            image->width  = 1;
            image->height = 1;
            image->channels = 4;
            return image;
        }

        std::string cacheKey() const override { return key_; }

        bool operator==(const cw::ImageProvider& other) const override
        {
            auto* o = dynamic_cast<const BlockingTestProvider*>(&other);
            return o && o->key_ == key_;
        }

    private:
        std::string key_;
        std::atomic<int>& load_count_;
        std::mutex& mtx_;
        std::condition_variable& cv_;
        bool& released_;
    };
} // namespace

TEST(ImageLoader, ConcurrentLoadsForSameKeyShareOneDecode)
{
    std::atomic<int> load_count{0};
    std::mutex mtx;
    std::condition_variable cv;
    bool released = false;

    // A distinctive key -- ImageLoader::instance() is a process-wide
    // singleton also touched by other tests in this binary, so this must
    // not collide with anything else's cache key.
    auto provider = std::make_shared<BlockingTestProvider>(
        "test_image_loader_dedup_key_9f3a21", load_count, mtx, cv, released);
    cw::ImageConfiguration config;

    // loadAsync() registers this key's in-flight entry synchronously
    // before returning (see loadAsync()'s in_flight_ check-then-insert,
    // both under queue_mutex_, within the call itself) -- so the second
    // call below is guaranteed to see it regardless of whether a worker
    // thread has even started executing the first task yet. No sleep/poll
    // needed to make this deterministic.
    auto future1 = cw::ImageLoader::instance().loadAsync(provider, config);
    auto future2 = cw::ImageLoader::instance().loadAsync(provider, config);

    {
        std::lock_guard<std::mutex> lock(mtx);
        released = true;
    }
    cv.notify_all();

    ASSERT_EQ(future1.wait_for(std::chrono::seconds(5)), std::future_status::ready);
    ASSERT_EQ(future2.wait_for(std::chrono::seconds(5)), std::future_status::ready);

    auto result1 = future1.get();
    auto result2 = future2.get();

    EXPECT_EQ(load_count.load(), 1)
        << "second loadAsync() for the same key triggered its own decode "
           "instead of joining the in-flight one";

    ASSERT_EQ(result1.status, cw::ImageLoadStatus::completed);
    ASSERT_EQ(result2.status, cw::ImageLoadStatus::completed);
    ASSERT_NE(result1.image, nullptr);
    ASSERT_NE(result2.image, nullptr);
    EXPECT_EQ(result1.image.get(), result2.image.get())
        << "the two callers did not receive the same LoadedImage";
}
