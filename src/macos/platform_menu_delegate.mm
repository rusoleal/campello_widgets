#include <campello_widgets/widgets/platform_menu_delegate.hpp>
#include <campello_widgets/widgets/platform_menu.hpp>
#import <Cocoa/Cocoa.h>
#include <map>
#include <mutex>

namespace systems::leal::campello_widgets
{

    // -------------------------------------------------------------------------
    // Callback forwarding
    // -------------------------------------------------------------------------
    
    // Global map to store callbacks by ID - accessed from Objective-C
    static std::mutex g_callbacks_mutex;
    static std::map<uint64_t, std::function<void()>> g_callbacks;
    static uint64_t g_next_callback_id = 1;

    static uint64_t register_menu_callback(std::function<void()> callback)
    {
        std::lock_guard<std::mutex> lock(g_callbacks_mutex);
        uint64_t id = g_next_callback_id++;
        g_callbacks[id] = std::move(callback);
        return id;
    }

    static void clear_menu_callbacks()
    {
        std::lock_guard<std::mutex> lock(g_callbacks_mutex);
        g_callbacks.clear();
        g_next_callback_id = 1;
    }

    static void invoke_menu_callback(uint64_t callback_id)
    {
        std::function<void()> callback;
        {
            std::lock_guard<std::mutex> lock(g_callbacks_mutex);
            auto it = g_callbacks.find(callback_id);
            if (it != g_callbacks.end()) {
                callback = it->second;
            }
        }
        if (callback) {
            callback();
        }
    }

} // namespace systems::leal::campello_widgets

// Objective-C interface for menu item callbacks
@interface CampelloMenuItemTarget : NSObject
- (instancetype)initWithCallbackId:(uint64_t)callbackId;
- (void)menuItemSelected:(id)sender;
@end

@implementation CampelloMenuItemTarget {
    uint64_t _callbackId;
}

- (instancetype)initWithCallbackId:(uint64_t)callbackId
{
    self = [super init];
    if (self) {
        _callbackId = callbackId;
    }
    return self;
}

- (void)menuItemSelected:(id)sender
{
    (void)sender;
    systems::leal::campello_widgets::invoke_menu_callback(_callbackId);
}

@end

namespace systems::leal::campello_widgets
{

    // -------------------------------------------------------------------------
    // macOS PlatformMenuDelegate Implementation
    // -------------------------------------------------------------------------

    class MacOSPlatformMenuDelegate : public PlatformMenuDelegate
    {
    public:
        MacOSPlatformMenuDelegate();
        ~MacOSPlatformMenuDelegate();

        void setMenus(const std::vector<PlatformMenuRef>& menus) override;
        void clearMenus() override;

    private:
        void buildMenuBar(const std::vector<PlatformMenuRef>& menus);
        NSMenu* createMenu(const PlatformMenu& menu);
        NSMenu* createMenuFromSubmenu(const PlatformMenuItemSubmenu& submenu);
        NSMenuItem* createMenuItem(const PlatformMenuItem* item);
        NSMenuItem* createLabelItem(const PlatformMenuItemLabel* item);
        NSMenuItem* createSeparatorItem();
        NSMenuItem* createSubmenuItem(const PlatformMenuItemSubmenu* item);
        NSMenuItem* createProvidedItem(const PlatformProvidedMenuItem* item);
        
        NSString* convertLabelToNSString(const std::string& label);
        NSString* convertShortcutToKeyEquivalent(const std::string& shortcut, uint32_t* outModifiers);

        NSMutableArray<CampelloMenuItemTarget*>* _targets;
        bool _has_custom_menus = false;
    };

    MacOSPlatformMenuDelegate::MacOSPlatformMenuDelegate()
        : _targets([NSMutableArray array])
    {
        // Retain the targets array
        [_targets retain];
    }

    MacOSPlatformMenuDelegate::~MacOSPlatformMenuDelegate()
    {
        [_targets release];
    }

