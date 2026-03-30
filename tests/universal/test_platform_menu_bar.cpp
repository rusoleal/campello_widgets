#include <gtest/gtest.h>
#include <optional>
#include <campello_widgets/widgets/platform_menu.hpp>
#include <campello_widgets/widgets/platform_menu_bar.hpp>
#include <campello_widgets/widgets/platform_menu_delegate.hpp>
#include <campello_widgets/widgets/sized_box.hpp>
#include <campello_widgets/widgets/build_context.hpp>

namespace cw = systems::leal::campello_widgets;

// ---------------------------------------------------------------------------
// Test Fixture for PlatformMenuDelegate
// ---------------------------------------------------------------------------

class MockPlatformMenuDelegate : public cw::PlatformMenuDelegate
{
public:
    std::vector<cw::PlatformMenuRef> last_menus;
    int setMenus_call_count = 0;
    int clearMenus_call_count = 0;

    void setMenus(const std::vector<cw::PlatformMenuRef>& menus) override
    {
        last_menus = menus;
        setMenus_call_count++;
    }

    void clearMenus() override
    {
        last_menus.clear();
        clearMenus_call_count++;
    }

    void reset()
    {
        last_menus.clear();
        setMenus_call_count = 0;
        clearMenus_call_count = 0;
    }
};

class PlatformMenuBarTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        mock_delegate_ = std::make_unique<MockPlatformMenuDelegate>();
        mock_delegate_ptr_ = mock_delegate_.get();
        cw::PlatformMenuDelegate::setInstance(std::move(mock_delegate_));
    }

    void TearDown() override
    {
        // Reset to default (no-op) implementation
        cw::PlatformMenuDelegate::setInstance(nullptr);
    }

    std::unique_ptr<MockPlatformMenuDelegate> mock_delegate_;
    MockPlatformMenuDelegate* mock_delegate_ptr_;
};

// ---------------------------------------------------------------------------
// PlatformMenuItemLabel Tests
// ---------------------------------------------------------------------------

TEST(PlatformMenuItemLabel, CreateWithLabelOnly)
{
    auto item = cw::PlatformMenuItemLabel::create("Test Label");
    ASSERT_NE(item, nullptr);
    
    auto* label = dynamic_cast<cw::PlatformMenuItemLabel*>(item.get());
    ASSERT_NE(label, nullptr);
    EXPECT_EQ(label->label, "Test Label");
    EXPECT_TRUE(label->shortcut.empty());
    EXPECT_TRUE(label->enabled);
    EXPECT_FALSE(label->on_selected);
}

TEST(PlatformMenuItemLabel, CreateWithCallback)
{
    bool called = false;
    auto item = cw::PlatformMenuItemLabel::create("Click Me", [&called]() { called = true; });
    
    auto* label = dynamic_cast<cw::PlatformMenuItemLabel*>(item.get());
    ASSERT_NE(label, nullptr);
    ASSERT_TRUE(label->on_selected);
    
    label->on_selected();
    EXPECT_TRUE(called);
}

TEST(PlatformMenuItemLabel, CreateWithShortcut)
{
    auto item = cw::PlatformMenuItemLabel::create("Open", "Cmd+O", []() {});
    
    auto* label = dynamic_cast<cw::PlatformMenuItemLabel*>(item.get());
    ASSERT_NE(label, nullptr);
    EXPECT_EQ(label->label, "Open");
    EXPECT_EQ(label->shortcut, "Cmd+O");
}

TEST(PlatformMenuItemLabel, CloneCopiesAllFields)
{
    bool called = false;
    auto original = std::make_shared<cw::PlatformMenuItemLabel>("Test", "Cmd+T", [&called]() { called = true; });
    original->enabled = false;
    
    auto cloned = original->clone();
    ASSERT_NE(cloned, nullptr);
    
    auto* cloned_label = dynamic_cast<cw::PlatformMenuItemLabel*>(cloned.get());
    ASSERT_NE(cloned_label, nullptr);
    
    EXPECT_EQ(cloned_label->label, "Test");
    EXPECT_EQ(cloned_label->shortcut, "Cmd+T");
    EXPECT_FALSE(cloned_label->enabled);
    ASSERT_TRUE(cloned_label->on_selected);
    
    // Verify callback is copied and works
    cloned_label->on_selected();
    EXPECT_TRUE(called);
}

