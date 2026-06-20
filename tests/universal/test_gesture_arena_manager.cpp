#include <gtest/gtest.h>
#include <campello_widgets/ui/gesture_arena_manager.hpp>

namespace cw = systems::leal::campello_widgets;

namespace
{
    // Records accept/reject calls so tests can assert on arena outcomes.
    class RecordingMember : public cw::GestureArenaMember
    {
    public:
        int  accepted = 0;
        int  rejected = 0;

        void acceptGesture(int32_t /*pointer_id*/) override { ++accepted; }
        void rejectGesture(int32_t /*pointer_id*/) override { ++rejected; }
    };
} // namespace

struct GestureArenaFixture : public ::testing::Test
{
    cw::GestureArenaManager mgr;

    void SetUp() override { cw::GestureArenaManager::setActive(&mgr); }
    void TearDown() override { cw::GestureArenaManager::setActive(nullptr); }
};

// -----------------------------------------------------------------------
// Global accessor
// -----------------------------------------------------------------------

TEST_F(GestureArenaFixture, GlobalAccessorReturnsSetInstance)
{
    EXPECT_EQ(cw::GestureArenaManager::active(), &mgr);
}

TEST(GestureArenaManager, GlobalAccessorNullAfterClear)
{
    cw::GestureArenaManager::setActive(nullptr);
    EXPECT_EQ(cw::GestureArenaManager::active(), nullptr);
}

// -----------------------------------------------------------------------
// Sole member resolves by default
// -----------------------------------------------------------------------

TEST_F(GestureArenaFixture, SingleMemberWinsOnClose)
{
    RecordingMember a;
    mgr.add(1, &a);
    mgr.close(1);

    EXPECT_EQ(a.accepted, 1);
    EXPECT_EQ(a.rejected, 0);
}

TEST_F(GestureArenaFixture, SingleMemberNotResolvedBeforeClose)
{
    RecordingMember a;
    mgr.add(1, &a);

    EXPECT_EQ(a.accepted, 0);
    EXPECT_EQ(a.rejected, 0);
}

// -----------------------------------------------------------------------
// Explicit accept ("first recognizer to claim wins")
// -----------------------------------------------------------------------

TEST_F(GestureArenaFixture, ExplicitAcceptWhileOpenIsDeferredUntilClose)
{
    RecordingMember a, b;
    auto entry_a = mgr.add(1, &a);
    mgr.add(1, &b);

    entry_a.resolve(cw::GestureDisposition::accepted);
    EXPECT_EQ(a.accepted, 0); // arena still open — third member could yet join
    EXPECT_EQ(b.rejected, 0);

    mgr.close(1);
    EXPECT_EQ(a.accepted, 1);
    EXPECT_EQ(b.rejected, 1);
}

TEST_F(GestureArenaFixture, ExplicitAcceptAfterCloseResolvesImmediately)
{
    RecordingMember a, b;
    auto entry_a = mgr.add(1, &a);
    mgr.add(1, &b);
    mgr.close(1);

    entry_a.resolve(cw::GestureDisposition::accepted);

    EXPECT_EQ(a.accepted, 1);
    EXPECT_EQ(b.rejected, 1);
}

TEST_F(GestureArenaFixture, FirstEagerWinnerSticksWhenSecondAlsoAccepts)
{
    RecordingMember a, b;
    auto entry_a = mgr.add(1, &a);
    auto entry_b = mgr.add(1, &b);

    entry_a.resolve(cw::GestureDisposition::accepted);
    entry_b.resolve(cw::GestureDisposition::accepted); // too late, a already claimed eager win
    mgr.close(1);

    EXPECT_EQ(a.accepted, 1);
    EXPECT_EQ(b.rejected, 1);
    EXPECT_EQ(b.accepted, 0);
}

// -----------------------------------------------------------------------
// Explicit reject
// -----------------------------------------------------------------------

TEST_F(GestureArenaFixture, RejectLeavesRemainingMemberToWinAfterClose)
{
    RecordingMember a, b;
    auto entry_a = mgr.add(1, &a);
    mgr.add(1, &b);

    entry_a.resolve(cw::GestureDisposition::rejected);
    EXPECT_EQ(a.rejected, 1);
    EXPECT_EQ(b.accepted, 0); // arena still open

    mgr.close(1);
    EXPECT_EQ(b.accepted, 1);
}

