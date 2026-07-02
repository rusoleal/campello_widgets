#include <gtest/gtest.h>
#include <campello_widgets/ui/render_repaint_boundary.hpp>
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
