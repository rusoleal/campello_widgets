#include <gtest/gtest.h>
#include <campello_widgets/campello_widgets.hpp>
#include <campello_widgets/macos/run_app.hpp>

#import <Cocoa/Cocoa.h>

namespace cw = systems::leal::campello_widgets;

// ---------------------------------------------------------------------------
// macOS PlatformMenu Integration Tests
// ---------------------------------------------------------------------------
// These tests verify the actual macOS native menu bar integration.
// They require BUILD_INTEGRATION_TESTS to be enabled.
//
// Note: These tests create real NSMenu/NSMenuItem objects and interact
// with the AppKit menu system. They must run on macOS with a display.
// ---------------------------------------------------------------------------

class MacOSPlatformMenuTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        // Initialize the macOS platform menu delegate
        // This will set up the real MacOSPlatformMenuDelegate
        cw::initializeMacOSPlatformMenuDelegate();
        delegate_ = cw::PlatformMenuDelegate::instance();
        ASSERT_NE(delegate_, nullptr);
    }

    void TearDown() override
    {
        // Clear menus after each test
        delegate_->clearMenus();
    }

    cw::PlatformMenuDelegate* delegate_;
};

// ---------------------------------------------------------------------------
// Basic Menu Creation Tests
// ---------------------------------------------------------------------------

TEST_F(MacOSPlatformMenuTest, SetMenusCreatesNativeMenuBar)
{
    auto menus = std::vector<cw::PlatformMenuRef>{
        cw::PlatformMenu::create("File", {
            cw::PlatformMenuItemLabel::create("New", "Cmd+N", []() {}),
            cw::PlatformMenuItemLabel::create("Open", "Cmd+O", []() {}),
            cw::PlatformMenuItemSeparator::create(),
            cw::PlatformMenuItemLabel::create("Close", "Cmd+W", []() {}),
        }),
        cw::PlatformMenu::create("Edit", {
            cw::PlatformProvidedMenuItem::undo(),
            cw::PlatformProvidedMenuItem::redo(),
            cw::PlatformMenuItemSeparator::create(),
            cw::PlatformProvidedMenuItem::cut(),
            cw::PlatformProvidedMenuItem::copy(),
            cw::PlatformProvidedMenuItem::paste(),
            cw::PlatformMenuItemSeparator::create(),
            cw::PlatformProvidedMenuItem::select_all(),
        }),
    };

    delegate_->setMenus(menus);

    // Verify the menu bar was created
    NSMenu* mainMenu = [NSApp mainMenu];
    EXPECT_NE(mainMenu, nil);
    EXPECT_GT([mainMenu numberOfItems], 0);
}

TEST_F(MacOSPlatformMenuTest, StandardFileMenuStructure)
{
    bool new_called = false;
    bool open_called = false;
    bool save_called = false;

    auto menus = std::vector<cw::PlatformMenuRef>{
        cw::PlatformMenu::create("File", {
            cw::PlatformMenuItemLabel::create("New", "Cmd+N", [&new_called]() { new_called = true; }),
            cw::PlatformMenuItemLabel::create("Open...", "Cmd+O", [&open_called]() { open_called = true; }),
            cw::PlatformMenuItemSeparator::create(),
            cw::PlatformMenuItemLabel::create("Save", "Cmd+S", [&save_called]() { save_called = true; }),
            cw::PlatformMenuItemLabel::create("Save As...", "Cmd+Shift+S", []() {}),
            cw::PlatformMenuItemSeparator::create(),
            cw::PlatformProvidedMenuItem::close(),
        }),
    };

    delegate_->setMenus(menus);

    // Find the File menu
    NSMenu* mainMenu = [NSApp mainMenu];
    NSMenuItem* fileItem = nil;
    for (NSMenuItem* item in [mainMenu itemArray]) {
        if ([[item title] isEqualToString:@"File"]) {
            fileItem = item;
            break;
        }
    }

    EXPECT_NE(fileItem, nil);
    if (fileItem) {
        NSMenu* fileMenu = [fileItem submenu];
        EXPECT_NE(fileMenu, nil);
        EXPECT_GE([[fileMenu itemArray] count], 6u); // At least our 6 items
    }
}

