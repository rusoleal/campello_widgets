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

TEST(ToolbarStyle, FloatingIsRoundedAndFlatUnlessElevated)
{
    auto cfg = twoGroups();
    const auto widget = cw::buildToolbarWithStyle(cfg, style(), true);
    const auto box = std::dynamic_pointer_cast<const cw::DecoratedBox>(widget);
    ASSERT_NE(box, nullptr);
    EXPECT_EQ(box->decoration.color, style().container);
    EXPECT_FLOAT_EQ(box->decoration.border_radius, 24.0f);
    EXPECT_TRUE(box->decoration.box_shadow.empty()) << "MD3 floating toolbars: elevation level 0";
    cfg.elevated = true;
    const auto raised = std::dynamic_pointer_cast<const cw::DecoratedBox>(cw::buildToolbarWithStyle(cfg, style(), true));
    EXPECT_EQ(raised->decoration.box_shadow.size(), 1u);
    const auto padded = std::dynamic_pointer_cast<const cw::Padding>(box->child);
    ASSERT_NE(padded, nullptr);
    EXPECT_FLOAT_EQ(padded->padding.left, 8.0f);
}

TEST(ToolbarStyle, DockedIsFlat)
{
    auto cfg = twoGroups();
    cfg.floating = false;
    cfg.elevated = true;  // ignored when docked
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

// ---------------------------------------------------------------------------
// Keyboard (WAI-ARIA toolbar pattern): one Tab stop at the selected item;
// Right/Left move between items skipping disabled ones and wrapping;
// Home/End; Space activates the focused item.
// ---------------------------------------------------------------------------

#include <campello_widgets/ui/focus_manager.hpp>
#include <campello_widgets/ui/key_event.hpp>
#include <campello_widgets/ui/render_box.hpp>
#include <campello_widgets/ui/box_constraints.hpp>
#include <campello_widgets/ui/ticker.hpp>
#include <campello_widgets/ui/pointer_dispatcher.hpp>
#include <campello_widgets/widgets/element.hpp>

namespace
{
    cw::KeyEvent key(cw::KeyCode code, cw::KeyEventKind kind = cw::KeyEventKind::down)
    {
        cw::KeyEvent e;
        e.kind     = kind;
        e.key_code = code;
        return e;
    }
}

TEST(ToolbarStyle, KeyboardRovesFocusAmongItems)
{
    cw::FocusManager manager;
    cw::FocusManager* previous = cw::FocusManager::activeManager();
    cw::FocusManager::setActiveManager(&manager);
    cw::TickerScheduler ticker;
    cw::TickerScheduler::setActive(&ticker);
    cw::PointerDispatcher dispatcher;
    cw::PointerDispatcher::setActiveDispatcher(&dispatcher);

    // A | B C(disabled) | D, B selected: Tab enters at B.
    std::vector<int> pressed;
    const auto item = [&](int id, bool selected, bool enabled) {
        return cw::ToolbarConfig::Item{nullptr, std::string(1, static_cast<char>('A' + id)), "", [&pressed, id] { pressed.push_back(id); },
                                       selected, enabled};
    };
    cw::ToolbarConfig cfg;
    cfg.groups = {{item(0, false, true)}, {item(1, true, true), item(2, false, false)}, {item(3, false, true)}};
    {
        auto keyed = style();
        keyed.focus_ring = cw::Color::black();
        auto root = cw::buildToolbarWithStyle(cfg, keyed, true)->createElement();
        root->mount(nullptr);
        auto* roe = root->findDescendantRenderObjectElement();
        ASSERT_NE(roe, nullptr);
        auto box = std::dynamic_pointer_cast<cw::RenderBox>(roe->sharedRenderObject());
        ASSERT_NE(box, nullptr);
        box->layout(cw::BoxConstraints::loose(800.0f, 200.0f));

        const auto press = [&](cw::KeyCode code) {
            manager.handleKeyEvent(key(code));
            manager.handleKeyEvent(key(code, cw::KeyEventKind::up));
        };
        const auto activate = [&] {
            press(cw::KeyCode::space);
            return pressed.empty() ? -1 : pressed.back();
        };

        // A focus ring is drawn for keyboard focus only: count the bordered
        // DecoratedBoxes (rings) in the tree after rebuilding.
        const auto rings = [&]() {
            cw::Element::buildScope();
            int count = 0;
            std::function<void(cw::Element*)> walk = [&](cw::Element* e) {
                if (auto* d = dynamic_cast<const cw::DecoratedBox*>(&e->widget()); d && d->decoration.border) ++count;
                e->visitChildren(walk);
            };
            walk(root.get());
            return count;
        };
        EXPECT_EQ(rings(), 0);

        press(cw::KeyCode::tab);
        EXPECT_EQ(rings(), 1) << "keyboard focus: one ring";
        EXPECT_EQ(activate(), 1) << "Tab enters at the selected item";
        press(cw::KeyCode::right);
        EXPECT_EQ(activate(), 3) << "the disabled item is skipped";
        press(cw::KeyCode::right);
        EXPECT_EQ(activate(), 0) << "wraps to the first";
        press(cw::KeyCode::left);
        EXPECT_EQ(activate(), 3) << "and back";
        press(cw::KeyCode::home);
        EXPECT_EQ(activate(), 0);
        press(cw::KeyCode::end);
        EXPECT_EQ(activate(), 3);
        // One Tab stop: Tab leaves the toolbar (here, nothing else: back to it).
        press(cw::KeyCode::tab);
        EXPECT_EQ(activate(), 1) << "the only Tab stop is the selected item";
        root->unmount();
    }

    cw::PointerDispatcher::setActiveDispatcher(nullptr);
    cw::TickerScheduler::setActive(nullptr);
    cw::FocusManager::setActiveManager(previous);
}