TEST_F(GestureArenaFixture, RejectAfterCloseResolvesRemainingMemberImmediately)
{
    RecordingMember a, b;
    auto entry_a = mgr.add(1, &a);
    mgr.add(1, &b);
    mgr.close(1);

    entry_a.resolve(cw::GestureDisposition::rejected);

    EXPECT_EQ(a.rejected, 1);
    EXPECT_EQ(b.accepted, 1);
}

TEST_F(GestureArenaFixture, RejectingDownToOneMemberInClosedArenaWinsByDefault)
{
    // Rejecting the second-to-last member in a *closed* arena immediately
    // resolves the sole survivor — it doesn't wait for sweep().
    RecordingMember a, b;
    auto entry_a = mgr.add(1, &a);
    mgr.add(1, &b);
    mgr.close(1);

    entry_a.resolve(cw::GestureDisposition::rejected);

    EXPECT_EQ(a.rejected, 1);
    EXPECT_EQ(b.accepted, 1);
}

TEST_F(GestureArenaFixture, AllMembersRejectingBeforeCloseLeavesNoWinner)
{
    // Both reject while the arena is still open, so no auto-resolution
    // happens yet; close() then finds zero members and quietly discards
    // the arena instead of crashing or picking a "winner" out of thin air.
    RecordingMember a, b;
    auto entry_a = mgr.add(1, &a);
    auto entry_b = mgr.add(1, &b);

    entry_a.resolve(cw::GestureDisposition::rejected);
    entry_b.resolve(cw::GestureDisposition::rejected);
    mgr.close(1);

    EXPECT_EQ(a.rejected, 1);
    EXPECT_EQ(b.rejected, 1);
    EXPECT_EQ(a.accepted, 0);
    EXPECT_EQ(b.accepted, 0);

    // Arena is gone; sweep() must be a no-op, not double-fire callbacks.
    mgr.sweep(1);
    EXPECT_EQ(a.rejected, 1);
    EXPECT_EQ(b.rejected, 1);
}

// -----------------------------------------------------------------------
// Sweep ("last recognizer standing wins")
// -----------------------------------------------------------------------

TEST_F(GestureArenaFixture, SweepPicksFirstMemberAddedWhenUnresolved)
{
    RecordingMember a, b, c;
    mgr.add(1, &a);
    mgr.add(1, &b);
    mgr.add(1, &c);
    mgr.close(1);

    mgr.sweep(1);

    EXPECT_EQ(a.accepted, 1);
    EXPECT_EQ(b.rejected, 1);
    EXPECT_EQ(c.rejected, 1);
}

TEST_F(GestureArenaFixture, SweepIsNoOpIfArenaAlreadyResolved)
{
    RecordingMember a, b;
    auto entry_a = mgr.add(1, &a);
    mgr.add(1, &b);
    mgr.close(1);

    entry_a.resolve(cw::GestureDisposition::accepted); // resolves immediately, arena removed

    mgr.sweep(1); // should not re-fire callbacks

    EXPECT_EQ(a.accepted, 1);
    EXPECT_EQ(b.rejected, 1);
}

TEST_F(GestureArenaFixture, SweepOnUnknownPointerIsNoOp)
{
    EXPECT_NO_THROW(mgr.sweep(42));
}

// -----------------------------------------------------------------------
// Independent pointers
// -----------------------------------------------------------------------

TEST_F(GestureArenaFixture, ArenasForDifferentPointersAreIndependent)
{
    RecordingMember a, b;
    auto entry_a = mgr.add(1, &a);
    mgr.add(2, &b);

    entry_a.resolve(cw::GestureDisposition::accepted);
    mgr.close(1);

    EXPECT_EQ(a.accepted, 1);
    EXPECT_EQ(b.accepted, 0);
    EXPECT_EQ(b.rejected, 0);

    mgr.close(2);
    EXPECT_EQ(b.accepted, 1);
}
