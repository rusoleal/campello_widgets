#include <gtest/gtest.h>
#include <algorithm>
#include <campello_widgets/widgets/hero.hpp>
#include <campello_widgets/widgets/center.hpp>
#include <campello_widgets/widgets/padding.hpp>
#include <campello_widgets/widgets/row.hpp>
#include <campello_widgets/widgets/column.hpp>
#include <campello_widgets/widgets/container.hpp>
#include <campello_widgets/widgets/text.hpp>
#include <campello_widgets/widgets/sized_box.hpp>

namespace cw = systems::leal::campello_widgets;

namespace
{
    // Mounts `root_widget` as a real Element tree (mirrors
    // TabWidgetsTest::mountAndLayout's own root-mounting idiom in
    // test_tabs_and_animated_switcher.cpp, minus the render-layout half --
    // these tests only need the Element tree itself, no RenderObject layout).
    // Deliberately distinctly-named from other test files' own root-mounting
    // helpers to avoid a Unity Build redefinition collision.
    std::shared_ptr<cw::Element> mountHeroTestTree(cw::WidgetRef root_widget)
    {
        auto element = root_widget->createElement();
        element->mount(nullptr);
        return element;
    }
} // namespace

// ---------------------------------------------------------------------------
// 1. Element::visitAllDescendants() -- single-child chains
// ---------------------------------------------------------------------------

TEST(ElementVisitAllDescendants, SingleChildChainVisitsEveryDescendantOnce)
{
    // Padding(Center(Padding(SizedBox))) -- a chain of single-child widgets.
    // SizedBox (a plain SingleChildRenderObjectWidget, not a composite
    // StatelessWidget) is used as the leaf specifically so this test's
    // element count is exactly predictable -- Container/Text are themselves
    // StatelessWidgets that build into further internal elements, which
    // would make an exact count assertion here test their own unrelated
    // internal composition rather than visitAllDescendants() itself
    // (discovered by running this test: an earlier draft using Container/
    // Text produced more elements than expected for exactly this reason).
    auto leaf      = std::make_shared<cw::SizedBox>(10.0f, 10.0f);
    auto inner_pad = std::make_shared<cw::Padding>();
    inner_pad->child = leaf;
    auto center   = std::make_shared<cw::Center>(inner_pad);
    auto outer_pad = std::make_shared<cw::Padding>();
    outer_pad->child = center;

    auto root = mountHeroTestTree(outer_pad);

    std::vector<cw::Element*> visited;
    cw::Element::visitAllDescendants(root.get(), [&](cw::Element* e) { visited.push_back(e); });

    // root itself (outer_pad's element) must never appear in the list.
    for (cw::Element* e : visited)
        EXPECT_NE(e, root.get());

    // 3 descendants: center's element, inner_pad's element, leaf's element.
    EXPECT_EQ(visited.size(), 3u);

    // No duplicates.
    std::vector<cw::Element*> unique_visited = visited;
    std::sort(unique_visited.begin(), unique_visited.end());
    unique_visited.erase(std::unique(unique_visited.begin(), unique_visited.end()), unique_visited.end());
    EXPECT_EQ(unique_visited.size(), visited.size());

    root->unmount();
}

// ---------------------------------------------------------------------------
// 2. Element::visitAllDescendants() -- multi-child (real Row/Column tree)
// ---------------------------------------------------------------------------

TEST(ElementVisitAllDescendants, MultiChildTreeVisitsEveryBranch)
{
    // Column [ Row [ SizedBox, SizedBox ], SizedBox ] -- a real multi-child
    // tree crossing two different multi-child element types. SizedBox (not
    // Container -- see the single-child test's own comment on why) keeps
    // the element count exactly predictable, one element per leaf.
    auto row_child_a = std::make_shared<cw::SizedBox>(10.0f, 10.0f);
    auto row_child_b = std::make_shared<cw::SizedBox>(10.0f, 10.0f);
    auto row = std::make_shared<cw::Row>(cw::MainAxisAlignment::start, cw::CrossAxisAlignment::start,
        cw::WidgetList{ row_child_a, row_child_b });

    auto column_child_b = std::make_shared<cw::SizedBox>(10.0f, 10.0f);
    auto column = std::make_shared<cw::Column>(cw::MainAxisAlignment::start, cw::CrossAxisAlignment::start,
        cw::WidgetList{ row, column_child_b });

    auto root = mountHeroTestTree(column);

    int visit_count = 0;
    cw::Element::visitAllDescendants(root.get(), [&](cw::Element*) { ++visit_count; });

    // row's element + row_child_a's + row_child_b's + column_child_b's = 4
    // descendants beneath column's own element (root).
    EXPECT_EQ(visit_count, 4);

    root->unmount();
}