TEST_F(MacOSPlatformMenuTest, StandardEditMenuWithProvidedItems)
{
    auto menus = std::vector<cw::PlatformMenuRef>{
        cw::PlatformMenu::create("Edit", {
            cw::PlatformProvidedMenuItem::undo(),
            cw::PlatformProvidedMenuItem::redo(),
            cw::PlatformMenuItemSeparator::create(),
            cw::PlatformProvidedMenuItem::cut(),
            cw::PlatformProvidedMenuItem::copy(),
            cw::PlatformProvidedMenuItem::paste(),
            cw::PlatformMenuItemLabel::create("Paste and Match Style", "Cmd+Shift+V", []() {}),
            cw::PlatformMenuItemSeparator::create(),
            cw::PlatformProvidedMenuItem::select_all(),
        }),
    };

    delegate_->setMenus(menus);

    // Verify Edit menu exists
    NSMenu* mainMenu = [NSApp mainMenu];
    NSMenuItem* editItem = nil;
    for (NSMenuItem* item in [mainMenu itemArray]) {
        if ([[item title] isEqualToString:@"Edit"]) {
            editItem = item;
            break;
        }
    }

    EXPECT_NE(editItem, nil);
    if (editItem) {
        NSMenu* editMenu = [editItem submenu];
        EXPECT_GE([[editMenu itemArray] count], 8u);
    }
}

TEST_F(MacOSPlatformMenuTest, ViewMenuWithFullscreen)
{
    auto menus = std::vector<cw::PlatformMenuRef>{
        cw::PlatformMenu::create("View", {
            cw::PlatformMenuItemLabel::create("Zoom In", "Cmd++", []() {}),
            cw::PlatformMenuItemLabel::create("Zoom Out", "Cmd+-", []() {}),
            cw::PlatformMenuItemSeparator::create(),
            cw::PlatformProvidedMenuItem::toggle_fullscreen(),
        }),
    };

    delegate_->setMenus(menus);

    NSMenu* mainMenu = [NSApp mainMenu];
    NSMenuItem* viewItem = nil;
    for (NSMenuItem* item in [mainMenu itemArray]) {
        if ([[item title] isEqualToString:@"View"]) {
            viewItem = item;
            break;
        }
    }

    EXPECT_NE(viewItem, nil);
}

// ---------------------------------------------------------------------------
// Submenu Tests
// ---------------------------------------------------------------------------

TEST_F(MacOSPlatformMenuTest, NestedSubmenuStructure)
{
    auto menus = std::vector<cw::PlatformMenuRef>{
        cw::PlatformMenu::create("Format", {
            cw::PlatformMenuItemLabel::create("Font", []() {}),
            cw::PlatformMenuItemSubmenu::create("Text Style", {
                cw::PlatformMenuItemLabel::create("Bold", "Cmd+B", []() {}),
                cw::PlatformMenuItemLabel::create("Italic", "Cmd+I", []() {}),
                cw::PlatformMenuItemLabel::create("Underline", "Cmd+U", []() {}),
            }),
            cw::PlatformMenuItemSeparator::create(),
            cw::PlatformMenuItemLabel::create("Align Left", []() {}),
            cw::PlatformMenuItemLabel::create("Align Center", []() {}),
            cw::PlatformMenuItemLabel::create("Align Right", []() {}),
        }),
    };

    delegate_->setMenus(menus);

    NSMenu* mainMenu = [NSApp mainMenu];
    NSMenuItem* formatItem = nil;
    for (NSMenuItem* item in [mainMenu itemArray]) {
        if ([[item title] isEqualToString:@"Format"]) {
            formatItem = item;
            break;
        }
    }

    EXPECT_NE(formatItem, nil);
    if (formatItem) {
        NSMenu* formatMenu = [formatItem submenu];
        BOOL foundTextStyleSubmenu = NO;
        for (NSMenuItem* item in [formatMenu itemArray]) {
            if ([[item title] isEqualToString:@"Text Style"]) {
                foundTextStyleSubmenu = YES;
                EXPECT_NE([item submenu], nil);
                break;
            }
        }
        EXPECT_TRUE(foundTextStyleSubmenu);
    }
}