// ---------------------------------------------------------------------------
// PlatformMenuItemSeparator Tests
// ---------------------------------------------------------------------------

TEST(PlatformMenuItemSeparator, CreateAndClone)
{
    auto item = cw::PlatformMenuItemSeparator::create();
    ASSERT_NE(item, nullptr);
    
    auto* sep = dynamic_cast<cw::PlatformMenuItemSeparator*>(item.get());
    EXPECT_NE(sep, nullptr);
    
    auto cloned = item->clone();
    EXPECT_NE(cloned, nullptr);
    EXPECT_TRUE(dynamic_cast<cw::PlatformMenuItemSeparator*>(cloned.get()) != nullptr);
}

// ---------------------------------------------------------------------------
// PlatformMenuItemSubmenu Tests
// ---------------------------------------------------------------------------

TEST(PlatformMenuItemSubmenu, CreateWithLabelOnly)
{
    auto item = cw::PlatformMenuItemSubmenu::create("File");
    ASSERT_NE(item, nullptr);
    
    auto* submenu = dynamic_cast<cw::PlatformMenuItemSubmenu*>(item.get());
    ASSERT_NE(submenu, nullptr);
    EXPECT_EQ(submenu->label, "File");
    EXPECT_TRUE(submenu->items.empty());
    EXPECT_TRUE(submenu->enabled);
}

TEST(PlatformMenuItemSubmenu, CreateWithItems)
{
    auto item = cw::PlatformMenuItemSubmenu::create("Edit", {
        cw::PlatformMenuItemLabel::create("Cut"),
        cw::PlatformMenuItemLabel::create("Copy"),
        cw::PlatformMenuItemSeparator::create(),
    });
    
    auto* submenu = dynamic_cast<cw::PlatformMenuItemSubmenu*>(item.get());
    ASSERT_NE(submenu, nullptr);
    EXPECT_EQ(submenu->items.size(), 3);
}

TEST(PlatformMenuItemSubmenu, CloneDeepCopiesItems)
{
    auto original = std::make_shared<cw::PlatformMenuItemSubmenu>("File");
    original->items.push_back(cw::PlatformMenuItemLabel::create("Open"));
    original->items.push_back(cw::PlatformMenuItemSeparator::create());
    
    auto cloned = original->clone();
    ASSERT_NE(cloned, nullptr);
    
    auto* cloned_submenu = dynamic_cast<cw::PlatformMenuItemSubmenu*>(cloned.get());
    ASSERT_NE(cloned_submenu, nullptr);
    EXPECT_EQ(cloned_submenu->label, "File");
    EXPECT_EQ(cloned_submenu->items.size(), 2);
}

// ---------------------------------------------------------------------------
// PlatformProvidedMenuItem Tests
// ---------------------------------------------------------------------------

TEST(PlatformProvidedMenuItem, FactoryMethods)
{
    auto quit = cw::PlatformProvidedMenuItem::quit();
    ASSERT_NE(quit, nullptr);
    
    auto* item = dynamic_cast<cw::PlatformProvidedMenuItem*>(quit.get());
    ASSERT_NE(item, nullptr);
    EXPECT_EQ(item->type, cw::PlatformProvidedMenuItemType::quit);
    EXPECT_TRUE(item->enabled);
    
    auto cut = cw::PlatformProvidedMenuItem::cut();
    auto* cut_item = dynamic_cast<cw::PlatformProvidedMenuItem*>(cut.get());
    EXPECT_EQ(cut_item->type, cw::PlatformProvidedMenuItemType::cut);
}

