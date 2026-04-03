#include <campello_widgets/campello_widgets.hpp>
#include <campello_widgets/macos/run_app.hpp>

#include <iostream>
#include <string>

namespace cw = systems::leal::campello_widgets;

// ---------------------------------------------------------------------------
// PlatformMenu Test Application
// ---------------------------------------------------------------------------
// This application demonstrates the PlatformMenu/PlatformMenuBar functionality
// with standard macOS menus (File, Edit, View, Window, Help).
// 
// Run this and check the menu bar - you should see all standard menus
// with working keyboard shortcuts.
// ---------------------------------------------------------------------------

static cw::TextStyle makeStyle(float size, cw::Color color)
{
    cw::TextStyle s{};
    s.font_family = "Helvetica Neue";
    s.font_size   = size;
    s.color       = color;
    return s;
}

// Track which menu items were clicked
struct MenuState {
    std::string last_action = "No action yet";
    int action_count = 0;
};

static MenuState g_menu_state;

// Helper to update state when a menu item is clicked
static std::function<void()> menuAction(const std::string& action_name)
{
    return [action_name]() {
        g_menu_state.last_action = action_name;
        g_menu_state.action_count++;
        std::cout << "[Menu] " << action_name << " clicked! (total: " 
                  << g_menu_state.action_count << ")" << std::endl;
    };
}

// ---------------------------------------------------------------------------
// Main Application Widget
// ---------------------------------------------------------------------------

class MenuTestApp;

class MenuTestAppState : public cw::State<MenuTestApp>
{
public:
    void initState() override
    {
        g_menu_state.last_action = "No action yet";
        g_menu_state.action_count = 0;
    }

    cw::WidgetRef build(cw::BuildContext&) override
    {
        auto title_style = makeStyle(24.0f, cw::Color::fromRGB(0.1f, 0.1f, 0.1f));
        auto label_style = makeStyle(14.0f, cw::Color::fromRGB(0.4f, 0.4f, 0.4f));
        auto action_style = makeStyle(18.0f, cw::Color::fromRGB(0.08f, 0.47f, 0.95f));
        auto hint_style = makeStyle(12.0f, cw::Color::fromRGB(0.6f, 0.6f, 0.6f));

        // Status display
        auto status_col = cw::mw<cw::Column>(
            cw::MainAxisAlignment::center,
            cw::CrossAxisAlignment::center,
            cw::WidgetList{
                cw::mw<cw::Text>("PlatformMenu Test", title_style),
                cw::mw<cw::Padding>(
                    cw::EdgeInsets::only(0, 16, 0, 8),
                    cw::mw<cw::Text>("Last action:", label_style)
                ),
                cw::mw<cw::Text>(g_menu_state.last_action, action_style),
                cw::mw<cw::Padding>(
                    cw::EdgeInsets::only(0, 24, 0, 0),
                    cw::mw<cw::Text>(
                        "Action count: " + std::to_string(g_menu_state.action_count),
                        label_style
                    )
                ),
                cw::mw<cw::Padding>(
                    cw::EdgeInsets::only(32, 8, 0, 0),
                    cw::mw<cw::Text>("Try the menu items and keyboard shortcuts!", hint_style)
                ),
                cw::mw<cw::Text>("Cmd+N, Cmd+O, Cmd+S, Cmd+Z, Cmd+C, Cmd+V, etc.", hint_style),
            }
        );

        auto container = std::make_shared<cw::Container>();
        container->color = cw::Color::fromRGB(0.97f, 0.97f, 1.0f);
        container->child = cw::mw<cw::Center>(status_col);

        return container;
    }
};

class MenuTestApp : public cw::StatefulWidget
{
public:
    std::unique_ptr<cw::StateBase> createState() const override
    {
        return std::make_unique<MenuTestAppState>();
    }
};

// ---------------------------------------------------------------------------
// Build Standard Menu Bar
// ---------------------------------------------------------------------------

