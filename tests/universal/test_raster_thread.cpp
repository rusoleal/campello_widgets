#include <gtest/gtest.h>
#include <campello_widgets/ui/raster_thread.hpp>

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <future>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace cw = systems::leal::campello_widgets;

// These tests exercise RasterThread's real concurrency primitives directly —
// no GPU, no Renderer, no IDrawBackend involved. FramePackage instances here
// carry an empty draw_list and a null target; only frame_id is used, purely
// to identify which package a given raster_fn_ invocation received.

TEST(RasterThread, PickupUnblocksProducerNotCompletion)
{
    std::mutex log_mutex;
    std::vector<std::string> events;
    auto logEvent = [&](const char* s)
    {
        std::lock_guard<std::mutex> lock(log_mutex);
        events.push_back(s);
    };
    auto indexOf = [&](const char* s) -> int
    {
        std::lock_guard<std::mutex> lock(log_mutex);
        for (size_t i = 0; i < events.size(); ++i)
            if (events[i] == s) return static_cast<int>(i);
        return -1;
    };

    std::mutex started_mutex;
    std::condition_variable started_cv;
    bool frame1_started = false;

    std::mutex release_mutex;
    std::condition_variable release_cv;
    bool release_frame1 = false;

    cw::RasterThread rt([&](const cw::FramePackage& pkg)
    {
        if (pkg.frame_id == 1)
        {
            logEvent("start1");
            {
                std::lock_guard<std::mutex> lock(started_mutex);
                frame1_started = true;
            }
            started_cv.notify_one();

            std::unique_lock<std::mutex> lock(release_mutex);
            release_cv.wait(lock, [&] { return release_frame1; });
            logEvent("end1");
        }
        else
        {
            logEvent("start2");
            logEvent("end2");
        }
    });
    rt.start();

    cw::FramePackage pkg1;
    pkg1.frame_id = 1;
    rt.submit(std::move(pkg1));

    {
        std::unique_lock<std::mutex> lock(started_mutex);
        ASSERT_TRUE(started_cv.wait_for(lock, std::chrono::seconds(2),
                                         [&] { return frame1_started; }));
    }

    // Frame 1 has been picked up (dequeued) and is now blocked inside
    // raster_fn_. submit() for frame 2 must return as soon as the mailbox
    // is empty — it must NOT wait for frame 1's raster_fn_ to finish.
    cw::FramePackage pkg2;
    pkg2.frame_id = 2;
    auto submit2 = std::async(std::launch::async, [&] { rt.submit(std::move(pkg2)); });
    ASSERT_EQ(submit2.wait_for(std::chrono::seconds(2)), std::future_status::ready);
    logEvent("submit2_returned");

    {
        std::lock_guard<std::mutex> lock(release_mutex);
        release_frame1 = true;
    }
    release_cv.notify_one();

    rt.stop();

    ASSERT_NE(indexOf("submit2_returned"), -1);
    ASSERT_NE(indexOf("end1"), -1);
    EXPECT_LT(indexOf("submit2_returned"), indexOf("end1"))
        << "submit() for frame 2 should unblock on frame 1's pickup, not its completion";
}

TEST(RasterThread, OrderingPreserved)
{
    std::mutex log_mutex;
    std::vector<uint64_t> observed;

    cw::RasterThread rt([&](const cw::FramePackage& pkg)
    {
        std::lock_guard<std::mutex> lock(log_mutex);
        observed.push_back(pkg.frame_id);
    });
    rt.start();

    constexpr int kFrames = 20;
    for (uint64_t i = 1; i <= kFrames; ++i)
    {
        cw::FramePackage pkg;
        pkg.frame_id = i;
        rt.submit(std::move(pkg));
    }

    rt.stop();

    std::lock_guard<std::mutex> lock(log_mutex);
    ASSERT_EQ(observed.size(), static_cast<size_t>(kFrames));
    for (uint64_t i = 0; i < kFrames; ++i)
        EXPECT_EQ(observed[i], i + 1);
}

TEST(RasterThread, StopWaitsForInFlightWork)
{
    std::atomic<bool> finished{false};

    cw::RasterThread rt([&](const cw::FramePackage&)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        finished.store(true, std::memory_order_release);
    });
    rt.start();

    cw::FramePackage pkg;
    pkg.frame_id = 1;
    rt.submit(std::move(pkg));

    rt.stop();

    EXPECT_TRUE(finished.load(std::memory_order_acquire));
}

TEST(RasterThread, StopWithNothingPendingReturnsPromptly)
{
    cw::RasterThread rt([](const cw::FramePackage&) {});
    rt.start();
    rt.stop(); // must not hang
    SUCCEED();
}
