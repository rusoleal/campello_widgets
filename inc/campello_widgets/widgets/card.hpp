#pragma once

#include <campello_widgets/widgets/stateless_widget.hpp>
#include <campello_widgets/ui/color.hpp>
#include <campello_widgets/ui/edge_insets.hpp>

namespace systems::leal::campello_widgets
{

    /**
     * @brief A material design card — an elevated surface with rounded corners.
     *
     * Card wraps its child in a DecoratedBox with a background color, rounded
     * corners, and an optional elevation shadow. An outer margin separates the
     * card from neighboring widgets.
     *
     * @code
     * auto card = std::make_shared<Card>();
     * card->child     = std::make_shared<Text>("Hello");
     * card->elevation = 2.0f;
     * @endcode
     */
    class Card : public StatelessWidget
    {
    public:
        WidgetRef   child;
        Color       color          = Color::white();
        float       border_radius  = 4.0f;
        float       elevation      = 1.0f;
        EdgeInsets  margin         = EdgeInsets::all(4.0f);

        Card() = default;
        explicit Card(WidgetRef c) : child(std::move(c)) {}

        WidgetRef build(BuildContext& ctx) const override;

        static std::shared_ptr<Card> create(WidgetRef child)
        {
            return std::make_shared<Card>(std::move(child));
        }
    };

} // namespace systems::leal::campello_widgets