// ---------------------------------------------------------------------------
// 3-6. Hero::collectHeroesFor()
// ---------------------------------------------------------------------------

TEST(Hero, CollectHeroesForFindsHeroesAtDifferentNestingDepths)
{
    auto hero_a = std::make_shared<cw::Hero>();
    hero_a->tag   = "a";
    hero_a->child = std::make_shared<cw::Container>();

    auto hero_b = std::make_shared<cw::Hero>();
    hero_b->tag   = "b";
    hero_b->child = std::make_shared<cw::Container>();

    // hero_b sits inside a Row, nested two levels deeper than hero_a.
    auto row = std::make_shared<cw::Row>(cw::MainAxisAlignment::start, cw::CrossAxisAlignment::start,
        cw::WidgetList{ hero_b });
    auto padded_row = std::make_shared<cw::Padding>();
    padded_row->child = row;

    auto column = std::make_shared<cw::Column>(cw::MainAxisAlignment::start, cw::CrossAxisAlignment::start,
        cw::WidgetList{ hero_a, padded_row });

    auto root = mountHeroTestTree(column);

    auto heroes = cw::Hero::collectHeroesFor(root.get());

    ASSERT_EQ(heroes.size(), 2u);
    ASSERT_TRUE(heroes.count("a"));
    ASSERT_TRUE(heroes.count("b"));
    EXPECT_TRUE(dynamic_cast<const cw::Hero*>(&heroes["a"]->widget()));
    EXPECT_EQ(static_cast<const cw::Hero&>(heroes["a"]->widget()).tag, "a");
    EXPECT_EQ(static_cast<const cw::Hero&>(heroes["b"]->widget()).tag, "b");

    root->unmount();
}

TEST(Hero, CollectHeroesForReturnsEmptyMapWhenNoHeroesPresent)
{
    auto column = std::make_shared<cw::Column>(cw::MainAxisAlignment::start, cw::CrossAxisAlignment::start,
        cw::WidgetList{ std::make_shared<cw::Container>(), std::make_shared<cw::Text>("plain") });

    auto root = mountHeroTestTree(column);

    auto heroes = cw::Hero::collectHeroesFor(root.get());
    EXPECT_TRUE(heroes.empty());

    root->unmount();
}

TEST(Hero, CollectHeroesForWithDuplicateTagDoesNotCrashAndProducesOneEntry)
{
    // Two Heroes sharing the tag "dup" in one subtree -- unspecified which
    // one wins (last-visited, per Element::visitChildren()'s own traversal
    // order), but must not crash and must produce exactly one map entry.
    auto hero_1 = std::make_shared<cw::Hero>();
    hero_1->tag   = "dup";
    hero_1->child = std::make_shared<cw::Container>();

    auto hero_2 = std::make_shared<cw::Hero>();
    hero_2->tag   = "dup";
    hero_2->child = std::make_shared<cw::Container>();

    auto column = std::make_shared<cw::Column>(cw::MainAxisAlignment::start, cw::CrossAxisAlignment::start,
        cw::WidgetList{ hero_1, hero_2 });

    auto root = mountHeroTestTree(column);

    auto heroes = cw::Hero::collectHeroesFor(root.get());

    // Observed: exactly one entry survives (the later sibling, matching
    // Column's own child registration/visitChildren order) -- documented
    // here rather than silently assumed.
    ASSERT_EQ(heroes.size(), 1u);
    ASSERT_TRUE(heroes.count("dup"));

    root->unmount();
}

TEST(Hero, BuildIsAPurePassthroughToChild)
{
    auto child = std::make_shared<cw::Container>();
    cw::Hero hero;
    hero.tag   = "x";
    hero.child = child;

    // Hero::build() doesn't touch the BuildContext it receives; a
    // default-constructed dummy is fine as long as nothing dereferences it.
    class NullContext : public cw::BuildContext
    {
    public:
        const cw::Widget& widget() const override { throw std::logic_error("unused"); }
    protected:
        const cw::InheritedWidget* getInheritedWidget(const std::type_info&) override { return nullptr; }
    } ctx;

    cw::WidgetRef built = hero.build(ctx);
    EXPECT_EQ(built.get(), child.get());
}
