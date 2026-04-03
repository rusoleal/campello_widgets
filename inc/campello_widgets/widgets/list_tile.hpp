#pragma once

#include <campello_widgets/widgets/stateless_widget.hpp>
#include <campello_widgets/ui/color.hpp>
#include <campello_widgets/ui/edge_insets.hpp>

#include <functional>

namespace systems::leal::campello_widgets
{

    /**
     * @brief A single fixed-height row for use in lists.
     *
     * ListTile composes a Row with up to four slots:
     *  - leading  : widget at the start (e.g. icon or avatar)
     *  - title    : primary text or widget
     *  - subtitle : secondary text below the title (optional)
     *  - trailing : widget at the end (e.g. switch or icon)
     *
     * Tap / long-press handlers wrap the tile in a GestureDetector.
     * When disabled the tile is rendered at 38% opacity.
     *
     * @code
     * auto tile = std::make_shared<ListTile>();
     * tile->title    = std::make_shared<Text>("Settings");
     * tile->trailing = std::make_shared<Text>(">");
     * tile->on_tap   = []{ };
     * @endcode
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
        bool                  enabled          = true;
        bool                  dense            = false;
        Color                 background_color = Color::transparent();
        EdgeInsets            content_padding  = EdgeInsets::symmetric(16.0f, 0.0f);

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
