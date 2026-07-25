#include <gtest/gtest.h>
#include <campello_widgets/ui/render_repaint_boundary.hpp>
#include <campello_widgets/ui/render_backdrop_filter.hpp>
#include <campello_widgets/ui/render_box.hpp>
#include <campello_widgets/ui/box_constraints.hpp>
#include <campello_widgets/ui/paint_context.hpp>
#include <campello_widgets/ui/renderer.hpp>
#include <campello_widgets/ui/paint.hpp>
#include <campello_widgets/ui/color.hpp>
#include <campello_widgets/ui/rect.hpp>

namespace cw = systems::leal::campello_widgets;

namespace
{
    // A trivial single-child pass-through RenderBox — stands in for
    // "everything above the nearest repaint boundary" (e.g. the rest of a
    // page's layout) with no caching of its own, so any propagation past a
    // boundary would show up here.
    class WrapperRenderBox : public cw::RenderBox
    {
    public:
        void performLayout() override
        {
            if (auto* c = child()) { size_ = layoutChild(*c, constraints_); positionChild(*c, cw::Offset::zero()); }
            else size_ = constraints_.constrain(cw::Size::zero());
        }

        void performPaint(cw::PaintContext& context, const cw::Offset& offset) override
        {
            paintChild(context, offset);
        }
    };

    class LeafRenderBox : public cw::RenderBox
    {
    public:
        void performLayout() override { size_ = constraints_.constrain(cw::Size{10.0f, 10.0f}); }
        void performPaint(cw::PaintContext& context, const cw::Offset& offset) override
        {
            context.canvas().drawRect(
                cw::Rect::fromLTWH(offset.x, offset.y, size_.width, size_.height),
                cw::Paint::filled(cw::Color::black()));
        }
    };

    // Two independent children laid out and painted directly at fixed
    // offsets, bypassing RenderBox's single-child (`child_`/`setChild()`)
    // assumptions entirely — lets a test dirty one subtree while leaving
    // the other (positioned far away on screen) completely untouched.
    class TwoChildRenderBox : public cw::RenderBox
    {
    public:
        void setChildren(std::shared_ptr<cw::RenderBox> a, cw::Offset a_offset,
                          std::shared_ptr<cw::RenderBox> b, cw::Offset b_offset)
        {
            a_ = std::move(a); a_offset_ = a_offset; a_->setParent(this);
            b_ = std::move(b); b_offset_ = b_offset; b_->setParent(this);
        }

        void performLayout() override
        {
            a_->layout(cw::BoxConstraints::loose(1000.0f, 1000.0f));
            b_->layout(cw::BoxConstraints::loose(1000.0f, 1000.0f));
            size_ = constraints_.constrain(cw::Size{1000.0f, 2000.0f});
        }

        void performPaint(cw::PaintContext& context, const cw::Offset& offset) override
        {
            a_->paint(context, offset + a_offset_);
            b_->paint(context, offset + b_offset_);
        }

    private:
        std::shared_ptr<cw::RenderBox> a_;
        std::shared_ptr<cw::RenderBox> b_;
        cw::Offset                     a_offset_;
        cw::Offset                     b_offset_;
    };
} // namespace

TEST(RenderObjectPaintPropagation, StopsAtRepaintBoundaryAncestor)
{
    auto leaf     = std::make_shared<LeafRenderBox>();
    auto boundary = std::make_shared<cw::RenderRepaintBoundary>();
    auto wrapper  = std::make_shared<WrapperRenderBox>();

    boundary->setChild(leaf);
    wrapper->setChild(boundary);
    wrapper->layout(cw::BoxConstraints::loose(200.0f, 200.0f));

    cw::PaintContext ctx(200.0f, 200.0f);
    wrapper->paint(ctx, cw::Offset::zero());

    // All clean after the first paint (boundary resets its own flag; the
    // leaf and wrapper go through RenderObject::paint()'s base reset).
    ASSERT_FALSE(wrapper->needsPaint());
    ASSERT_FALSE(boundary->needsPaint());
    ASSERT_FALSE(leaf->needsPaint());

    leaf->markNeedsPaint();

    EXPECT_TRUE(leaf->needsPaint())
        << "the node that was actually dirtied must be marked dirty";
    EXPECT_TRUE(boundary->needsPaint())
        << "propagation must still reach the nearest repaint boundary — it "
           "owns the paint cache and must know to re-record, not replay";
    EXPECT_FALSE(wrapper->needsPaint())
        << "propagation must stop at the boundary — an ancestor above it "
           "has no reason to know a descendant changed, since the boundary "
           "will handle re-recording its own subtree independently";
}

TEST(RenderObjectPaintPropagation, MarkingTheBoundaryItselfStillWorks)
{
    auto leaf     = std::make_shared<LeafRenderBox>();
    auto boundary = std::make_shared<cw::RenderRepaintBoundary>();
    auto wrapper  = std::make_shared<WrapperRenderBox>();

    boundary->setChild(leaf);
    wrapper->setChild(boundary);
    wrapper->layout(cw::BoxConstraints::loose(200.0f, 200.0f));

    cw::PaintContext ctx(200.0f, 200.0f);
    wrapper->paint(ctx, cw::Offset::zero());
    ASSERT_FALSE(wrapper->needsPaint());

    boundary->markNeedsPaint();

    EXPECT_TRUE(boundary->needsPaint());
    EXPECT_FALSE(wrapper->needsPaint())
        << "a repaint boundary marking itself dirty must also stop right "
           "there, not propagate further up just because the dirty node "
           "happens to be the boundary itself";
}

