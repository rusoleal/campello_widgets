#include <gtest/gtest.h>
#include <campello_widgets/ui/offset_layer.hpp>
#include <campello_widgets/ui/paint_context.hpp>
#include <campello_widgets/ui/paint.hpp>
#include <campello_widgets/ui/color.hpp>
#include <campello_widgets/ui/rect.hpp>
#include <campello_widgets/ui/draw_command.hpp>
#include <campello_widgets/ui/image_filter.hpp>
#include <variant>

namespace cw = systems::leal::campello_widgets;

namespace
{
    // Draws one DrawRectCmd at a fixed local rect, independent of `offset` —
    // mirrors how a real RenderObject's performPaint() bakes offset-based
    // geometry directly into the command, not via canvas.translate().
    void paintPlainRect(cw::PaintContext& ctx, int& invocations)
    {
        ++invocations;
        ctx.canvas().drawRect(cw::Rect::fromLTWH(10.0f, 10.0f, 40.0f, 30.0f),
            cw::Paint::filled(cw::Color::black()));
    }

    void paintClippedRect(cw::PaintContext& ctx, int& invocations)
    {
        ++invocations;
        ctx.canvas().save();
        ctx.canvas().clipRect(cw::Rect::fromLTWH(0.0f, 0.0f, 100.0f, 100.0f));
        ctx.canvas().drawRect(cw::Rect::fromLTWH(10.0f, 10.0f, 40.0f, 30.0f),
            cw::Paint::filled(cw::Color::black()));
        ctx.canvas().restore();
    }

    void paintBackdropFilterContent(cw::PaintContext& ctx, int& invocations)
    {
        ++invocations;
        ctx.canvas().beginBackdropFilter(
            cw::Rect::fromLTWH(0.0f, 0.0f, 100.0f, 100.0f), cw::ImageFilter::blur(8.0f));
        ctx.canvas().drawRect(cw::Rect::fromLTWH(10.0f, 10.0f, 40.0f, 30.0f),
            cw::Paint::filled(cw::Color::black()));
        ctx.canvas().endBackdropFilter();
    }
} // namespace

TEST(OffsetLayer, FirstPaintRecordsAndEmitsCommands)
{
    cw::OffsetLayer layer;
    int invocations = 0;

    cw::PaintContext ctx(200.0f, 200.0f);
    ASSERT_FALSE(layer.maybeReplay(ctx, cw::Offset{0.0f, 0.0f}, cw::Size{60.0f, 50.0f}, /*dirty=*/false));
    layer.record(ctx, cw::Offset{0.0f, 0.0f}, [&] { paintPlainRect(ctx, invocations); });

    EXPECT_EQ(invocations, 1);
    EXPECT_FALSE(ctx.commands().empty());
}

TEST(OffsetLayer, IdentityReplaySkipsReRecording)
{
    cw::OffsetLayer layer;
    int invocations = 0;
    const cw::Offset fixedOffset{5.0f, 5.0f};

    cw::PaintContext ctx1(200.0f, 200.0f);
    layer.record(ctx1, fixedOffset, [&] { paintPlainRect(ctx1, invocations); });
    ASSERT_EQ(invocations, 1);

    cw::PaintContext ctx2(200.0f, 200.0f);
    const bool replayed = layer.maybeReplay(ctx2, fixedOffset, cw::Size{60.0f, 50.0f}, /*dirty=*/false);

    EXPECT_TRUE(replayed);
    EXPECT_EQ(invocations, 1) << "identity replay must not re-invoke paintContent";
    EXPECT_FALSE(ctx2.commands().empty());
}

TEST(OffsetLayer, DirtyForcesReRecording)
{
    cw::OffsetLayer layer;
    int invocations = 0;
    const cw::Offset fixedOffset{0.0f, 0.0f};

    cw::PaintContext ctx1(200.0f, 200.0f);
    layer.record(ctx1, fixedOffset, [&] { paintPlainRect(ctx1, invocations); });
    ASSERT_EQ(invocations, 1);

    cw::PaintContext ctx2(200.0f, 200.0f);
    const bool replayed = layer.maybeReplay(ctx2, fixedOffset, cw::Size{60.0f, 50.0f}, /*dirty=*/true);

    EXPECT_FALSE(replayed);
    if (!replayed)
        layer.record(ctx2, fixedOffset, [&] { paintPlainRect(ctx2, invocations); });

    EXPECT_EQ(invocations, 2) << "a dirty layer must force a fresh recording";
}

