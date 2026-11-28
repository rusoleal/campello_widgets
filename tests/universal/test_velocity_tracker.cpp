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
    t.addPosition(1'000, 10.0f);
    EXPECT_FLOAT_EQ(t.getVelocity(), 0.0f);
}

TEST(VelocityTracker, ConstantVelocityIsRecoveredExactly)
{
    cw::VelocityTracker t;
    const uint64_t t0 = 1'000;
    // 100 px/s, sampled every 10ms for 50ms.
    for (int i = 0; i <= 5; i++)
        t.addPosition(t0 + static_cast<uint64_t>(i * 10), 100.0f * (i * 0.01f));

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
    const uint64_t t0 = 1'000;

    t.addPosition(t0,      0.0f);
    t.addPosition(t0 + 10, 20.0f);
    t.addPosition(t0 + 20, 40.0f);
    t.addPosition(t0 + 30, 60.0f);
    // Final sample: same position as the previous one, 1ms later — a naive
    // last-delta estimate would compute 0px / 0.001s = 0.
    t.addPosition(t0 + 31, 60.0f);

    // True velocity over the bulk of the gesture is 2000 px/s (20px/10ms).
    // The naive last-sample estimate here would be exactly 0.
    EXPECT_GT(t.getVelocity(), 500.0f);
}

TEST(VelocityTracker, BurstyZeroDtSamplesDoNotExplodeVelocity)
{
    // Reproduces batched event delivery: several samples arrive stamped
    // with the exact same millisecond (PointerEvent::timestamp_ms is
    // millisecond-granular, so genuinely-simultaneous dispatch collapses to
    // identical timestamps, not just a near-zero gap). The regression must
    // not blow up or divide by ~0 the way a naive per-pair delta/dt would.
    cw::VelocityTracker t;
    const uint64_t t0 = 1'000;

    t.addPosition(t0, 0.0f);
    t.addPosition(t0, 0.01f);
    t.addPosition(t0, 0.02f);
    t.addPosition(t0 + 50, 100.0f);

    const float v = t.getVelocity();
    EXPECT_TRUE(std::isfinite(v));
    EXPECT_LT(std::abs(v), 100000.0f);
}

TEST(VelocityTracker, OldSamplesOutsideHorizonAreIgnored)
{
    cw::VelocityTracker t;
    const uint64_t t0 = 1'000;

    // Slow drift long ago (outside the ~100ms horizon)...
    t.addPosition(t0,       0.0f);
    t.addPosition(t0 + 500, 5.0f);
    // ...then a fast flick right at the end.
    t.addPosition(t0 + 520, 25.0f);
    t.addPosition(t0 + 540, 45.0f);

    // ~1000px/s from the recent flick; the ancient slow drift should not
    // pull this down toward its own ~10px/s rate.
    EXPECT_GT(t.getVelocity(), 500.0f);
}

TEST(VelocityTracker, ResetClearsHistory)
{
    cw::VelocityTracker t;
    const uint64_t t0 = 1'000;
    t.addPosition(t0,      0.0f);
    t.addPosition(t0 + 10, 100.0f);
    ASSERT_NE(t.getVelocity(), 0.0f);

    t.reset();
    EXPECT_FLOAT_EQ(t.getVelocity(), 0.0f);
}

// ---------------------------------------------------------------------------
// Flutter-parity behaviour (degree-2 fit, pause detection).
// ---------------------------------------------------------------------------

namespace
{
    // A flick that accelerates up to release: v(t) = 300 + 2700·(t/T)²
    // px/s over T = 120ms, sampled at 120Hz with millisecond timestamps.
    // Returns the true finger velocity at the last sample.
    float feedAcceleratingFlick(cw::VelocityTracker& t)
    {
        const double T = 0.12, hz = 120.0;
        double pos = 0.0, v = 0.0;
        for (int i = 0; i <= static_cast<int>(T * hz); ++i)
        {
            const double s = i / hz;
            v = 300.0 + 2700.0 * (s / T) * (s / T);
            if (i > 0) pos += v / hz;
            t.addPosition(1'000 + static_cast<uint64_t>(std::llround(s * 1000.0)),
                          static_cast<float>(pos));
        }
        return static_cast<float>(v);
    }
}

TEST(VelocityTracker, AcceleratingFlickReportsReleaseVelocityNotWindowAverage)
{
    // A straight-line fit reports the 100ms-window *average* (~47% of the
    // release speed for this flick), making momentum start visibly slower
    // than the finger. The quadratic fit must land close to the real
    // release speed.
    cw::VelocityTracker t;
    const float release_v = feedAcceleratingFlick(t);
    EXPECT_NEAR(t.getVelocity(), release_v, release_v * 0.10f);
}

TEST(VelocityTracker, ConstantAccelerationIsRecoveredExactly)
{
    // p(t) = 0.5·a·t² with a = 20000 px/s²: velocity at t is a·t.
    cw::VelocityTracker t;
    for (int ms = 0; ms <= 80; ms += 8)
    {
        const double s = ms / 1000.0;
        t.addPosition(1'000 + ms, static_cast<float>(0.5 * 20000.0 * s * s));
    }
    EXPECT_NEAR(t.getVelocity(), 20000.0f * 0.080f, 5.0f);
}

TEST(VelocityTracker, HoldBeforeReleaseReportsZero)
{
    // Drag fast, then hold still: no more move events arrive, then the
    // finger lifts 200ms later. Flutter treats >40ms without a sample as
    // "pointer stopped" — releasing must not fling with stale velocity.
    cw::VelocityTracker t;
    for (int i = 0; i <= 5; i++)
        t.addPosition(1'000 + i * 10, 20.0f * i);
    ASSERT_GT(t.getVelocity(), 1000.0f);
    EXPECT_FLOAT_EQ(t.getVelocity(1'050 + 200), 0.0f);
    // A release right after the last move still flings.
    EXPECT_GT(t.getVelocity(1'050 + 8), 1000.0f);
}

TEST(VelocityTracker, SamplesBeforeAPauseAreIgnored)
{
    cw::VelocityTracker t;
    // Fast motion, then a 60ms pause (no events), then slow motion.
    t.addPosition(1'000, 0.0f);
    t.addPosition(1'010, 50.0f);
    t.addPosition(1'020, 100.0f);
    t.addPosition(1'080, 101.0f);
    t.addPosition(1'090, 102.0f);
    t.addPosition(1'100, 103.0f);
    // Only the slow ~100 px/s motion after the pause counts.
    EXPECT_NEAR(t.getVelocity(), 100.0f, 20.0f);
}