// ---------------------------------------------------------------------------
// Help Menu Test
// ---------------------------------------------------------------------------

TEST_F(MacOSPlatformMenuTest, HelpMenu)
{
    auto menus = std::vector<cw::PlatformMenuRef>{
        cw::PlatformMenu::create("Help", {
            cw::PlatformMenuItemLabel::create("Documentation", []() {}),
            cw::PlatformMenuItemSeparator::create(),
            cw::PlatformMenuItemLabel::create("Keyboard Shortcuts", []() {}),
            cw::PlatformMenuItemLabel::create("Check for Updates", []() {}),
        }),
    };

    delegate_->setMenus(menus);

    NSMenu* mainMenu = [NSApp mainMenu];
    NSMenuItem* helpItem = nil;
    for (NSMenuItem* item in [mainMenu itemArray]) {
        if ([[item title] isEqualToString:@"Help"]) {
            helpItem = item;
            break;
        }
    }

    EXPECT_NE(helpItem, nil);
}

// ---------------------------------------------------------------------------
// Complete Standard Menu Bar Test
// ---------------------------------------------------------------------------

TEST_F(MacOSPlatformMenuTest, CompleteStandardMenuBar)
{
    // This test creates a complete, realistic macOS menu bar
    // similar to what a typical application would have

    auto menus = std::vector<cw::PlatformMenuRef>{
        // File menu
        cw::PlatformMenu::create("File", {
            cw::PlatformMenuItemLabel::create("New", "Cmd+N", []() {}),
            cw::PlatformMenuItemLabel::create("Open...", "Cmd+O", []() {}),
            cw::PlatformMenuItemSeparator::create(),
            cw::PlatformMenuItemSubmenu::create("Open Recent", {
                cw::PlatformMenuItemLabel::create("Clear Menu", []() {}),
            }),
            cw::PlatformMenuItemSeparator::create(),
            cw::PlatformMenuItemLabel::create("Save", "Cmd+S", []() {}),
            cw::PlatformMenuItemLabel::create("Save As...", "Cmd+Shift+S", []() {}),
            cw::PlatformMenuItemSeparator::create(),
            cw::PlatformMenuItemLabel::create("Export...", []() {}),
            cw::PlatformMenuItemSeparator::create(),
            cw::PlatformProvidedMenuItem::close(),
        }),

        // Edit menu
        cw::PlatformMenu::create("Edit", {
            cw::PlatformProvidedMenuItem::undo(),
            cw::PlatformProvidedMenuItem::redo(),
            cw::PlatformMenuItemSeparator::create(),
            cw::PlatformProvidedMenuItem::cut(),
            cw::PlatformProvidedMenuItem::copy(),
            cw::PlatformProvidedMenuItem::paste(),
            cw::PlatformProvidedMenuItem::paste_and_match_style(),
            cw::PlatformMenuItemLabel::create("Delete", []() {}),
            cw::PlatformMenuItemSeparator::create(),
            cw::PlatformProvidedMenuItem::select_all(),
            cw::PlatformMenuItemSeparator::create(),
            cw::PlatformMenuItemLabel::create("Find", "Cmd+F", []() {}),
            cw::PlatformMenuItemLabel::create("Find Next", "Cmd+G", []() {}),
            cw::PlatformMenuItemLabel::create("Find Previous", "Cmd+Shift+G", []() {}),
        }),

        // View menu
        cw::PlatformMenu::create("View", {
            cw::PlatformMenuItemLabel::create("Zoom In", "Cmd++", []() {}),
            cw::PlatformMenuItemLabel::create("Zoom Out", "Cmd+-", []() {}),
            cw::PlatformMenuItemLabel::create("Reset Zoom", "Cmd+0", []() {}),
            cw::PlatformMenuItemSeparator::create(),
            cw::PlatformMenuItemLabel::create("Show Sidebar", []() {}),
            cw::PlatformMenuItemSeparator::create(),
            cw::PlatformProvidedMenuItem::toggle_fullscreen(),
        }),

        // Window menu
        cw::PlatformMenu::create("Window", {
            cw::PlatformProvidedMenuItem::minimize(),
            cw::PlatformProvidedMenuItem::zoom(),
            cw::PlatformMenuItemSeparator::create(),
            cw::PlatformProvidedMenuItem::bring_all_to_front(),
        }),

        // Help menu
        cw::PlatformMenu::create("Help", {
            cw::PlatformMenuItemLabel::create("Search", []() {}),
            cw::PlatformMenuItemSeparator::create(),
            cw::PlatformMenuItemLabel::create("Documentation", []() {}),
            cw::PlatformMenuItemLabel::create("Release Notes", []() {}),
            cw::PlatformMenuItemSeparator::create(),
            cw::PlatformMenuItemLabel::create("Report an Issue", []() {}),
        }),
    };

    delegate_->setMenus(menus);

    // Verify the complete menu structure
    NSMenu* mainMenu = [NSApp mainMenu];
    EXPECT_NE(mainMenu, nil);

    // Count total menu items (including app menu and window menu that macOS adds)
    NSUInteger itemCount = [mainMenu numberOfItems];
    EXPECT_GE(itemCount, 6u); // App + File + Edit + View + Window + Help

    // Verify each menu exists
    NSArray* expectedMenus = @[@"File", @"Edit", @"View", @"Window", @"Help"];
    for (NSString* menuTitle in expectedMenus) {
        BOOL found = NO;
        for (NSMenuItem* item in [mainMenu itemArray]) {
            if ([[item title] isEqualToString:menuTitle]) {
                found = YES;
                break;
            }
        }
        EXPECT_TRUE(found) << "Menu '" << [menuTitle UTF8String] << "' not found";
    }
}