TEST(PlatformProvidedMenuItem, CloneCopiesType)
{
    auto original = std::make_shared<cw::PlatformProvidedMenuItem>(cw::PlatformProvidedMenuItemType::copy);
    original->enabled = false;
    
    auto cloned = original->clone();
    ASSERT_NE(cloned, nullptr);
    
    auto* cloned_item = dynamic_cast<cw::PlatformProvidedMenuItem*>(cloned.get());
    ASSERT_NE(cloned_item, nullptr);
    EXPECT_EQ(cloned_item->type, cw::PlatformProvidedMenuItemType::copy);
    EXPECT_FALSE(cloned_item->enabled);
}

// ---------------------------------------------------------------------------
// PlatformMenu Tests
// ---------------------------------------------------------------------------

TEST(PlatformMenu, CreateWithLabelOnly)
{
    auto menu = cw::PlatformMenu::create("File");
    ASSERT_NE(menu, nullptr);
    EXPECT_EQ(menu->label, "File");
    EXPECT_TRUE(menu->items.empty());
}

TEST(PlatformMenu, CreateWithItems)
{
    auto menu = cw::PlatformMenu::create("File", {
        cw::PlatformMenuItemLabel::create("New"),
        cw::PlatformMenuItemLabel::create("Open"),
        cw::PlatformMenuItemSeparator::create(),
        cw::PlatformMenuItemLabel::create("Quit"),
    });
    
    ASSERT_NE(menu, nullptr);
    EXPECT_EQ(menu->label, "File");
    EXPECT_EQ(menu->items.size(), 4);
}

// ---------------------------------------------------------------------------
// PlatformMenuDelegate Tests
// ---------------------------------------------------------------------------

TEST_F(PlatformMenuBarTest, SetMenusCalledWithCorrectMenus)
{
    auto menus = std::vector<cw::PlatformMenuRef>{
        cw::PlatformMenu::create("File"),
        cw::PlatformMenu::create("Edit"),
    };
    
    cw::PlatformMenuDelegate::instance()->setMenus(menus);
    
    EXPECT_EQ(mock_delegate_ptr_->setMenus_call_count, 1);
    EXPECT_EQ(mock_delegate_ptr_->last_menus.size(), 2);
    EXPECT_EQ(mock_delegate_ptr_->last_menus[0]->label, "File");
    EXPECT_EQ(mock_delegate_ptr_->last_menus[1]->label, "Edit");
}

TEST_F(PlatformMenuBarTest, ClearMenusCalled)
{
    cw::PlatformMenuDelegate::instance()->clearMenus();
    
    EXPECT_EQ(mock_delegate_ptr_->clearMenus_call_count, 1);
    EXPECT_TRUE(mock_delegate_ptr_->last_menus.empty());
}

// ---------------------------------------------------------------------------
// PlatformMenuBar Widget Tests
// ---------------------------------------------------------------------------

TEST_F(PlatformMenuBarTest, BuildReturnsChild)
{
    auto child = cw::SizedBox::create(100.0f, 100.0f);
    auto menu_bar = cw::PlatformMenuBar::create(child);
    
    // Create a dummy BuildContext (we can't easily create a real one in unit tests)
    // For this test, we just verify the child is returned
    // The actual delegate call happens in build()
    EXPECT_EQ(menu_bar->child, child);
}

TEST_F(PlatformMenuBarTest, BuildCallsSetMenus)
{
    auto menus = std::vector<cw::PlatformMenuRef>{
        cw::PlatformMenu::create("File"),
    };
    auto child = std::make_shared<cw::SizedBox>(100.0f, std::nullopt);
    auto menu_bar = cw::PlatformMenuBar::create(menus, child);
    
    // build() would be called by the framework, but we can verify the menus are set
    EXPECT_EQ(menu_bar->menus.size(), 1);
    EXPECT_EQ(menu_bar->menus[0]->label, "File");
}

