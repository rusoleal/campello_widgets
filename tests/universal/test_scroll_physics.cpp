#include <gtest/gtest.h>
#include <campello_widgets/ui/scroll_physics.hpp>

#include <cmath>

namespace cw = systems::leal::campello_widgets;

namespace
{
    // Runs a fling to completion at a fixed tick; returns total distance.
    float runFling(const cw::ScrollPhysics& p, float v, float dt, int max_ticks = 100000)
    {
        float total = 0.0f;
        for (int i = 0; i < max_ticks && std::abs(v) >= 1.0f; ++i)
            total += p.advanceMomentum(v, dt);
        return total;
    }
}

TEST(ClampingScrollPhysics, MatchesFlutterClampingScrollSimulation)
{
    // Reference values from Flutter's ClampingScrollSimulation for
    // v0 = 4000: duration = 2.358·0.35·(v0/v_ref)^(1/1.358),
    // distance = v0·duration/2.358.
    const double duration = cw::ClampingScrollPhysics::flingDuration(4000.0);
    const double distance = cw::ClampingScrollPhysics::flingDistance(4000.0);
    const double v_ref = 0.015 * (9.80665 * 39.37 * 160.0 * 0.84) / 0.35;
    const double k = std::log(0.78) / std::log(0.9);
    EXPECT_NEAR(duration, k * 0.35 * std::pow(4000.0 / v_ref, 1.0 / (k - 1.0)), 1e-9);
    EXPECT_NEAR(distance, 4000.0 * duration / k, 1e-6);

    cw::ClampingScrollPhysics p;
    float v = 4000.0f;
    float travelled = 0.0f;
    const float dt = 1.0f / 60.0f;
    int ticks = 0;
    while (v != 0.0f && ticks < 1000) { travelled += p.advanceMomentum(v, dt); ++ticks; }
    EXPECT_NEAR(travelled, distance, 0.5);
    // Ends at the fling's own duration, not by fading out.
    EXPECT_EQ(ticks, static_cast<int>(std::ceil(duration / dt)));
}

TEST(ClampingScrollPhysics, StepsAreFrameRateIndependent)
{
    cw::ClampingScrollPhysics p;
    // Same 0.3s of fling at 60Hz, 120Hz, and in one big (dropped-frame) step.
    auto run = [&](int steps) {
        float v = 3000.0f, x = 0.0f;
        for (int i = 0; i < steps; ++i) x += p.advanceMomentum(v, 0.3f / steps);
        return std::pair{x, v};
    };
    const auto a = run(18), b = run(36), c = run(1);
    EXPECT_NEAR(a.first, c.first, 0.05f);
    EXPECT_NEAR(b.first, c.first, 0.05f);
    EXPECT_NEAR(a.second, c.second, 0.05f);
}

TEST(ClampingScrollPhysics, FirstStepContinuesReleaseVelocity)
{
    // The distance covered in the first millisecond must match the release
    // velocity — no speed drop at the instant the finger lifts.
    cw::ClampingScrollPhysics p;
    float v = 2500.0f;
    const float dx = p.advanceMomentum(v, 0.001f);
    EXPECT_NEAR(dx / 0.001f, 2500.0f, 5.0f);
}

TEST(BouncingScrollPhysics, MatchesFlutterFrictionSimulation)
{
    // FrictionSimulation(0.135, 0, v0): x(t) = v0·(0.135^t − 1)/ln(0.135),
    // v(t) = v0·0.135^t.
    cw::BouncingScrollPhysics p;
    float v = 2000.0f;
    float x = 0.0f;
    for (int i = 0; i < 60; ++i) x += p.advanceMomentum(v, 1.0f / 60.0f);
    EXPECT_NEAR(v, 2000.0 * 0.135, 0.5);
    EXPECT_NEAR(x, 2000.0 * (0.135 - 1.0) / std::log(0.135), 0.5);
    // Total glide ≈ v0 / −ln(0.135).
    float v2 = 2000.0f;
    EXPECT_NEAR(runFling(p, v2, 1.0f / 60.0f), 2000.0 / -std::log(0.135), 2.0);
}

TEST(ScrollPhysics, FlingVelocityGating)
{
    cw::ClampingScrollPhysics c;
    cw::BouncingScrollPhysics b;
    cw::NeverScrollableScrollPhysics n;
    EXPECT_FLOAT_EQ(c.flingVelocity(40.0f), 0.0f);    // < 50
    EXPECT_FLOAT_EQ(c.flingVelocity(-60.0f), -60.0f);
    EXPECT_FLOAT_EQ(b.flingVelocity(80.0f), 0.0f);    // < 100 for bouncing
    EXPECT_FLOAT_EQ(c.flingVelocity(20000.0f), 8000.0f);
    EXPECT_FLOAT_EQ(c.flingVelocity(-20000.0f), -8000.0f);
    EXPECT_FLOAT_EQ(n.flingVelocity(5000.0f), 0.0f);
}