    void MacOSPlatformMenuDelegate::setMenus(const std::vector<PlatformMenuRef>& menus)
    {
        // Clear existing callbacks
        clear_menu_callbacks();
        
        // Release old targets
        [_targets removeAllObjects];
        
        buildMenuBar(menus);
        _has_custom_menus = !menus.empty();
    }

    void MacOSPlatformMenuDelegate::clearMenus()
    {
        clear_menu_callbacks();
        [_targets removeAllObjects];
        
        // Reset to a basic menu bar with just the app menu
        NSMenu* menuBar = [[NSMenu alloc] init];
        
        // Add basic app menu
        NSMenuItem* appItem = [[NSMenuItem alloc] init];
        NSMenu* appMenu = [[NSMenu alloc] init];
        [appMenu addItemWithTitle:@"Quit"
                           action:@selector(terminate:)
                    keyEquivalent:@"q"];
        appItem.submenu = appMenu;
        [menuBar addItem:appItem];
        
        [NSApp setMainMenu:menuBar];
        
        [menuBar release];
        [appItem release];
        [appMenu release];
        
        _has_custom_menus = false;
    }

    void MacOSPlatformMenuDelegate::buildMenuBar(const std::vector<PlatformMenuRef>& menus)
    {
        // Create the main menu bar
        NSMenu* menuBar = [[NSMenu alloc] init];
        
        // First, add the application menu (required on macOS)
        NSMenuItem* appItem = [[NSMenuItem alloc] init];
        NSString* appName = [[NSProcessInfo processInfo] processName];
        NSMenu* appMenu = [[NSMenu alloc] initWithTitle:appName];
        
        // Add About item
        [appMenu addItemWithTitle:[NSString stringWithFormat:@"About %@", appName]
                           action:@selector(orderFrontStandardAboutPanel:)
                    keyEquivalent:@""];
        [appMenu addItem:[NSMenuItem separatorItem]];
        
        // Add Preferences if available
        // (Could be provided by the user menu structure)
        
        // Hide/Show items
        [appMenu addItemWithTitle:[NSString stringWithFormat:@"Hide %@", appName]
                           action:@selector(hide:)
                    keyEquivalent:@"h"];
        [appMenu addItemWithTitle:@"Hide Others"
                           action:@selector(hideOtherApplications:)
                    keyEquivalent:@"h"];
        [appMenu itemAtIndex:[appMenu numberOfItems] - 1].keyEquivalentModifierMask = NSEventModifierFlagOption | NSEventModifierFlagCommand;
        [appMenu addItemWithTitle:@"Show All"
                           action:@selector(unhideAllApplications:)
                    keyEquivalent:@""];
        [appMenu addItem:[NSMenuItem separatorItem]];
        
        // Quit item
        [appMenu addItemWithTitle:[NSString stringWithFormat:@"Quit %@", appName]
                           action:@selector(terminate:)
                    keyEquivalent:@"q"];
        
        appItem.submenu = appMenu;
        [menuBar addItem:appItem];
        
        // Add user-defined menus
        for (const auto& menu : menus) {
            if (!menu) continue;
            
            NSMenuItem* item = [[NSMenuItem alloc] init];
            item.submenu = createMenu(*menu);
            [menuBar addItem:item];
            [item release];
        }
        
        // Window menu (standard macOS menu)
        NSMenuItem* windowItem = [[NSMenuItem alloc] init];
        NSMenu* windowMenu = [[NSMenu alloc] initWithTitle:@"Window"];
        [windowMenu addItemWithTitle:@"Minimize"
                              action:@selector(performMiniaturize:)
                       keyEquivalent:@"m"];
        [windowMenu addItemWithTitle:@"Zoom"
                              action:@selector(performZoom:)
                       keyEquivalent:@""];
        [windowMenu addItem:[NSMenuItem separatorItem]];
        [windowMenu addItemWithTitle:@"Bring All to Front"
                              action:@selector(arrangeInFront:)
                       keyEquivalent:@""];
        windowItem.submenu = windowMenu;
        [NSApp setWindowsMenu:windowMenu];
        [menuBar addItem:windowItem];
        
        // Set the main menu
        [NSApp setMainMenu:menuBar];
        
        // Clean up
        [menuBar release];
        [appItem release];
        [appMenu release];
        [windowItem release];
        [windowMenu release];
    }

