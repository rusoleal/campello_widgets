#include <gtest/gtest.h>
#include <campello_widgets/ui/render_repaint_boundary.hpp>
#include <campello_widgets/ui/render_clip_rect.hpp>
#include <campello_widgets/ui/render_box.hpp>
#include <campello_widgets/ui/box_constraints.hpp>
#include <campello_widgets/ui/paint_context.hpp>
#include <campello_widgets/ui/paint.hpp>
#include <campello_widgets/ui/color.hpp>
#include <campello_widgets/ui/rect.hpp>
#include <variant>

namespace cw = systems::leal::campello_widgets;

namespace
{
    // A leaf RenderBox with a fixed size that counts how many times
    // performPaint() actually runs, and emits one DrawRectCmd per call so
    // tests can also check whether commands made it into the output list
    // (via a fresh paint or a replayed cache) independent of the counter.
    class CountingRenderBox : public cw::RenderBox
    {
    public:
        int paintCount = 0;

        void performLayout() override
        {
            size_ = constraints_.constrain(cw::Size{40.0f, 30.0f});
        }

        void performPaint(cw::PaintContext& context, const cw::Offset& offset) override
        {
            ++paintCount;
            context.canvas().drawRect(
                cw::Rect::fromLTWH(offset.x, offset.y, size_.width, size_.height),
                cw::Paint::filled(cw::Color::black()));
        }
    };
} // namespace

TEST(RenderRepaintBoundary, FirstPaintRecordsAndEmitsCommands)
{
    auto child = std::make_shared<CountingRenderBox>();
    cw::RenderRepaintBoundary boundary;
    boundary.setChild(child);
    boundary.layout(cw::BoxConstraints::loose(200.0f, 200.0f));

    cw::PaintContext ctx(200.0f, 200.0f);
    boundary.paint(ctx, cw::Offset{0.0f, 0.0f});

    EXPECT_EQ(child->paintCount, 1);
    EXPECT_FALSE(ctx.commands().empty());
}

TEST(RenderRepaintBoundary, CleanRepaintReplaysCacheWithoutWalkingChild)
{
    auto child = std::make_shared<CountingRenderBox>();
    cw::RenderRepaintBoundary boundary;
    boundary.setChild(child);
    boundary.layout(cw::BoxConstraints::loose(200.0f, 200.0f));

    // Offset is held constant across both paints — see the class doc
    // comment: a clean replay assumes its on-screen offset hasn't moved.
    const cw::Offset fixedOffset{10.0f, 20.0f};

    cw::PaintContext ctx1(200.0f, 200.0f);
    boundary.paint(ctx1, fixedOffset);
    ASSERT_EQ(child->paintCount, 1);
    const size_t firstCommandCount = ctx1.commands().size();
    ASSERT_FALSE(firstCommandCount == 0);
    const auto& firstRect = std::get<cw::DrawRectCmd>(ctx1.commands().back());

    // Boundary is now clean (needsPaint() == false). Repainting it again
    // (as would happen because some unrelated sibling elsewhere marked the
    // shared root dirty) must not re-walk the child...
    ASSERT_FALSE(boundary.needsPaint());
    cw::PaintContext ctx2(200.0f, 200.0f);
    boundary.paint(ctx2, fixedOffset);

    EXPECT_EQ(child->paintCount, 1) << "clean boundary should replay its cache, not call performPaint() again";
    // ...but the replayed commands must still be present, at the exact same
    // geometry — this is the regression test for the bug where a clip rect
    // recorded against a fabricated local origin and translated on replay
    // landed at the wrong position (draw geometry is transform-deferred and
    // followed the translate; PushClipRectCmd bakes an absolute rect at
    // record time and didn't, clipping the content out of place).
    ASSERT_EQ(ctx2.commands().size(), firstCommandCount);
    const auto& replayedRect = std::get<cw::DrawRectCmd>(ctx2.commands().back());
    EXPECT_FLOAT_EQ(replayedRect.rect.x, firstRect.rect.x);
    EXPECT_FLOAT_EQ(replayedRect.rect.y, firstRect.rect.y);
}

TEST(RenderRepaintBoundary, MarkNeedsPaintForcesReRecording)
{
    auto child = std::make_shared<CountingRenderBox>();
    cw::RenderRepaintBoundary boundary;
    boundary.setChild(child);
    boundary.layout(cw::BoxConstraints::loose(200.0f, 200.0f));

    cw::PaintContext ctx1(200.0f, 200.0f);
    boundary.paint(ctx1, cw::Offset{0.0f, 0.0f});
    ASSERT_EQ(child->paintCount, 1);

    boundary.markNeedsPaint();
    ASSERT_TRUE(boundary.needsPaint());

    cw::PaintContext ctx2(200.0f, 200.0f);
    boundary.paint(ctx2, cw::Offset{0.0f, 0.0f});

    EXPECT_EQ(child->paintCount, 2) << "explicitly dirtying the boundary should force a fresh recording";
}

