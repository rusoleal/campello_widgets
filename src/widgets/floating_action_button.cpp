#include <campello_widgets/widgets/floating_action_button.hpp>
#include <campello_widgets/widgets/gesture_detector.hpp>
#include <campello_widgets/widgets/decorated_box.hpp>
#include <campello_widgets/widgets/sized_box.hpp>
#include <campello_widgets/widgets/center.hpp>
#include <campello_widgets/widgets/opacity.hpp>
#include <campello_widgets/ui/box_decoration.hpp>
#include <campello_widgets/ui/box_shadow.hpp>

namespace systems::leal::campello_widgets
{

    WidgetRef FloatingActionButton::build(BuildContext&) const
    {
        const float diameter = mini ? 40.0f : 56.0f;

        BoxDecoration deco;
        deco.color         = background_color;
        deco.border_radius = diameter / 2.0f;   // circular
        if (elevation > 0.0f) {
            deco.box_shadow = {BoxShadow{
                Color::fromRGBA(0.0f, 0.0f, 0.0f, 0.3f),
                Offset{0.0f, elevation * 0.5f},
                elevation * 2.0f
            }};
        }

        auto sized     = SizedBox::create(diameter, diameter);
        sized->child   = Center::create(child);

        auto decorated        = std::make_shared<DecoratedBox>();
        decorated->decoration = deco;
        decorated->child      = sized;

        auto gesture         = std::make_shared<GestureDetector>();
        gesture->on_tap      = on_pressed;
        gesture->child       = decorated;

        if (!on_pressed) {
            auto faded     = std::make_shared<Opacity>();
            faded->opacity = 0.38f;
            faded->child   = gesture;
            return faded;
        }

        return gesture;
    }

} // namespace systems::leal::campello_widgets