    NSMenu* MacOSPlatformMenuDelegate::createMenu(const PlatformMenu& menu)
    {
        NSMenu* nsMenu = [[NSMenu alloc] initWithTitle:convertLabelToNSString(menu.label)];
        
        for (const auto& item : menu.items) {
            if (!item) continue;
            
            NSMenuItem* nsItem = createMenuItem(item.get());
            if (nsItem) {
                [nsMenu addItem:nsItem];
                [nsItem release];
            }
        }
        
        return nsMenu;
    }

    NSMenu* MacOSPlatformMenuDelegate::createMenuFromSubmenu(const PlatformMenuItemSubmenu& menu)
    {
        NSMenu* nsMenu = [[NSMenu alloc] initWithTitle:convertLabelToNSString(menu.label)];
        
        for (const auto& item : menu.items) {
            if (!item) continue;
            
            NSMenuItem* nsItem = createMenuItem(item.get());
            if (nsItem) {
                [nsMenu addItem:nsItem];
                [nsItem release];
            }
        }
        
        return nsMenu;
    }

    NSMenuItem* MacOSPlatformMenuDelegate::createMenuItem(const PlatformMenuItem* item)
    {
        if (auto* label = dynamic_cast<const PlatformMenuItemLabel*>(item)) {
            return createLabelItem(label);
        }
        if (dynamic_cast<const PlatformMenuItemSeparator*>(item)) {
            return createSeparatorItem();
        }
        if (auto* submenu = dynamic_cast<const PlatformMenuItemSubmenu*>(item)) {
            return createSubmenuItem(submenu);
        }
        if (auto* provided = dynamic_cast<const PlatformProvidedMenuItem*>(item)) {
            return createProvidedItem(provided);
        }
        return nil;
    }

    NSMenuItem* MacOSPlatformMenuDelegate::createLabelItem(const PlatformMenuItemLabel* item)
    {
        uint32_t modifiers = 0;
        NSString* keyEquiv = convertShortcutToKeyEquivalent(item->shortcut, &modifiers);
        
        NSMenuItem* nsItem = [[NSMenuItem alloc] 
            initWithTitle:convertLabelToNSString(item->label)
                   action:@selector(menuItemSelected:)
            keyEquivalent:keyEquiv];
        
        nsItem.keyEquivalentModifierMask = modifiers;
        nsItem.enabled = item->enabled ? YES : NO;
        
        // Register callback if present
        if (item->on_selected) {
            uint64_t callbackId = register_menu_callback(item->on_selected);
            CampelloMenuItemTarget* target = [[CampelloMenuItemTarget alloc] initWithCallbackId:callbackId];
            nsItem.target = target;
            [_targets addObject:target];
            [target release];
        }
        
        return nsItem;
    }

    NSMenuItem* MacOSPlatformMenuDelegate::createSeparatorItem()
    {
        return [NSMenuItem separatorItem];
    }

    NSMenuItem* MacOSPlatformMenuDelegate::createSubmenuItem(const PlatformMenuItemSubmenu* item)
    {
        NSMenuItem* nsItem = [[NSMenuItem alloc] 
            initWithTitle:convertLabelToNSString(item->label)
                   action:nil
            keyEquivalent:@""];
        
        nsItem.enabled = item->enabled ? YES : NO;
        nsItem.submenu = createMenuFromSubmenu(*item);
        
        return nsItem;
    }

