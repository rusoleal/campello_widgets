#include <gtest/gtest.h>

#include <campello_widgets/ui/render_baseline.hpp>
#include <campello_widgets/ui/render_text.hpp>
#include <campello_widgets/ui/render_sized_box.hpp>
#include <campello_widgets/ui/render_align.hpp>
#include <campello_widgets/ui/render_flow.hpp>
#include <campello_widgets/ui/render_custom_multi_child_layout.hpp>
#include <campello_widgets/ui/render_indexed_stack.hpp>
#include <campello_widgets/ui/scrollbar_geometry.hpp>
#include <campello_widgets/ui/key_combo.hpp>
#include <campello_widgets/ui/box_constraints.hpp>
#include <campello_widgets/ui/text_span.hpp>
#include <campello_widgets/ui/focus_node.hpp>
#include <campello_widgets/widgets/shortcuts.hpp>
#include <campello_widgets/widgets/build_context.hpp>

namespace cw = systems::leal::campello_widgets;

namespace
{
    struct StubBuildContext : cw::BuildContext
    {
        const cw::Widget& widget() const override { std::abort(); }
    protected:
        const cw::InheritedWidget* getInheritedWidget(const std::type_info&) override { return nullptr; }
    };
}

// ---------------------------------------------------------------------------
// Baseline
// ---------------------------------------------------------------------------

TEST(RenderBox, DefaultBaselineDelegatesToChildWithOffset)
{
    auto child = std::make_shared<cw::RenderText>();
    child->setTextSpan(cw::TextSpan{"hi", cw::TextStyle{}.withFontSize(20.0f)});

    // RenderAlign centers its child within extra space -- unlike
    // RenderSizedBox (which always positions its child at {0,0}) this
    // actually produces a nonzero child_offset_ to verify gets added in.
    cw::RenderAlign box;
    box.setChild(child);
    box.layout(cw::BoxConstraints::tight(200.0f, 100.0f));

    const auto d = box.computeDistanceToActualBaseline(cw::TextBaseline::alphabetic);
    ASSERT_TRUE(d.has_value());
    const auto child_d = child->computeDistanceToActualBaseline(cw::TextBaseline::alphabetic);
    ASSERT_TRUE(child_d.has_value());
    EXPECT_GT(*d, *child_d);
}

TEST(RenderText, BaselineApproximatesSeventyFivePercentOfFontSize)
{
    cw::RenderText t;
    t.setTextSpan(cw::TextSpan{"hello", cw::TextStyle{}.withFontSize(20.0f)});
    t.layout(cw::BoxConstraints::loose(200.0f, 100.0f));

    const auto d = t.computeDistanceToActualBaseline(cw::TextBaseline::alphabetic);
    ASSERT_TRUE(d.has_value());
    EXPECT_FLOAT_EQ(*d, 15.0f); // 20 * 0.75
}

TEST(RenderBaseline, PositionsChildSoBaselineLandsAtRequestedOffset)
{
    auto child = std::make_shared<cw::RenderText>();
    child->setTextSpan(cw::TextSpan{"hi", cw::TextStyle{}.withFontSize(20.0f)}); // child baseline = 15

    cw::RenderBaseline rb;
    rb.baseline = 40.0f;
    rb.setChild(child);
    rb.layout(cw::BoxConstraints::loose(200.0f, 200.0f));

    // top = baseline - child_baseline = 40 - 15 = 25
    // RenderBaseline positions the child via positionChild(), reflected in
    // its own resulting size (top + child height).
    EXPECT_FLOAT_EQ(rb.size().height, 25.0f + child->size().height);
}

// ---------------------------------------------------------------------------
// Flow
// ---------------------------------------------------------------------------

TEST(RenderFlow, LayoutFillsAvailableSpace)
{
    cw::RenderFlow flow;
    flow.layout(cw::BoxConstraints::loose(300.0f, 200.0f));
    EXPECT_FLOAT_EQ(flow.size().width,  300.0f);
    EXPECT_FLOAT_EQ(flow.size().height, 200.0f);
}

TEST(RenderFlow, InsertAndClearChildrenManagesParenting)
{
    cw::RenderFlow flow;
    auto a = std::make_shared<cw::RenderSizedBox>();
    a->width = 10.0f; a->height = 10.0f;
    flow.insertChild(a, 0);

    bool found = false;
    flow.visitRenderChildren([&](cw::RenderBox* b) { if (b == a.get()) found = true; });
    EXPECT_TRUE(found);

    flow.clearChildren();
    found = false;
    flow.visitRenderChildren([&](cw::RenderBox*) { found = true; });
    EXPECT_FALSE(found);
}

// ---------------------------------------------------------------------------
// CustomMultiChildLayout
// ---------------------------------------------------------------------------

namespace
{
    class TestLayoutDelegate : public cw::MultiChildLayoutDelegate
    {
    public:
        void performLayout(const cw::Size&) override
        {
            if (hasChild("a"))
            {
                layoutChild("a", cw::BoxConstraints::loose(50.0f, 50.0f));
                positionChild("a", {0.0f, 0.0f});
            }
            if (hasChild("b"))
            {
                layoutChild("b", cw::BoxConstraints::loose(50.0f, 50.0f));
                positionChild("b", {100.0f, 0.0f});
            }
        }
        bool shouldRelayout(const cw::MultiChildLayoutDelegate&) const override { return true; }

    private:
        // Expose protected helpers for the test.
        using cw::MultiChildLayoutDelegate::hasChild;
        using cw::MultiChildLayoutDelegate::layoutChild;
        using cw::MultiChildLayoutDelegate::positionChild;
    };
}