static std::vector<cw::PlatformMenuRef> buildStandardMenuBar()
{
    return {
        // File Menu
        cw::PlatformMenu::create("File", {
            cw::PlatformMenuItemLabel::create("New", "Cmd+N", menuAction("New")),
            cw::PlatformMenuItemLabel::create("Open...", "Cmd+O", menuAction("Open")),
            cw::PlatformMenuItemSeparator::create(),
            cw::PlatformMenuItemSubmenu::create("Open Recent", {
                cw::PlatformMenuItemLabel::create("Document 1.txt", menuAction("Open Recent: Document 1")),
                cw::PlatformMenuItemLabel::create("Document 2.txt", menuAction("Open Recent: Document 2")),
                cw::PlatformMenuItemSeparator::create(),
                cw::PlatformMenuItemLabel::create("Clear Menu", menuAction("Clear Recent")),
            }),
            cw::PlatformMenuItemSeparator::create(),
            cw::PlatformMenuItemLabel::create("Save", "Cmd+S", menuAction("Save")),
            cw::PlatformMenuItemLabel::create("Save As...", "Cmd+Shift+S", menuAction("Save As")),
            cw::PlatformMenuItemSeparator::create(),
            cw::PlatformMenuItemLabel::create("Export...", menuAction("Export")),
            cw::PlatformMenuItemLabel::create("Print...", "Cmd+P", menuAction("Print")),
            cw::PlatformMenuItemSeparator::create(),
            cw::PlatformProvidedMenuItem::close(),
        }),

        // Edit Menu
        cw::PlatformMenu::create("Edit", {
            cw::PlatformProvidedMenuItem::undo(),
            cw::PlatformProvidedMenuItem::redo(),
            cw::PlatformMenuItemSeparator::create(),
            cw::PlatformProvidedMenuItem::cut(),
            cw::PlatformProvidedMenuItem::copy(),
            cw::PlatformProvidedMenuItem::paste(),
            cw::PlatformProvidedMenuItem::paste_and_match_style(),
            cw::PlatformMenuItemLabel::create("Delete", menuAction("Delete")),
            cw::PlatformMenuItemSeparator::create(),
            cw::PlatformProvidedMenuItem::select_all(),
            cw::PlatformMenuItemSeparator::create(),
            cw::PlatformMenuItemLabel::create("Find", "Cmd+F", menuAction("Find")),
            cw::PlatformMenuItemLabel::create("Find Next", "Cmd+G", menuAction("Find Next")),
            cw::PlatformMenuItemLabel::create("Find Previous", "Cmd+Shift+G", menuAction("Find Previous")),
            cw::PlatformMenuItemSeparator::create(),
            cw::PlatformMenuItemLabel::create("Preferences...", "Cmd+,", menuAction("Preferences")),
        }),

        // View Menu
        cw::PlatformMenu::create("View", {
            cw::PlatformMenuItemLabel::create("Zoom In", "Cmd++", menuAction("Zoom In")),
            cw::PlatformMenuItemLabel::create("Zoom Out", "Cmd+-", menuAction("Zoom Out")),
            cw::PlatformMenuItemLabel::create("Reset Zoom", "Cmd+0", menuAction("Reset Zoom")),
            cw::PlatformMenuItemSeparator::create(),
            cw::PlatformMenuItemLabel::create("Show Sidebar", menuAction("Show Sidebar")),
            cw::PlatformMenuItemLabel::create("Show Toolbar", menuAction("Show Toolbar")),
            cw::PlatformMenuItemSeparator::create(),
            cw::PlatformProvidedMenuItem::toggle_fullscreen(),
        }),

        // Format Menu (with nested submenus)
        cw::PlatformMenu::create("Format", {
            cw::PlatformMenuItemSubmenu::create("Font", {
                cw::PlatformMenuItemLabel::create("Show Fonts", "Cmd+T", menuAction("Show Fonts")),
                cw::PlatformMenuItemSeparator::create(),
                cw::PlatformMenuItemLabel::create("Bold", "Cmd+B", menuAction("Bold")),
                cw::PlatformMenuItemLabel::create("Italic", "Cmd+I", menuAction("Italic")),
                cw::PlatformMenuItemLabel::create("Underline", "Cmd+U", menuAction("Underline")),
                cw::PlatformMenuItemSeparator::create(),
                cw::PlatformMenuItemLabel::create("Bigger", "Cmd++", menuAction("Bigger")),
                cw::PlatformMenuItemLabel::create("Smaller", "Cmd+-", menuAction("Smaller")),
            }),
            cw::PlatformMenuItemSubmenu::create("Text", {
                cw::PlatformMenuItemLabel::create("Align Left", menuAction("Align Left")),
                cw::PlatformMenuItemLabel::create("Align Center", menuAction("Align Center")),
                cw::PlatformMenuItemLabel::create("Align Right", menuAction("Align Right")),
                cw::PlatformMenuItemLabel::create("Justify", menuAction("Justify")),
                cw::PlatformMenuItemSeparator::create(),
                cw::PlatformMenuItemSubmenu::create("Spacing", {
                    cw::PlatformMenuItemLabel::create("Use None", menuAction("Spacing: None")),
                    cw::PlatformMenuItemLabel::create("Use Single", menuAction("Spacing: Single")),
                    cw::PlatformMenuItemLabel::create("Use Double", menuAction("Spacing: Double")),
                }),
            }),
        }),

        // Window Menu
        cw::PlatformMenu::create("Window", {
            cw::PlatformProvidedMenuItem::minimize(),
            cw::PlatformProvidedMenuItem::zoom(),
            cw::PlatformMenuItemSeparator::create(),
            cw::PlatformProvidedMenuItem::bring_all_to_front(),
        }),

        // Help Menu
        cw::PlatformMenu::create("Help", {
            cw::PlatformMenuItemLabel::create("Search", menuAction("Search Help")),
            cw::PlatformMenuItemSeparator::create(),
            cw::PlatformMenuItemLabel::create("Documentation", menuAction("Documentation")),
            cw::PlatformMenuItemLabel::create("Release Notes", menuAction("Release Notes")),
            cw::PlatformMenuItemSeparator::create(),
            cw::PlatformProvidedMenuItem::about(),
        }),
    };
}

// ---------------------------------------------------------------------------
// Entry Point
// ---------------------------------------------------------------------------

int main()
{
    std::cout << "============================================" << std::endl;
    std::cout << "PlatformMenu Test Application" << std::endl;
    std::cout << "============================================" << std::endl;
    std::cout << "This app tests the macOS PlatformMenu implementation." << std::endl;
    std::cout << "Try the menu bar items and keyboard shortcuts!" << std::endl;
    std::cout << "============================================" << std::endl;

    // Build and set the standard menu bar
    auto menus = buildStandardMenuBar();
    
    // Create the menu bar widget wrapper
    auto app_widget = std::make_shared<MenuTestApp>();
    auto menu_bar = cw::PlatformMenuBar::create(menus, app_widget);

    return cw::runApp(
        menu_bar,
        "PlatformMenu Test",
        600.0f,
        400.0f);
}