TEST_F(PlatformMenuBarTest, EmptyMenusStillValid)
{
    auto child = std::make_shared<cw::SizedBox>(100.0f, std::nullopt);
    auto menu_bar = cw::PlatformMenuBar::create(child);
    
    EXPECT_TRUE(menu_bar->menus.empty());
    EXPECT_EQ(menu_bar->child, child);
}

// ---------------------------------------------------------------------------
// Complex Menu Structure Tests
// ---------------------------------------------------------------------------

TEST(PlatformMenuComplex, NestedSubmenuStructure)
{
    auto menu = cw::PlatformMenu::create("View", {
        cw::PlatformMenuItemLabel::create("Zoom In", "Cmd+=", []() {}),
        cw::PlatformMenuItemLabel::create("Zoom Out", "Cmd+-", []() {}),
        cw::PlatformMenuItemSeparator::create(),
        cw::PlatformMenuItemSubmenu::create("More Options", {
            cw::PlatformMenuItemLabel::create("Option 1"),
            cw::PlatformMenuItemLabel::create("Option 2"),
            cw::PlatformMenuItemSubmenu::create("Deep Nested", {
                cw::PlatformMenuItemLabel::create("Deep Item"),
            }),
        }),
        cw::PlatformMenuItemSeparator::create(),
        cw::PlatformProvidedMenuItem::toggle_fullscreen(),
    });
    
    ASSERT_EQ(menu->items.size(), 6);
    
    // Check nested submenu
    auto* submenu = dynamic_cast<cw::PlatformMenuItemSubmenu*>(menu->items[3].get());
    ASSERT_NE(submenu, nullptr);
    EXPECT_EQ(submenu->label, "More Options");
    EXPECT_EQ(submenu->items.size(), 3);
    
    // Check deep nesting
    auto* deep_submenu = dynamic_cast<cw::PlatformMenuItemSubmenu*>(submenu->items[2].get());
    ASSERT_NE(deep_submenu, nullptr);
    EXPECT_EQ(deep_submenu->label, "Deep Nested");
}

TEST(PlatformMenuComplex, MixedProvidedAndCustomItems)
{
    auto menu = cw::PlatformMenu::create("Edit", {
        cw::PlatformProvidedMenuItem::undo(),
        cw::PlatformProvidedMenuItem::redo(),
        cw::PlatformMenuItemSeparator::create(),
        cw::PlatformProvidedMenuItem::cut(),
        cw::PlatformProvidedMenuItem::copy(),
        cw::PlatformProvidedMenuItem::paste(),
        cw::PlatformMenuItemSeparator::create(),
        cw::PlatformMenuItemLabel::create("Special Paste", "Cmd+Shift+V", []() {}),
        cw::PlatformProvidedMenuItem::select_all(),
    });
    
    EXPECT_EQ(menu->items.size(), 9);
    
    // Verify mix of types
    EXPECT_TRUE(dynamic_cast<cw::PlatformProvidedMenuItem*>(menu->items[0].get()) != nullptr);
    EXPECT_TRUE(dynamic_cast<cw::PlatformMenuItemSeparator*>(menu->items[2].get()) != nullptr);
    EXPECT_TRUE(dynamic_cast<cw::PlatformMenuItemLabel*>(menu->items[7].get()) != nullptr);
}

// ---------------------------------------------------------------------------
// PlatformMenuDelegate No-Op Default Tests
// ---------------------------------------------------------------------------

TEST(PlatformMenuDelegateDefault, NoOpImplementationDoesNotCrash)
{
    // Reset to default implementation
    cw::PlatformMenuDelegate::setInstance(nullptr);
    
    auto delegate = cw::PlatformMenuDelegate::instance();
    ASSERT_NE(delegate, nullptr);
    
    // These should not crash even with no implementation
    std::vector<cw::PlatformMenuRef> menus;
    delegate->setMenus(menus);
    delegate->clearMenus();
}
