#include <campello_widgets/widgets/platform_menu.hpp>

namespace systems::leal::campello_widgets
{

    // -------------------------------------------------------------------------
    // PlatformMenuItemLabel
    // -------------------------------------------------------------------------

    std::unique_ptr<PlatformMenuItem> PlatformMenuItemLabel::clone() const
    {
        auto copy = std::make_unique<PlatformMenuItemLabel>();
        copy->label = label;
        copy->shortcut = shortcut;
        copy->on_selected = on_selected;  // Copy the function
        copy->enabled = enabled;
        return copy;
    }

    PlatformMenuItemRef PlatformMenuItemLabel::create(std::string label)
    {
        return std::make_shared<PlatformMenuItemLabel>(std::move(label));
    }

    PlatformMenuItemRef PlatformMenuItemLabel::create(std::string label, std::function<void()> on_selected)
    {
        return std::make_shared<PlatformMenuItemLabel>(std::move(label), std::move(on_selected));
    }

    PlatformMenuItemRef PlatformMenuItemLabel::create(std::string label, std::string shortcut, std::function<void()> on_selected)
    {
        return std::make_shared<PlatformMenuItemLabel>(std::move(label), std::move(shortcut), std::move(on_selected));
    }

    // -------------------------------------------------------------------------
    // PlatformMenuItemSeparator
    // -------------------------------------------------------------------------

    std::unique_ptr<PlatformMenuItem> PlatformMenuItemSeparator::clone() const
    {
        return std::make_unique<PlatformMenuItemSeparator>();
    }

    PlatformMenuItemRef PlatformMenuItemSeparator::create()
    {
        return std::make_shared<PlatformMenuItemSeparator>();
    }

    // -------------------------------------------------------------------------
    // PlatformMenuItemSubmenu
    // -------------------------------------------------------------------------

    std::unique_ptr<PlatformMenuItem> PlatformMenuItemSubmenu::clone() const
    {
        auto copy = std::make_unique<PlatformMenuItemSubmenu>();
        copy->label = label;
        copy->enabled = enabled;
        // Deep copy items
        copy->items.reserve(items.size());
        for (const auto& item : items) {
            if (item) {
                copy->items.push_back(std::shared_ptr<PlatformMenuItem>(item->clone().release()));
            }
        }
        return copy;
    }

    PlatformMenuItemRef PlatformMenuItemSubmenu::create(std::string label)
    {
        return std::make_shared<PlatformMenuItemSubmenu>(std::move(label));
    }

    PlatformMenuItemRef PlatformMenuItemSubmenu::create(std::string label, std::vector<PlatformMenuItemRef> items)
    {
        return std::make_shared<PlatformMenuItemSubmenu>(std::move(label), std::move(items));
    }

    // -------------------------------------------------------------------------
    // PlatformProvidedMenuItem
    // -------------------------------------------------------------------------

    std::unique_ptr<PlatformMenuItem> PlatformProvidedMenuItem::clone() const
    {
        auto copy = std::make_unique<PlatformProvidedMenuItem>(type);
        copy->enabled = enabled;
        return copy;
    }

    // -------------------------------------------------------------------------
    // PlatformMenu
    // -------------------------------------------------------------------------

    PlatformMenuRef PlatformMenu::create(std::string label)
    {
        return std::make_shared<PlatformMenu>(std::move(label));
    }

    PlatformMenuRef PlatformMenu::create(std::string label, std::vector<PlatformMenuItemRef> items)
    {
        return std::make_shared<PlatformMenu>(std::move(label), std::move(items));
    }

} // namespace systems::leal::campello_widgets