    NSMenuItem* MacOSPlatformMenuDelegate::createProvidedItem(const PlatformProvidedMenuItem* item)
    {
        SEL action = nil;
        NSString* keyEquiv = @"";
        uint32_t modifiers = NSEventModifierFlagCommand;
        NSString* title = @"";
        
        switch (item->type) {
            case PlatformProvidedMenuItemType::about:
                title = @"About";
                action = @selector(orderFrontStandardAboutPanel:);
                break;
            case PlatformProvidedMenuItemType::preferences:
                title = @"Preferences...";
                keyEquiv = @",";
                // Preferences typically needs a custom handler
                break;
            case PlatformProvidedMenuItemType::hide:
                title = @"Hide";
                keyEquiv = @"h";
                action = @selector(hide:);
                break;
            case PlatformProvidedMenuItemType::hide_others:
                title = @"Hide Others";
                keyEquiv = @"h";
                modifiers = NSEventModifierFlagOption | NSEventModifierFlagCommand;
                action = @selector(hideOtherApplications:);
                break;
            case PlatformProvidedMenuItemType::show_all:
                title = @"Show All";
                action = @selector(unhideAllApplications:);
                break;
            case PlatformProvidedMenuItemType::quit:
                title = @"Quit";
                keyEquiv = @"q";
                action = @selector(terminate:);
                break;
            case PlatformProvidedMenuItemType::new_file:
                title = @"New";
                keyEquiv = @"n";
                break;
            case PlatformProvidedMenuItemType::open:
                title = @"Open...";
                keyEquiv = @"o";
                break;
            case PlatformProvidedMenuItemType::close:
                title = @"Close";
                keyEquiv = @"w";
                action = @selector(performClose:);
                break;
            case PlatformProvidedMenuItemType::save:
                title = @"Save";
                keyEquiv = @"s";
                break;
            case PlatformProvidedMenuItemType::save_as:
                title = @"Save As...";
                keyEquiv = @"S";
                break;
            case PlatformProvidedMenuItemType::print:
                title = @"Print...";
                keyEquiv = @"p";
                break;
            case PlatformProvidedMenuItemType::undo:
                title = @"Undo";
                keyEquiv = @"z";
                break;
            case PlatformProvidedMenuItemType::redo:
                title = @"Redo";
                keyEquiv = @"Z";
                break;
            case PlatformProvidedMenuItemType::cut:
                title = @"Cut";
                keyEquiv = @"x";
                action = @selector(cut:);
                break;
            case PlatformProvidedMenuItemType::copy:
                title = @"Copy";
                keyEquiv = @"c";
                action = @selector(copy:);
                break;
            case PlatformProvidedMenuItemType::paste:
                title = @"Paste";
                keyEquiv = @"v";
                action = @selector(paste:);
                break;
            case PlatformProvidedMenuItemType::paste_and_match_style:
                title = @"Paste and Match Style";
                keyEquiv = @"V";
                modifiers = NSEventModifierFlagOption | NSEventModifierFlagCommand;
                action = @selector(pasteAsPlainText:);
                break;
            case PlatformProvidedMenuItemType::delete_item:
                title = @"Delete";
                action = @selector(delete:);
                break;
            case PlatformProvidedMenuItemType::select_all:
                title = @"Select All";
                keyEquiv = @"a";
                action = @selector(selectAll:);
                break;
            case PlatformProvidedMenuItemType::toggle_fullscreen:
                title = @"Toggle Full Screen";
                keyEquiv = @"f";
                modifiers = NSEventModifierFlagControl | NSEventModifierFlagCommand;
                action = @selector(toggleFullScreen:);
                break;
            case PlatformProvidedMenuItemType::minimize:
                title = @"Minimize";
                keyEquiv = @"m";
                action = @selector(performMiniaturize:);
                break;
            case PlatformProvidedMenuItemType::zoom:
                title = @"Zoom";
                action = @selector(performZoom:);
                break;
            case PlatformProvidedMenuItemType::bring_all_to_front:
                title = @"Bring All to Front";
                action = @selector(arrangeInFront:);
                break;
        }
        
        NSMenuItem* nsItem = [[NSMenuItem alloc] 
            initWithTitle:title
                   action:action
            keyEquivalent:keyEquiv];
        
        nsItem.keyEquivalentModifierMask = modifiers;
        nsItem.enabled = item->enabled ? YES : NO;
        
        return nsItem;
    }

