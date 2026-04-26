#pragma once

#include <campello_widgets/widgets/stateless_widget.hpp>
#include <campello_widgets/ui/design_system.hpp>
#include <campello_widgets/ui/edge_insets.hpp>

namespace systems::leal::campello_widgets
{

    /**
     * @brief An adaptive card that delegates its visual appearance to the
     *        active DesignSystem.
     *
     * The card's look (colors, radius, elevation, outline) is decided by the
     * current theme's DesignSystem based on the semantic `priority` you supply.
     *
     * @code
     * auto card = std::make_shared<Card>(
     *     std::make_shared<Text>("Hello"));
     * card->priority = CardPriority::elevated;
     * @endcode
     */
    class Card : public StatelessWidget
    {
    public:
        WidgetRef    child;
        CardPriority priority = CardPriority::elevated;
        EdgeInsets   padding  = EdgeInsets::all(16.0f);

        Card() = default;
        explicit Card(WidgetRef c) : child(std::move(c)) {}

        WidgetRef build(BuildContext& ctx) const override;

        static std::shared_ptr<Card> create(WidgetRef child)
        {
            return std::make_shared<Card>(std::move(child));
        }
    };

} // namespace systems::leal::campello_widgets