TEST(OffsetLayer, RepositionWithClipFreeContentReplaysViaDeltaTranslate)
{
    cw::OffsetLayer layer;
    int invocations = 0;
    const cw::Offset firstOffset{10.0f, 10.0f};
    const cw::Offset secondOffset{30.0f, 15.0f};

    cw::PaintContext ctx1(200.0f, 200.0f);
    layer.record(ctx1, firstOffset, [&] { paintPlainRect(ctx1, invocations); });
    ASSERT_EQ(invocations, 1);

    cw::PaintContext ctx2(200.0f, 200.0f);
    const bool replayed = layer.maybeReplay(ctx2, secondOffset, cw::Size{60.0f, 50.0f}, /*dirty=*/false);

    ASSERT_TRUE(replayed) << "clip-free content should replay via a cheap delta translate";
    EXPECT_EQ(invocations, 1) << "reposition must not re-invoke paintContent when geometry is safe";

    // Expect a PushTransformCmd carrying the delta translate, wrapping the
    // original (unmodified) DrawRectCmd.
    const auto& cmds = ctx2.commands();
    ASSERT_FALSE(cmds.empty());
    const auto* pushTransform = std::get_if<cw::PushTransformCmd>(&cmds.front());
    ASSERT_NE(pushTransform, nullptr)
        << "delta reposition must wrap replayed content in a fresh PushTransformCmd";

    const cw::Matrix4 expectedDelta = cw::Matrix4::translate(
        {secondOffset.x - firstOffset.x, secondOffset.y - firstOffset.y, 0.0f});
    for (int i = 0; i < 16; ++i)
        EXPECT_FLOAT_EQ(pushTransform->transform.data[i], expectedDelta.data[i]);

    bool foundOriginalRect = false;
    for (const auto& c : cmds)
    {
        if (const auto* rectCmd = std::get_if<cw::DrawRectCmd>(&c))
        {
            EXPECT_FLOAT_EQ(rectCmd->rect.x, 10.0f)
                << "replayed geometry must be untouched — repositioning happens via the "
                   "wrapping transform, not by re-baking the cached rect";
            foundOriginalRect = true;
        }
    }
    EXPECT_TRUE(foundOriginalRect);
}

TEST(OffsetLayer, RepositionWithUnsafeGeometryForcesReRecording)
{
    cw::OffsetLayer layer;
    int invocations = 0;
    const cw::Offset firstOffset{10.0f, 10.0f};
    const cw::Offset secondOffset{30.0f, 15.0f};

    cw::PaintContext ctx1(200.0f, 200.0f);
    layer.record(ctx1, firstOffset, [&] { paintClippedRect(ctx1, invocations); });
    ASSERT_EQ(invocations, 1);

    cw::PaintContext ctx2(200.0f, 200.0f);
    const bool replayed = layer.maybeReplay(ctx2, secondOffset, cw::Size{60.0f, 50.0f}, /*dirty=*/false);

    EXPECT_FALSE(replayed)
        << "content containing a clip must fall back to a full re-record on reposition, "
           "matching PaintCache's existing behavior";

    if (!replayed)
        layer.record(ctx2, secondOffset, [&] { paintClippedRect(ctx2, invocations); });

    EXPECT_EQ(invocations, 2);

    // A third paint at the same (second) offset should now replay cleanly.
    cw::PaintContext ctx3(200.0f, 200.0f);
    const bool replayedAgain = layer.maybeReplay(ctx3, secondOffset, cw::Size{60.0f, 50.0f}, /*dirty=*/false);
    EXPECT_TRUE(replayedAgain);
    EXPECT_EQ(invocations, 2) << "unchanged offset should replay the cache again";
}

TEST(OffsetLayer, BackdropFilterContentNeverReplaysEvenAtUnchangedOffset)
{
    // Regression test for "BackdropFilter inside a scroll doesn't respect
    // offset": RenderBackdropFilter::performPaint() has a side effect
    // (Renderer::noteBackdropFilter()) that must fire every frame this
    // content is visible, or the Renderer's full-viewport backdrop-capture
    // pass silently stops updating — the destination quad still tracks the
    // ambient transform correctly (so it *looks* like it's scrolling), but
    // the blurred content it samples goes stale/frozen, since the capture
    // pass that refreshes it never re-runs. An identity replay at an
    // unchanged offset (exactly what happens when an ancestor scrolls
    // without repositioning this boundary's own logical offset) must
    // therefore still force a fresh recording, unlike ordinary content.
    cw::OffsetLayer layer;
    int invocations = 0;
    const cw::Offset fixedOffset{0.0f, 0.0f};

    cw::PaintContext ctx1(200.0f, 200.0f);
    layer.record(ctx1, fixedOffset, [&] { paintBackdropFilterContent(ctx1, invocations); });
    ASSERT_EQ(invocations, 1);
    ASSERT_FALSE(ctx1.commands().empty());

    // Clean, unchanged offset — an ordinary picture would replay here.
    cw::PaintContext ctx2(200.0f, 200.0f);
    const bool replayed = layer.maybeReplay(ctx2, fixedOffset, cw::Size{60.0f, 50.0f}, /*dirty=*/false);

    EXPECT_FALSE(replayed)
        << "backdrop-filter content must never replay, even at an identity offset";

    if (!replayed)
        layer.record(ctx2, fixedOffset, [&] { paintBackdropFilterContent(ctx2, invocations); });

    EXPECT_EQ(invocations, 2)
        << "paintContent (and therefore noteBackdropFilter()) must re-run every frame";

    // A third, still-clean, still-unchanged-offset paint must keep forcing
    // a fresh recording — this is not a one-time fallback.
    cw::PaintContext ctx3(200.0f, 200.0f);
    const bool replayedAgain = layer.maybeReplay(ctx3, fixedOffset, cw::Size{60.0f, 50.0f}, /*dirty=*/false);
    EXPECT_FALSE(replayedAgain);
    if (!replayedAgain)
        layer.record(ctx3, fixedOffset, [&] { paintBackdropFilterContent(ctx3, invocations); });
    EXPECT_EQ(invocations, 3);
}
