// The shared toolbar layout (buildToolbarWithStyle): the container's
// decoration (floating or docked), its padding, and the items, gaps and
// dividers in its row.

#include <gtest/gtest.h>

#include <campello_widgets/ui/toolbar_style.hpp>
#include <campello_widgets/widgets/decorated_box.hpp>
#include <campello_widgets/widgets/padding.hpp>
#include <campello_widgets/widgets/row.hpp>
#include <campello_widgets/widgets/column.hpp>
#include <campello_widgets/widgets/colored_box.hpp>
#include <campello_widgets/widgets/sized_box.hpp>

namespace cw = systems::leal::campello_widgets;

namespace
{
    cw::ToolbarStyle style()
    {
        cw::ToolbarStyle s;
        s.container        = cw::Color::fromRGB(0.9f, 0.9f, 0.9f);
        s.docked_container = cw::Color::fromRGB(0.8f, 0.8f, 0.8f);
        s.divider          = cw::Color::fromRGB(0.5f, 0.5f, 0.5f);
        s.radius           = 24.0f;
        s.shadow           = {cw::BoxShadow{cw::Color::black(), {0.0f, 2.0f}, 6.0f, 0.0f}};
        return s;
    }

    cw::ToolbarConfig twoGroups()
    {
        cw::ToolbarConfig cfg;
        cfg.groups = {{cw::ToolbarConfig::Item{nullptr, "Select", "", [] {}, true, true}},
                      {cw::ToolbarConfig::Item{nullptr, "Wall", "", [] {}, false, true},
                       cw::ToolbarConfig::Item{nullptr, "Door", "", [] {}, false, true}}};
        return cfg;
    }
}

TEST(ToolbarStyle, FloatingIsRoundedAndRaised)
{
    const auto widget = cw::buildToolbarWithStyle(twoGroups(), style(), true);
    const auto box = std::dynamic_pointer_cast<const cw::DecoratedBox>(widget);
    ASSERT_NE(box, nullptr);
    EXPECT_EQ(box->decoration.color, style().container);
    EXPECT_FLOAT_EQ(box->decoration.border_radius, 24.0f);
    EXPECT_EQ(box->decoration.box_shadow.size(), 1u);
    const auto padded = std::dynamic_pointer_cast<const cw::Padding>(box->child);
    ASSERT_NE(padded, nullptr);
    EXPECT_FLOAT_EQ(padded->padding.left, 8.0f);
}

TEST(ToolbarStyle, DockedIsFlat)
{
    auto cfg = twoGroups();
    cfg.floating = false;
    const auto box = std::dynamic_pointer_cast<const cw::DecoratedBox>(cw::buildToolbarWithStyle(cfg, style(), true));
    ASSERT_NE(box, nullptr);
    EXPECT_EQ(box->decoration.color, style().docked_container);
    EXPECT_FLOAT_EQ(box->decoration.border_radius, 0.0f);
    EXPECT_TRUE(box->decoration.box_shadow.empty());
}

TEST(ToolbarStyle, GroupsAreSetApartByGapsAndADivider)
{
    const auto box = std::dynamic_pointer_cast<const cw::DecoratedBox>(cw::buildToolbarWithStyle(twoGroups(), style(), true));
    const auto padded = std::dynamic_pointer_cast<const cw::Padding>(box->child);
    const auto row = std::dynamic_pointer_cast<const cw::Row>(padded->child);
    ASSERT_NE(row, nullptr);
    // Select | gap divider gap | Wall gap Door
    ASSERT_EQ(row->children.size(), 1u + 3u + 3u);
    const auto divider = std::dynamic_pointer_cast<const cw::SizedBox>(row->children[2]);
    ASSERT_NE(divider, nullptr);
    EXPECT_FLOAT_EQ(divider->width.value_or(0.0f), 1.0f);
    EXPECT_NE(std::dynamic_pointer_cast<const cw::ColoredBox>(divider->child), nullptr);
    // Gaps are fixed both ways (never stretched across the toolbar).
    const auto gap = std::dynamic_pointer_cast<const cw::SizedBox>(row->children[1]);
    ASSERT_NE(gap, nullptr);
    EXPECT_TRUE(gap->width.has_value() && gap->height.has_value());

    // Without a divider colour: one gap between groups.
    auto plain = style();
    plain.divider = cw::Color::transparent();
    const auto box2 = std::dynamic_pointer_cast<const cw::DecoratedBox>(cw::buildToolbarWithStyle(twoGroups(), plain, true));
    const auto row2 = std::dynamic_pointer_cast<const cw::Row>(std::dynamic_pointer_cast<const cw::Padding>(box2->child)->child);
    EXPECT_EQ(row2->children.size(), 1u + 1u + 3u);
}

TEST(ToolbarStyle, VerticalIsAColumn)
{
    auto cfg = twoGroups();
    cfg.direction = cw::Axis::vertical;
    const auto box = std::dynamic_pointer_cast<const cw::DecoratedBox>(cw::buildToolbarWithStyle(cfg, style(), true));
    const auto padded = std::dynamic_pointer_cast<const cw::Padding>(box->child);
    EXPECT_NE(std::dynamic_pointer_cast<const cw::Column>(padded->child), nullptr);
}
