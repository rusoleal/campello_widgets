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

// Regression coverage for the clip-shape/shader-mask GPU compositing cache
// (Renderer::clip_shape_gpu_cache_/shader_mask_gpu_cache_): an identity
// replay must be bracketed with CacheReplayBeginCmd/EndCmd carrying this
// OffsetLayer's own address, so Renderer::flushDrawList() can recognize
// any ClipRRect/ClipOval/ShaderMask inside as guaranteed-unchanged and
// reuse its cached GPU composite. See CacheReplayBeginCmd's doc comment.
TEST(OffsetLayer, IdentityReplayIsBracketedWithCacheReplayMarkers)
{
    cw::OffsetLayer layer;
    int invocations = 0;
    const cw::Offset fixedOffset{5.0f, 5.0f};

    cw::PaintContext ctx1(200.0f, 200.0f);
    layer.record(ctx1, fixedOffset, [&] { paintPlainRect(ctx1, invocations); });
    ASSERT_EQ(invocations, 1);
    const size_t recordedCount = ctx1.commands().size();

    cw::PaintContext ctx2(200.0f, 200.0f);
    const bool replayed = layer.maybeReplay(ctx2, fixedOffset, cw::Size{60.0f, 50.0f}, /*dirty=*/false);
    ASSERT_TRUE(replayed);

    const auto& cmds = ctx2.commands();
    ASSERT_EQ(cmds.size(), recordedCount + 2);

    const auto* beginCmd = std::get_if<cw::CacheReplayBeginCmd>(&cmds.front());
    ASSERT_NE(beginCmd, nullptr) << "identity replay must open with CacheReplayBeginCmd";
    EXPECT_EQ(beginCmd->region_id, static_cast<const void*>(&layer))
        << "region_id must be this OffsetLayer's own address";

    EXPECT_NE(std::get_if<cw::CacheReplayEndCmd>(&cmds.back()), nullptr)
        << "identity replay must close with CacheReplayEndCmd";
}

// The delta-translate reposition path is content the GPU clip-shape/
// shader-mask/save-layer/shadow caches can still reuse — only *where* it's
// drawn moved, not the content itself — so it's bracketed exactly like an
// identity replay (see CacheReplayBeginCmd's doc comment and
// PictureLayer's doc comment for why every GPU-cacheable consumer already
// re-applies the ambient transform, which now includes this reposition, on
// every use regardless of cache hit).
TEST(OffsetLayer, DeltaTranslateReplayIsBracketed)
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
    ASSERT_TRUE(replayed);

    bool sawBegin = false, sawEnd = false;
    for (const auto& c : ctx2.commands())
    {
        if (std::get_if<cw::CacheReplayBeginCmd>(&c)) sawBegin = true;
        if (std::get_if<cw::CacheReplayEndCmd>(&c))   sawEnd   = true;
    }
    EXPECT_TRUE(sawBegin) << "delta-translate replay must be wrapped in cache-replay markers";
    EXPECT_TRUE(sawEnd)   << "delta-translate replay must be wrapped in cache-replay markers";
}

