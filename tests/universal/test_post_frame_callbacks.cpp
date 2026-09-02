#include <gtest/gtest.h>
#include <campello_widgets/ui/post_frame_callbacks.hpp>

namespace cw = systems::leal::campello_widgets;

TEST(PostFrameCallbacks, RunPendingFiresScheduledCallbackOnce)
{
    int calls = 0;
    cw::PostFrameCallbacks::schedule([&] { ++calls; });

    cw::PostFrameCallbacks::runPending();
    EXPECT_EQ(calls, 1);

    // Nothing pending anymore -- a second runPending() is a no-op.
    cw::PostFrameCallbacks::runPending();
    EXPECT_EQ(calls, 1);
}

TEST(PostFrameCallbacks, RunPendingWithNothingScheduledIsANoOp)
{
    EXPECT_NO_THROW(cw::PostFrameCallbacks::runPending());
}

TEST(PostFrameCallbacks, MultiplePendingCallbacksAllFire)
{
    std::vector<int> order;
    cw::PostFrameCallbacks::schedule([&] { order.push_back(1); });
    cw::PostFrameCallbacks::schedule([&] { order.push_back(2); });
    cw::PostFrameCallbacks::schedule([&] { order.push_back(3); });

    cw::PostFrameCallbacks::runPending();
    ASSERT_EQ(order.size(), 3u);
    EXPECT_EQ(order[0], 1);
    EXPECT_EQ(order[1], 2);
    EXPECT_EQ(order[2], 3);
}

TEST(PostFrameCallbacks, CallbackThatSchedulesAnotherDefersToNextRunPending)
{
    // A callback that calls schedule() again during dispatch must NOT run
    // that new callback immediately (it would if runPending() drained a
    // live reference to s_pending_ instead of a swapped-out local copy) --
    // it should only fire on the *next* runPending() call, matching
    // Flutter's real post-frame-callback semantics.
    int first_calls  = 0;
    int second_calls = 0;

    cw::PostFrameCallbacks::schedule([&] {
        ++first_calls;
        cw::PostFrameCallbacks::schedule([&] { ++second_calls; });
    });

    cw::PostFrameCallbacks::runPending();
    EXPECT_EQ(first_calls, 1);
    EXPECT_EQ(second_calls, 0); // deferred, not run yet

    cw::PostFrameCallbacks::runPending();
    EXPECT_EQ(first_calls, 1);
    EXPECT_EQ(second_calls, 1); // now it runs
}