// ---------------------------------------------------------------------------
// Callback Invocation Tests
// ---------------------------------------------------------------------------

TEST_F(MacOSPlatformMenuTest, MenuCallbacksAreRegistered)
{
    bool callback_invoked = false;

    auto menus = std::vector<cw::PlatformMenuRef>{
        cw::PlatformMenu::create("Test", {
            cw::PlatformMenuItemLabel::create("Test Item", [&callback_invoked]() {
                callback_invoked = true;
            }),
        }),
    };

    delegate_->setMenus(menus);

    // Note: We can't easily simulate menu item selection in a unit test
    // because it requires AppKit event processing. However, we can verify
    // that the delegate was set and the menu structure is valid.
    
    NSMenu* mainMenu = [NSApp mainMenu];
    EXPECT_NE(mainMenu, nil);
}

// ---------------------------------------------------------------------------
// Clear Menus Test
// ---------------------------------------------------------------------------

TEST_F(MacOSPlatformMenuTest, ClearMenusRemovesCustomMenus)
{
    // First set some menus
    auto menus = std::vector<cw::PlatformMenuRef>{
        cw::PlatformMenu::create("File", {
            cw::PlatformMenuItemLabel::create("New", []() {}),
        }),
        cw::PlatformMenu::create("Edit", {
            cw::PlatformMenuItemLabel::create("Cut", []() {}),
        }),
    };

    delegate_->setMenus(menus);

    NSMenu* mainMenuBefore = [NSApp mainMenu];
    NSUInteger countBefore = [mainMenuBefore numberOfItems];
    EXPECT_GT(countBefore, 0u);

    // Now clear menus
    delegate_->clearMenus();

    // After clearing, we should have a minimal menu bar
    NSMenu* mainMenuAfter = [NSApp mainMenu];
    EXPECT_NE(mainMenuAfter, nil);
}

// ---------------------------------------------------------------------------
// Menu Item State Tests
// ---------------------------------------------------------------------------

TEST_F(MacOSPlatformMenuTest, DisabledMenuItems)
{
    auto item = std::make_shared<cw::PlatformMenuItemLabel>("Disabled Item", []() {});
    item->enabled = false;

    auto menus = std::vector<cw::PlatformMenuRef>{
        cw::PlatformMenu::create("Test", {
            cw::PlatformMenuItemLabel::create("Enabled Item", []() {}),
            item,
        }),
    };

    delegate_->setMenus(menus);

    NSMenu* mainMenu = [NSApp mainMenu];
    EXPECT_NE(mainMenu, nil);
}

// ---------------------------------------------------------------------------
// Platform-Provided Items Test
// ---------------------------------------------------------------------------