    NSString* MacOSPlatformMenuDelegate::convertLabelToNSString(const std::string& label)
    {
        return [NSString stringWithUTF8String:label.c_str()];
    }

    NSString* MacOSPlatformMenuDelegate::convertShortcutToKeyEquivalent(const std::string& shortcut, uint32_t* outModifiers)
    {
        if (shortcut.empty() || !outModifiers) {
            *outModifiers = 0;
            return @"";
        }
        
        *outModifiers = NSEventModifierFlagCommand;  // Default to Cmd
        
        // Parse shortcut format like "Cmd+O", "Ctrl+Shift+S", "Alt+F4"
        std::string key;
        size_t pos = 0;
        
        // Check for modifiers
        if (shortcut.find("Cmd+") != std::string::npos || 
            shortcut.find("cmd+") != std::string::npos) {
            *outModifiers = NSEventModifierFlagCommand;
            pos = shortcut.find('+') + 1;
        } else if (shortcut.find("Ctrl+") != std::string::npos || 
                   shortcut.find("ctrl+") != std::string::npos) {
            *outModifiers = NSEventModifierFlagControl;
            pos = shortcut.find('+') + 1;
        } else if (shortcut.find("Alt+") != std::string::npos || 
                   shortcut.find("alt+") != std::string::npos) {
            *outModifiers = NSEventModifierFlagOption;
            pos = shortcut.find('+') + 1;
        }
        
        // Check for additional modifiers
        size_t next_pos = shortcut.find('+', pos);
        while (next_pos != std::string::npos) {
            std::string mod = shortcut.substr(pos, next_pos - pos);
            if (mod == "Shift" || mod == "shift") {
                *outModifiers |= NSEventModifierFlagShift;
            } else if (mod == "Cmd" || mod == "cmd") {
                *outModifiers |= NSEventModifierFlagCommand;
            } else if (mod == "Ctrl" || mod == "ctrl") {
                *outModifiers |= NSEventModifierFlagControl;
            } else if (mod == "Alt" || mod == "alt") {
                *outModifiers |= NSEventModifierFlagOption;
            }
            pos = next_pos + 1;
            next_pos = shortcut.find('+', pos);
        }
        
        // Remaining part is the key
        key = shortcut.substr(pos);
        
        // Convert key to NSString (lowercase for key equivalent)
        if (key.empty()) {
            return @"";
        }
        
        // Single character keys
        if (key.length() == 1) {
            return [NSString stringWithUTF8String:key.c_str()].lowercaseString;
        }
        
        // Special keys
        if (key == "Enter" || key == "Return") return @"\r";
        if (key == "Escape" || key == "Esc") return @"\u001b";
        if (key == "Tab") return @"\t";
        if (key == "Up") return @"\uF700";
        if (key == "Down") return @"\uF701";
        if (key == "Left") return @"\uF702";
        if (key == "Right") return @"\uF703";
        
        // Default: return first character lowercase
        return [NSString stringWithUTF8String:key.substr(0, 1).c_str()].lowercaseString;
    }

    // -------------------------------------------------------------------------
    // Platform Menu Delegate Factory
    // -------------------------------------------------------------------------

    void initializeMacOSPlatformMenuDelegate()
    {
        PlatformMenuDelegate::setInstance(std::make_unique<MacOSPlatformMenuDelegate>());
    }

} // namespace systems::leal::campello_widgets

// C interface for run_app.mm to call
extern "C" void campello_widgets_initialize_macos_menu_delegate()
{
    systems::leal::campello_widgets::initializeMacOSPlatformMenuDelegate();
}
