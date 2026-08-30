#include <gtest/gtest.h>

#include <campello_widgets/ui/spring_simulation.hpp>
#include <campello_widgets/ui/reorder_math.hpp>

namespace cw = systems::leal::campello_widgets;

// ---------------------------------------------------------------------------
// SpringSimulation
// ---------------------------------------------------------------------------

TEST(SpringSimulation, StartsAtZeroEndsAtOne)
{
    auto curve = cw::SpringSimulation::curve(0.3, 3.0);
    EXPECT_DOUBLE_EQ(curve(0.0), 0.0);
    EXPECT_DOUBLE_EQ(curve(1.0), 1.0);
}

TEST(SpringSimulation, CriticallyDampedNeverOvershoots)
{
    // damping_ratio >= 1 -- monotonic approach, must never exceed 1.0.
    auto curve = cw::SpringSimulation::curve(1.0, 3.0);
    for (int i = 0; i <= 20; ++i)
    {
        const double t = i / 20.0;
        EXPECT_LE(curve(t), 1.0 + 1e-9) << "t=" << t;
    }
}

TEST(SpringSimulation, UnderdampedCanOvershoot)
{
    // A lightly-damped spring should overshoot 1.0 somewhere before settling.
    auto curve = cw::SpringSimulation::curve(0.1, 3.0);
    bool overshot = false;
    for (int i = 0; i <= 100; ++i)
    {
        const double t = i / 100.0;
        if (curve(t) > 1.01) { overshot = true; break; }
    }
    EXPECT_TRUE(overshot);
}

TEST(SpringSimulation, ClampsOutOfRangeInput)
{
    auto curve = cw::SpringSimulation::curve(0.5, 2.0);
    EXPECT_DOUBLE_EQ(curve(-1.0), 0.0);
    EXPECT_DOUBLE_EQ(curve(2.0), 1.0);
}

// ---------------------------------------------------------------------------
// computeReorderTargetIndex
// ---------------------------------------------------------------------------

TEST(ReorderMath, NoDragStaysAtSource)
{
    EXPECT_EQ(cw::computeReorderTargetIndex(2, 0.0f, 56.0f, 5), 2);
}

TEST(ReorderMath, DragDownByOneItemMovesDown)
{
    EXPECT_EQ(cw::computeReorderTargetIndex(1, 56.0f, 56.0f, 5), 2);
}

TEST(ReorderMath, DragUpByOneItemMovesUp)
{
    EXPECT_EQ(cw::computeReorderTargetIndex(3, -56.0f, 56.0f, 5), 2);
}

TEST(ReorderMath, DragPastEndClampsToLastIndex)
{
    EXPECT_EQ(cw::computeReorderTargetIndex(0, 1000.0f, 56.0f, 5), 4);
}

TEST(ReorderMath, DragPastStartClampsToZero)
{
    EXPECT_EQ(cw::computeReorderTargetIndex(4, -1000.0f, 56.0f, 5), 0);
}

TEST(ReorderMath, PartialDragRoundsToNearestSlot)
{
    // 30px of a 56px item -- rounds to 1 slot (>= half).
    EXPECT_EQ(cw::computeReorderTargetIndex(0, 30.0f, 56.0f, 5), 1);
    // 20px -- rounds to 0 slots (< half).
    EXPECT_EQ(cw::computeReorderTargetIndex(0, 20.0f, 56.0f, 5), 0);
}