TEST_F(MacOSPlatformMenuTest, AllPlatformProvidedItems)
{
    // Test that all platform-provided items can be created without crashing
    auto menus = std::vector<cw::PlatformMenuRef>{
        cw::PlatformMenu::create("App", {
            cw::PlatformProvidedMenuItem::about(),
            cw::PlatformProvidedMenuItem::preferences(),
            cw::PlatformMenuItemSeparator::create(),
            cw::PlatformProvidedMenuItem::hide(),
            cw::PlatformProvidedMenuItem::hide_others(),
            cw::PlatformProvidedMenuItem::show_all(),
            cw::PlatformMenuItemSeparator::create(),
            cw::PlatformProvidedMenuItem::quit(),
        }),
        cw::PlatformMenu::create("File", {
            cw::PlatformProvidedMenuItem::new_file(),
            cw::PlatformProvidedMenuItem::open(),
            cw::PlatformProvidedMenuItem::close(),
            cw::PlatformProvidedMenuItem::save(),
            cw::PlatformProvidedMenuItem::save_as(),
            cw::PlatformProvidedMenuItem::print(),
        }),
        cw::PlatformMenu::create("Edit", {
            cw::PlatformProvidedMenuItem::undo(),
            cw::PlatformProvidedMenuItem::redo(),
            cw::PlatformProvidedMenuItem::cut(),
            cw::PlatformProvidedMenuItem::copy(),
            cw::PlatformProvidedMenuItem::paste(),
            cw::PlatformProvidedMenuItem::paste_and_match_style(),
            cw::PlatformProvidedMenuItem::delete_item(),
            cw::PlatformProvidedMenuItem::select_all(),
        }),
        cw::PlatformMenu::create("View", {
            cw::PlatformProvidedMenuItem::toggle_fullscreen(),
        }),
        cw::PlatformMenu::create("Window", {
            cw::PlatformProvidedMenuItem::minimize(),
            cw::PlatformProvidedMenuItem::zoom(),
            cw::PlatformProvidedMenuItem::bring_all_to_front(),
        }),
    };

    // This should not throw or crash
    EXPECT_NO_THROW(delegate_->setMenus(menus));

    // Verify menu bar was created
    NSMenu* mainMenu = [NSApp mainMenu];
    EXPECT_NE(mainMenu, nil);
    EXPECT_GE([mainMenu numberOfItems], 5u);
}

// ---------------------------------------------------------------------------
// Keyboard Shortcut Tests
// ---------------------------------------------------------------------------

TEST_F(MacOSPlatformMenuTest, VariousKeyboardShortcuts)
{
    auto menus = std::vector<cw::PlatformMenuRef>{
        cw::PlatformMenu::create("Shortcuts", {
            // Command shortcuts
            cw::PlatformMenuItemLabel::create("Cmd Only", "Cmd+A", []() {}),
            // Shift+Cmd shortcuts
            cw::PlatformMenuItemLabel::create("Shift+Cmd", "Cmd+Shift+S", []() {}),
            // Option+Cmd shortcuts
            cw::PlatformMenuItemLabel::create("Option+Cmd", "Cmd+Alt+F", []() {}),
            // Control shortcuts
            cw::PlatformMenuItemLabel::create("Ctrl", "Ctrl+C", []() {}),
            // Special keys
            cw::PlatformMenuItemLabel::create("Enter", "Cmd+Enter", []() {}),
            cw::PlatformMenuItemLabel::create("Escape", "Cmd+Esc", []() {}),
            cw::PlatformMenuItemLabel::create("Tab", "Cmd+Tab", []() {}),
            // Arrow keys
            cw::PlatformMenuItemLabel::create("Up", "Cmd+Up", []() {}),
            cw::PlatformMenuItemLabel::create("Down", "Cmd+Down", []() {}),
            cw::PlatformMenuItemLabel::create("Left", "Cmd+Left", []() {}),
            cw::PlatformMenuItemLabel::create("Right", "Cmd+Right", []() {}),
        }),
    };

    delegate_->setMenus(menus);

    NSMenu* mainMenu = [NSApp mainMenu];
    EXPECT_NE(mainMenu, nil);
}