// The riskiest piece of the cache-replay design (see the clip-shape GPU
// cache plan's "Risk notes"): a replayed boundary whose cached content
// itself contains another boundary that was replayed at record time.
// Getting the region-stack tracking wrong could attribute a clip-shape to
// the wrong region's cache entry and serve stale/wrong content — so the
// nested markers must survive being re-wrapped by an outer replay, intact
// and distinguishable by their own region_id.
TEST(OffsetLayer, NestedReplayRegionsBracketCorrectly)
{
    cw::OffsetLayer inner;
    cw::OffsetLayer outer;
    int inner_invocations = 0;
    int outer_invocations = 0;
    const cw::Offset innerOffset{5.0f, 5.0f};
    const cw::Offset outerOffset{0.0f, 0.0f};

    // Prime inner with a first recording, independent of outer.
    {
        cw::PaintContext primer(200.0f, 200.0f);
        inner.record(primer, innerOffset, [&] { paintPlainRect(primer, inner_invocations); });
    }
    ASSERT_EQ(inner_invocations, 1);

    // Outer's first recording: its content replays inner (clean, same
    // offset — a genuine identity replay, itself bracketed) plus one rect
    // of its own.
    cw::PaintContext ctx1(200.0f, 200.0f);
    outer.record(ctx1, outerOffset, [&] {
        ++outer_invocations;
        const bool inner_replayed =
            inner.maybeReplay(ctx1, innerOffset, cw::Size{60.0f, 50.0f}, /*dirty=*/false);
        ASSERT_TRUE(inner_replayed);
        ctx1.canvas().drawRect(cw::Rect::fromLTWH(0.0f, 0.0f, 5.0f, 5.0f),
            cw::Paint::filled(cw::Color::white()));
    });
    ASSERT_EQ(outer_invocations, 1);
    ASSERT_EQ(inner_invocations, 1) << "inner replayed, should not re-invoke its paintContent";

    // Outer's second paint: identity replay at the same offset must
    // bracket the *entire* cached content — which already contains inner's
    // own nested bracket — in outer's own markers, without calling
    // inner.maybeReplay() at all this time (it's baked into outer's cached
    // picture verbatim).
    cw::PaintContext ctx2(200.0f, 200.0f);
    const bool outer_replayed =
        outer.maybeReplay(ctx2, outerOffset, cw::Size{100.0f, 100.0f}, /*dirty=*/false);
    ASSERT_TRUE(outer_replayed);
    EXPECT_EQ(outer_invocations, 1) << "outer identity replay must not re-invoke paintContent";
    EXPECT_EQ(inner_invocations, 1) << "outer identity replay must not re-invoke inner either";

    const auto& cmds = ctx2.commands();
    ASSERT_GE(cmds.size(), 4u);

    const auto* outerBegin = std::get_if<cw::CacheReplayBeginCmd>(&cmds.front());
    ASSERT_NE(outerBegin, nullptr);
    EXPECT_EQ(outerBegin->region_id, static_cast<const void*>(&outer));
    EXPECT_NE(std::get_if<cw::CacheReplayEndCmd>(&cmds.back()), nullptr);

    bool found_inner_begin = false;
    for (size_t i = 1; i + 1 < cmds.size(); ++i)
    {
        if (const auto* b = std::get_if<cw::CacheReplayBeginCmd>(&cmds[i]))
        {
            if (b->region_id == static_cast<const void*>(&inner))
                found_inner_begin = true;
        }
    }
    EXPECT_TRUE(found_inner_begin)
        << "nested replay region markers must survive being re-wrapped by an outer replay";
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

// PushClipRectCmd is the one absolute-geometry command OffsetLayer can
// still cheaply reposition (see PictureLayer's doc comment): its stored
// rect gets shifted by hand instead of forcing a full re-record.
TEST(OffsetLayer, RepositionWithClipRectShiftsClipGeometryAndReplays)
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

    EXPECT_TRUE(replayed)
        << "a plain clip rect is cheaply repositionable — it must not force a full re-record";
    EXPECT_EQ(invocations, 1) << "a successful reposition-replay must not re-invoke paintContent";

    // paintClippedRect() clips to (0,0,100,100) at record time; the replay
    // must carry that rect shifted by (secondOffset - firstOffset) = (20,5).
    const cw::PushClipRectCmd* clip = nullptr;
    for (const auto& c : ctx2.commands())
        if (const auto* pc = std::get_if<cw::PushClipRectCmd>(&c)) { clip = pc; break; }

    ASSERT_NE(clip, nullptr) << "replayed content must still carry the clip command";
    EXPECT_FLOAT_EQ(clip->rect.x, 20.0f);
    EXPECT_FLOAT_EQ(clip->rect.y, 5.0f);
    EXPECT_FLOAT_EQ(clip->rect.width, 100.0f);
    EXPECT_FLOAT_EQ(clip->rect.height, 100.0f);

    // A third paint at the same (second) offset should now replay cleanly
    // via the identity-replay path.
    cw::PaintContext ctx3(200.0f, 200.0f);
    const bool replayedAgain = layer.maybeReplay(ctx3, secondOffset, cw::Size{60.0f, 50.0f}, /*dirty=*/false);
    EXPECT_TRUE(replayedAgain);
    EXPECT_EQ(invocations, 1) << "unchanged offset should replay the cache again";
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
