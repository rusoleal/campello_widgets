#pragma once

#include <campello_widgets/widgets/stateless_widget.hpp>
#include <campello_widgets/ui/design_system.hpp>

#include <functional>

namespace systems::leal::campello_widgets
{

    /**
     * @brief An adaptive list tile that delegates its visual appearance to the
     *        active DesignSystem.
     */
    class ListTile : public StatelessWidget
    {
    public:
        WidgetRef             leading;
        WidgetRef             title;
        WidgetRef             subtitle;
        WidgetRef             trailing;
        std::function<void()> on_tap;
        std::function<void()> on_long_press;
        bool                  enabled = true;

        ListTile() = default;
        explicit ListTile(WidgetRef tile_title)
            : title(std::move(tile_title))
        {}
        explicit ListTile(WidgetRef tile_title, WidgetRef tile_trailing)
            : title(std::move(tile_title)), trailing(std::move(tile_trailing))
        {}
        explicit ListTile(
            WidgetRef tile_leading,
            WidgetRef tile_title,
            WidgetRef tile_trailing)
            : leading(std::move(tile_leading))
            , title(std::move(tile_title))
            , trailing(std::move(tile_trailing))
        {}
        explicit ListTile(
            WidgetRef tile_leading,
            WidgetRef tile_title,
            WidgetRef tile_subtitle,
            WidgetRef tile_trailing)
            : leading(std::move(tile_leading))
            , title(std::move(tile_title))
            , subtitle(std::move(tile_subtitle))
            , trailing(std::move(tile_trailing))
        {}

        WidgetRef build(BuildContext& ctx) const override;
    };

} // namespace systems::leal::campello_widgets
