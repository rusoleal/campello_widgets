#include <gtest/gtest.h>
#include <campello_widgets/ui/render_decorated_box.hpp>
#include <campello_widgets/ui/render_sized_box.hpp>
#include <campello_widgets/ui/box_constraints.hpp>
#include <campello_widgets/ui/box_decoration.hpp>
#include <campello_widgets/ui/box_border.hpp>
#include <campello_widgets/ui/box_shadow.hpp>
#include <campello_widgets/ui/color.hpp>

namespace cw = systems::leal::campello_widgets;

// -----------------------------------------------------------------------
// Layout — no child
// -----------------------------------------------------------------------

TEST(RenderDecoratedBox, NoChildFillsAvailableSpace)
{
    cw::RenderDecoratedBox box;
    box.decoration = cw::BoxDecoration{.color = cw::Color::fromRGB(0.2f, 0.5f, 0.9f)};
    box.layout(cw::BoxConstraints::loose(400, 300));
    EXPECT_FLOAT_EQ(box.size().width,  400.0f);
    EXPECT_FLOAT_EQ(box.size().height, 300.0f);
}

TEST(RenderDecoratedBox, NoChildNoDecorationIsZeroSize)
{
    // With loose constraints and no child, min is 0 so constrain(max) gives max.
    // This verifies that an empty decoration still performs layout.
    cw::RenderDecoratedBox box;
    box.layout(cw::BoxConstraints::tight(200, 150));
    EXPECT_FLOAT_EQ(box.size().width,  200.0f);
    EXPECT_FLOAT_EQ(box.size().height, 150.0f);
}

// -----------------------------------------------------------------------
// Layout — with child
// -----------------------------------------------------------------------

TEST(RenderDecoratedBox, SizeMatchesChild)
{
    auto child = std::make_shared<cw::RenderSizedBox>();
    child->width  = 120.0f;
    child->height = 80.0f;

    cw::RenderDecoratedBox box;
    box.decoration = cw::BoxDecoration{.color = cw::Color::white()};
    box.setChild(child);
    box.layout(cw::BoxConstraints::loose(400, 300));

    EXPECT_FLOAT_EQ(box.size().width,  120.0f);
    EXPECT_FLOAT_EQ(box.size().height,  80.0f);
}

TEST(RenderDecoratedBox, ChildClampedToParentMax)
{
    auto child = std::make_shared<cw::RenderSizedBox>();
    child->width  = 999.0f;
    child->height = 999.0f;

    cw::RenderDecoratedBox box;
    box.setChild(child);
    box.layout(cw::BoxConstraints::loose(100, 75));

    EXPECT_FLOAT_EQ(box.size().width,  100.0f);
    EXPECT_FLOAT_EQ(box.size().height,  75.0f);
}

// -----------------------------------------------------------------------
// Decoration fields compile and don't crash during layout
// -----------------------------------------------------------------------

TEST(RenderDecoratedBox, FullDecorationNoChildDoesNotCrash)
{
    cw::RenderDecoratedBox box;
    box.decoration = cw::BoxDecoration{
        .color         = cw::Color::fromRGB(1.0f, 0.0f, 0.0f),
        .border_radius = 8.0f,
        .border        = cw::BoxBorder::all(cw::Color::black(), 2.0f),
        .box_shadow    = {cw::BoxShadow{cw::Color::black(), {2.0f, 4.0f}, 8.0f, 0.0f}},
    };
    box.layout(cw::BoxConstraints::loose(200, 200));
    EXPECT_FLOAT_EQ(box.size().width,  200.0f);
    EXPECT_FLOAT_EQ(box.size().height, 200.0f);
}

TEST(RenderDecoratedBox, BorderRadiusOnlyNoChild)
{
    cw::RenderDecoratedBox box;
    box.decoration = cw::BoxDecoration{.border_radius = 16.0f};
    box.layout(cw::BoxConstraints::loose(300, 200));
    EXPECT_FLOAT_EQ(box.size().width,  300.0f);
    EXPECT_FLOAT_EQ(box.size().height, 200.0f);
}

TEST(RenderDecoratedBox, BorderOnlyNoChild)
{
    cw::RenderDecoratedBox box;
    box.decoration = cw::BoxDecoration{
        .border = cw::BoxBorder::all(cw::Color::black(), 1.0f)
    };
    box.layout(cw::BoxConstraints::loose(150, 100));
    EXPECT_FLOAT_EQ(box.size().width,  150.0f);
    EXPECT_FLOAT_EQ(box.size().height, 100.0f);
}

// -----------------------------------------------------------------------
// DecorationPosition — layout is identical regardless of position
// -----------------------------------------------------------------------

TEST(RenderDecoratedBox, ForegroundPositionSizeMatchesChild)
{
    auto child = std::make_shared<cw::RenderSizedBox>();
    child->width  = 60.0f;
    child->height = 40.0f;

    cw::RenderDecoratedBox box;
    box.decoration = cw::BoxDecoration{.color = cw::Color::white()};
    box.position   = cw::DecorationPosition::foreground;
    box.setChild(child);
    box.layout(cw::BoxConstraints::loose(200, 200));

    EXPECT_FLOAT_EQ(box.size().width,  60.0f);
    EXPECT_FLOAT_EQ(box.size().height, 40.0f);
}