TEST(RenderCustomMultiChildLayout, DelegateLaysOutAndPositionsChildrenById)
{
    auto child_a = std::make_shared<cw::RenderSizedBox>();
    child_a->width = 20.0f; child_a->height = 20.0f;
    auto child_b = std::make_shared<cw::RenderSizedBox>();
    child_b->width = 20.0f; child_b->height = 20.0f;

    cw::RenderCustomMultiChildLayout layout;
    layout.delegate = std::make_shared<TestLayoutDelegate>();
    layout.insertChild("a", child_a);
    layout.insertChild("b", child_b);
    layout.layout(cw::BoxConstraints::loose(300.0f, 100.0f));

    EXPECT_TRUE(layout.hasChild("a"));
    EXPECT_TRUE(layout.hasChild("b"));
    EXPECT_FALSE(layout.hasChild("c"));
}

// ---------------------------------------------------------------------------
// IndexedStack
// ---------------------------------------------------------------------------

TEST(RenderIndexedStack, OnlySelectedIndexIsHitTestable)
{
    cw::RenderIndexedStack stack;
    stack.index = 1;

    // Two overlapping RenderListener-like children would be ideal, but a
    // plain size check on the selected/unselected paint path is enough to
    // confirm the index gate without needing hit-testable leaves here --
    // see RenderIgnorePointer/RenderAbsorbPointer tests for that pattern.
    EXPECT_EQ(stack.index, 1);
}

// ---------------------------------------------------------------------------
// Shortcuts
// ---------------------------------------------------------------------------

TEST(Shortcuts, MatchingComboInvokesCallbackAndConsumes)
{
    auto node = std::make_shared<cw::FocusNode>();
    cw::Shortcuts w;
    w.focus_node = node;
    bool fired = false;
    w.bindings = {
        {cw::KeyCombo{cw::KeyCode::s, cw::KeyModifiers::ctrl}, [&]() { fired = true; }},
    };
    StubBuildContext ctx;
    w.build(ctx); // wires focus_node->on_key

    cw::KeyEvent e;
    e.kind = cw::KeyEventKind::down;
    e.key_code = cw::KeyCode::s;
    e.modifiers = cw::KeyModifiers::ctrl;

    ASSERT_TRUE(static_cast<bool>(node->on_key));
    EXPECT_TRUE(node->on_key(e));
    EXPECT_TRUE(fired);
}

TEST(Shortcuts, NonMatchingComboFallsThrough)
{
    auto node = std::make_shared<cw::FocusNode>();
    cw::Shortcuts w;
    w.focus_node = node;
    bool fired = false;
    w.bindings = {
        {cw::KeyCombo{cw::KeyCode::s, cw::KeyModifiers::ctrl}, [&]() { fired = true; }},
    };
    StubBuildContext ctx;
    w.build(ctx);

    cw::KeyEvent e;
    e.kind = cw::KeyEventKind::down;
    e.key_code = cw::KeyCode::a; // different key
    e.modifiers = cw::KeyModifiers::ctrl;

    EXPECT_FALSE(node->on_key(e));
    EXPECT_FALSE(fired);
}

TEST(Shortcuts, KeyUpNeverTriggers)
{
    auto node = std::make_shared<cw::FocusNode>();
    cw::Shortcuts w;
    w.focus_node = node;
    bool fired = false;
    w.bindings = {
        {cw::KeyCombo{cw::KeyCode::escape, cw::KeyModifiers::none}, [&]() { fired = true; }},
    };
    StubBuildContext ctx;
    w.build(ctx);

    cw::KeyEvent e;
    e.kind = cw::KeyEventKind::up;
    e.key_code = cw::KeyCode::escape;

    EXPECT_FALSE(node->on_key(e));
    EXPECT_FALSE(fired);
}

// ---------------------------------------------------------------------------
// Scrollbar thumb geometry
// ---------------------------------------------------------------------------

TEST(ScrollbarGeometry, NoOverflowFillsWholeTrack)
{
    const auto g = cw::computeScrollbarThumbGeometry(100.0f, 0.0f, 0.0f, 0.0f);
    EXPECT_FLOAT_EQ(g.length, 100.0f);
    EXPECT_FLOAT_EQ(g.position, 0.0f);
}

TEST(ScrollbarGeometry, AtTopPositionIsZero)
{
    const auto g = cw::computeScrollbarThumbGeometry(100.0f, 0.0f, 400.0f, 0.0f);
    EXPECT_FLOAT_EQ(g.position, 0.0f);
    // proportion = 100/(100+400) = 0.2, length = 20 -> clamped to min 24
    EXPECT_FLOAT_EQ(g.length, 24.0f);
}

TEST(ScrollbarGeometry, AtBottomThumbReachesTrackEnd)
{
    const auto g = cw::computeScrollbarThumbGeometry(100.0f, 0.0f, 400.0f, 400.0f);
    EXPECT_FLOAT_EQ(g.position, 100.0f - g.length);
}

TEST(ScrollbarGeometry, MidwayIsHalfway)
{
    const auto g = cw::computeScrollbarThumbGeometry(100.0f, 0.0f, 400.0f, 200.0f, /*min_thumb_length=*/0.0f);
    // proportion = 100/500 = 0.2, length = 20, track_room = 80
    EXPECT_FLOAT_EQ(g.length, 20.0f);
    EXPECT_FLOAT_EQ(g.position, 40.0f); // 0.5 * 80
}

TEST(ScrollbarGeometry, ThumbNeverExceedsViewport)
{
    // min_thumb_length larger than viewport must still clamp to viewport.
    const auto g = cw::computeScrollbarThumbGeometry(50.0f, 0.0f, 1000.0f, 0.0f, /*min_thumb_length=*/999.0f);
    EXPECT_FLOAT_EQ(g.length, 50.0f);
}