// Renderer::buildFrame() can no longer use root_->needsPaint() as its
// "does anything need painting" gate, since a dirty leaf under a boundary
// never reaches root anymore — see Renderer::notePaintRequested()'s doc.
// Constructed with device=nullptr: buildFrame() never touches the device
// (only rasterFrame() does), matching the existing Renderer test pattern
// in test_renderer.cpp.
TEST(RenderObjectPaintPropagation, BuildFrameStillRendersWhenOnlyABoundaryDescendantIsDirty)
{
    auto leaf     = std::make_shared<LeafRenderBox>();
    auto boundary = std::make_shared<cw::RenderRepaintBoundary>();
    auto wrapper  = std::make_shared<WrapperRenderBox>();

    boundary->setChild(leaf);
    wrapper->setChild(boundary);

    cw::Renderer renderer(/*device=*/nullptr, wrapper, cw::Color::white());

    // First frame: everything starts dirty by construction.
    ASSERT_TRUE(renderer.buildFrame(200.0f, 200.0f).has_value());

    // Now dirty only the deep leaf, well below the (only) boundary.
    leaf->markNeedsPaint();
    ASSERT_FALSE(wrapper->needsPaint())
        << "sanity check: propagation really did stop below the wrapper";

    const auto pkg = renderer.buildFrame(200.0f, 200.0f);
    EXPECT_TRUE(pkg.has_value())
        << "a dirty leaf under a repaint boundary must still cause a frame "
           "to actually build, even though root's own needsPaint() is now "
           "false — buildFrame() must consume the paint-requested latch "
           "instead of checking root_->needsPaint()";
}

// Regression test for the real-world bug found in the gallery's Images tab:
// a SingleChildScrollView containing both a continuously-animating widget
// and an unrelated, far-away BackdropFilter only hit 45fps instead of 60,
// because OffsetLayer::maybeReplay() was passed `needsPaint() ||
// needsDescendantPaint()` as one undifferentiated `dirty` flag. Since the
// scroll view's own needsPaint() stayed false but needsDescendantPaint()
// was true every frame (the animating child, a separate nested boundary,
// needed a real repaint), the scroll view's *entire* viewport got reported
// as a dirty region every frame — which happened to always overlap the
// backdrop filter living in the same viewport, forcing its expensive
// capture-and-blur pass to re-run on every single frame even though the
// filter's own subtree never changed. See OffsetLayer::maybeReplay()'s doc
// comment for the own_dirty/descendant_dirty split that fixes this.
//
// Reproduced here with RenderRepaintBoundary (one of the seven classes that
// route through OffsetLayer::maybeReplay(), alongside the four scrollables,
// RenderClipRRect, and shadow-bearing RenderDecoratedBox) standing in for
// the scroll view, via the real Renderer::buildFrame() path so the
// resulting FramePackage::has_backdrop_filter is exactly what
// Renderer::rasterFrame() would use to decide whether to run the capture
// pass.
TEST(RenderObjectPaintPropagation, DistantDirtyDescendantDoesNotForceUnrelatedBackdropFilterCapture)
{
    auto animating_leaf     = std::make_shared<LeafRenderBox>();
    auto animating_boundary = std::make_shared<cw::RenderRepaintBoundary>();
    animating_boundary->setChild(animating_leaf);

    auto backdrop_leaf = std::make_shared<LeafRenderBox>();
    auto backdrop      = std::make_shared<cw::RenderBackdropFilter>();
    backdrop->setChild(backdrop_leaf);
    auto backdrop_boundary = std::make_shared<cw::RenderRepaintBoundary>();
    backdrop_boundary->setChild(backdrop);

    // Positioned 1000px apart — nowhere near each other, even accounting
    // for a generous blur margin.
    auto siblings = std::make_shared<TwoChildRenderBox>();
    siblings->setChildren(animating_boundary, cw::Offset{0.0f, 0.0f},
                          backdrop_boundary, cw::Offset{0.0f, 1000.0f});

    auto outer_scroll_stand_in = std::make_shared<cw::RenderRepaintBoundary>();
    outer_scroll_stand_in->setChild(siblings);

    cw::Renderer renderer(/*device=*/nullptr, outer_scroll_stand_in, cw::Color::white());

    // First frame: everything starts dirty by construction, so the
    // backdrop filter's own footprint is legitimately captured once.
    ASSERT_TRUE(renderer.buildFrame(2000.0f, 2000.0f).has_value());

    // Second "frame": dirty only the far-away animating leaf, exactly like
    // an ongoing AnimationController tick. The backdrop filter's own
    // subtree is completely untouched.
    animating_leaf->markNeedsPaint();
    ASSERT_FALSE(outer_scroll_stand_in->needsPaint())
        << "sanity check: the outer boundary's own flag must stay clean — "
           "only needsDescendantPaint() should be set";
    ASSERT_TRUE(outer_scroll_stand_in->needsDescendantPaint());

    const auto pkg = renderer.buildFrame(2000.0f, 2000.0f);
    ASSERT_TRUE(pkg.has_value());
    EXPECT_FALSE(pkg->has_backdrop_filter)
        << "a distant, unrelated dirty descendant must not force the "
           "backdrop-filter capture pass to re-run — see "
           "OffsetLayer::maybeReplay()'s own_dirty/descendant_dirty split";
}
