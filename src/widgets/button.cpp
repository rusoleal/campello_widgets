#include <campello_widgets/widgets/button.hpp>
#include <campello_widgets/widgets/gesture_detector.hpp>
#include <campello_widgets/widgets/opacity.hpp>
#include <campello_widgets/widgets/padding.hpp>
#include <campello_widgets/widgets/decorated_box.hpp>
#include <campello_widgets/ui/box_decoration.hpp>
#include <campello_widgets/ui/box_shadow.hpp>

namespace systems::leal::campello_widgets
{

    WidgetRef Button::build(BuildContext&) const
    {
        // Content: padding + child
        auto padded      = std::make_shared<Padding>();
        padded->padding  = padding;
        padded->child    = child;

        // Decoration: rounded rect + background + optional elevation shadow
        BoxDecoration deco;
        deco.color         = background_color;
        deco.border_radius = border_radius;
        if (elevation > 0.0f) {
            deco.box_shadow = {
                BoxShadow{
                    Color::fromRGBA(0.0f, 0.0f, 0.0f, 0.3f),
                    Offset{0.0f, elevation},
                    elevation * 2.0f
                }
            };
        }

        auto decorated        = std::make_shared<DecoratedBox>();
        decorated->decoration = deco;
        decorated->child      = padded;

        // Tap handler
        auto detector         = std::make_shared<GestureDetector>();
        detector->on_tap      = on_pressed;
        detector->child       = decorated;

        // Disabled state: reduce opacity
        if (!on_pressed) {
            auto faded     = std::make_shared<Opacity>();
            faded->opacity = 0.38f;
            faded->child   = detector;
            return faded;
        }

        return detector;
    }

} // namespace systems::leal::campello_widgets