TEST(RenderRepaintBoundary, CleanReplayKeepsClipRectAtCorrectAbsolutePosition)
{
    // Regression test for the bug class this boundary is most at risk of:
    // PushClipRectCmd bakes an *absolute* rect at record time (unlike draw
    // geometry, which is transform-deferred to flush time). A child wrapped
    // in RenderClipRect (the same shape as production's ClipRect-around-
    // viewport-content usage) exercises that path directly.
    auto child = std::make_shared<CountingRenderBox>();
    auto clip  = std::make_shared<cw::RenderClipRect>();
    clip->setChild(child);

    cw::RenderRepaintBoundary boundary;
    boundary.setChild(clip);
    boundary.layout(cw::BoxConstraints::loose(200.0f, 200.0f));

    const cw::Offset realOffset{37.0f, 53.0f};

    cw::PaintContext ctx1(200.0f, 200.0f);
    boundary.paint(ctx1, realOffset);
    ASSERT_FALSE(boundary.needsPaint());

    auto findClipRect = [](const cw::DrawList& cmds) -> const cw::Rect* {
        for (const auto& cmd : cmds)
            if (std::holds_alternative<cw::PushClipRectCmd>(cmd))
                return &std::get<cw::PushClipRectCmd>(cmd).rect;
        return nullptr;
    };

    const cw::Rect* firstClip = findClipRect(ctx1.commands());
    ASSERT_NE(firstClip, nullptr);
    // The clip must be positioned at the real offset, not at a fabricated
    // local origin of (0,0).
    EXPECT_FLOAT_EQ(firstClip->x, realOffset.x);
    EXPECT_FLOAT_EQ(firstClip->y, realOffset.y);

    // Clean replay (same offset) must preserve that — this is exactly what
    // broke when the cache was recorded locally and relocated via a
    // wrapping translate() on replay: draw geometry followed the translate,
    // but the already-absolute clip rect didn't.
    cw::PaintContext ctx2(200.0f, 200.0f);
    boundary.paint(ctx2, realOffset);
    ASSERT_EQ(child->paintCount, 1) << "should have replayed the cache, not re-walked the child";

    const cw::Rect* replayedClip = findClipRect(ctx2.commands());
    ASSERT_NE(replayedClip, nullptr);
    EXPECT_FLOAT_EQ(replayedClip->x, realOffset.x);
    EXPECT_FLOAT_EQ(replayedClip->y, realOffset.y);
}

TEST(RenderRepaintBoundary, RepositionWithoutMarkNeedsPaintForcesReRecordingAtNewOffset)
{
    // Regression test for the bug where a Row/Column repositions a child
    // (e.g. on window resize) without calling markNeedsPaint() on it —
    // correct for every other RenderObject (which always repaints at the
    // fresh offset regardless), but RenderRepaintBoundary's clean-replay
    // path used to trust the *old* cached offset, leaving the boundary
    // visually stuck at its old position, unclipped to its new bounds.
    auto child = std::make_shared<CountingRenderBox>();
    auto clip  = std::make_shared<cw::RenderClipRect>();
    clip->setChild(child);

    cw::RenderRepaintBoundary boundary;
    boundary.setChild(clip);
    boundary.layout(cw::BoxConstraints::loose(200.0f, 200.0f));

    const cw::Offset firstOffset{10.0f, 10.0f};
    cw::PaintContext ctx1(200.0f, 200.0f);
    boundary.paint(ctx1, firstOffset);
    ASSERT_EQ(child->paintCount, 1);
    ASSERT_FALSE(boundary.needsPaint());

    // Reposition WITHOUT calling markNeedsPaint() — exactly what a Row's
    // performLayout() does when a sibling's size change shifts this
    // child's offset.
    const cw::Offset secondOffset{60.0f, 10.0f};
    ASSERT_FALSE(boundary.needsPaint());
    cw::PaintContext ctx2(200.0f, 200.0f);
    boundary.paint(ctx2, secondOffset);

    EXPECT_EQ(child->paintCount, 2)
        << "an offset change must force a fresh recording, not a stale replay";

    auto findClipRect = [](const cw::DrawList& cmds) -> const cw::Rect* {
        for (const auto& cmd : cmds)
            if (std::holds_alternative<cw::PushClipRectCmd>(cmd))
                return &std::get<cw::PushClipRectCmd>(cmd).rect;
        return nullptr;
    };

    const cw::Rect* clipAtSecondOffset = findClipRect(ctx2.commands());
    ASSERT_NE(clipAtSecondOffset, nullptr);
    EXPECT_FLOAT_EQ(clipAtSecondOffset->x, secondOffset.x)
        << "clip must reflect the new offset, not the stale cached one";
    EXPECT_FLOAT_EQ(clipAtSecondOffset->y, secondOffset.y);

    // A third paint at the same (second) offset should now replay cleanly.
    cw::PaintContext ctx3(200.0f, 200.0f);
    boundary.paint(ctx3, secondOffset);
    EXPECT_EQ(child->paintCount, 2) << "unchanged offset should replay the cache again";
}

TEST(RenderRepaintBoundary, ConstraintsChangeForcesReRecording)
{
    auto child = std::make_shared<CountingRenderBox>();
    cw::RenderRepaintBoundary boundary;
    boundary.setChild(child);
    boundary.layout(cw::BoxConstraints::loose(200.0f, 200.0f));

    cw::PaintContext ctx1(200.0f, 200.0f);
    boundary.paint(ctx1, cw::Offset{0.0f, 0.0f});
    ASSERT_EQ(child->paintCount, 1);
    ASSERT_FALSE(boundary.needsPaint());

    // A different incoming constraint marks needs-paint via layout()'s own
    // must_repaint -> markNeedsPaint() path (RenderObject::layout()).
    boundary.layout(cw::BoxConstraints::loose(50.0f, 50.0f));
    ASSERT_TRUE(boundary.needsPaint());

    cw::PaintContext ctx2(50.0f, 50.0f);
    boundary.paint(ctx2, cw::Offset{0.0f, 0.0f});

    EXPECT_EQ(child->paintCount, 2);
}
