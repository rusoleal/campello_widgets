#include <gtest/gtest.h>
#include <campello_widgets/ui/velocity_tracker.hpp>

namespace cw = systems::leal::campello_widgets;

TEST(VelocityTracker, NoSamplesReturnsZero)
{
    cw::VelocityTracker t;
    EXPECT_FLOAT_EQ(t.getVelocity(), 0.0f);
}

TEST(VelocityTracker, SingleSampleReturnsZero)
{
    cw::VelocityTracker t;
    t.addPosition(std::chrono::steady_clock::now(), 10.0f);
    EXPECT_FLOAT_EQ(t.getVelocity(), 0.0f);
}

TEST(VelocityTracker, ConstantVelocityIsRecoveredExactly)
{
    cw::VelocityTracker t;
    const auto t0 = std::chrono::steady_clock::now();
    // 100 px/s, sampled every 10ms for 50ms.
    for (int i = 0; i <= 5; i++)
        t.addPosition(t0 + std::chrono::milliseconds(i * 10), 100.0f * (i * 0.01f));

    EXPECT_NEAR(t.getVelocity(), 100.0f, 1.0f);
}

TEST(VelocityTracker, FinalNearZeroDeltaDoesNotZeroOutVelocity)
{
    // Reproduces the reported bug: a fast drag whose very last sample (the
    // one right before release) has a near-zero delta from the one before
    // it — either from natural deceleration or bursty event delivery.
    // A single-sample delta/dt estimate would report ~0 here; the
    // regression-based tracker should still see the real fling.
    cw::VelocityTracker t;
    const auto t0 = std::chrono::steady_clock::now();

    t.addPosition(t0 + std::chrono::milliseconds(0),  0.0f);
    t.addPosition(t0 + std::chrono::milliseconds(10), 20.0f);
    t.addPosition(t0 + std::chrono::milliseconds(20), 40.0f);
    t.addPosition(t0 + std::chrono::milliseconds(30), 60.0f);
    // Final sample: only 0.1px later, 1ms after the previous one — a naive
    // last-delta estimate would compute 0.1px / 0.001s = 100px/s... in this
    // contrived case that's coincidentally not zero, so use an even more
    // pathological case: same position as the previous sample.
    t.addPosition(t0 + std::chrono::milliseconds(31), 60.0f);

    // True velocity over the bulk of the gesture is 2000 px/s (20px/10ms).
    // The naive last-sample estimate here would be exactly 0.
    EXPECT_GT(t.getVelocity(), 500.0f);
}

TEST(VelocityTracker, BurstyZeroDtSamplesDoNotExplodeVelocity)
{
    // Reproduces batched event delivery: several samples arrive with the
    // same (or a microscopic) timestamp gap. The regression must not blow
    // up or divide by ~0 the way a naive per-pair delta/dt would.
    cw::VelocityTracker t;
    const auto t0 = std::chrono::steady_clock::now();

    t.addPosition(t0, 0.0f);
    t.addPosition(t0 + std::chrono::microseconds(1), 0.01f);
    t.addPosition(t0 + std::chrono::microseconds(2), 0.02f);
    t.addPosition(t0 + std::chrono::milliseconds(50), 100.0f);

    const float v = t.getVelocity();
    EXPECT_TRUE(std::isfinite(v));
    EXPECT_LT(std::abs(v), 100000.0f);
}

TEST(VelocityTracker, OldSamplesOutsideHorizonAreIgnored)
{
    cw::VelocityTracker t;
    const auto t0 = std::chrono::steady_clock::now();

    // Slow drift long ago (outside the ~100ms horizon)...
    t.addPosition(t0, 0.0f);
    t.addPosition(t0 + std::chrono::milliseconds(500), 5.0f);
    // ...then a fast flick right at the end.
    t.addPosition(t0 + std::chrono::milliseconds(520), 25.0f);
    t.addPosition(t0 + std::chrono::milliseconds(540), 45.0f);

    // ~1000px/s from the recent flick; the ancient slow drift should not
    // pull this down toward its own ~10px/s rate.
    EXPECT_GT(t.getVelocity(), 500.0f);
}

TEST(VelocityTracker, ResetClearsHistory)
{
    cw::VelocityTracker t;
    const auto t0 = std::chrono::steady_clock::now();
    t.addPosition(t0, 0.0f);
    t.addPosition(t0 + std::chrono::milliseconds(10), 100.0f);
    ASSERT_NE(t.getVelocity(), 0.0f);

    t.reset();
    EXPECT_FLOAT_EQ(t.getVelocity(), 0.0f);
}
