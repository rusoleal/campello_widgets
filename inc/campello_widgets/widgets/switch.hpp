#pragma once

#include <campello_widgets/widgets/stateful_widget.hpp>
#include <campello_widgets/ui/color.hpp>

#include <functional>

namespace systems::leal::campello_widgets
{

    /**
     * @brief An on/off toggle with an animated sliding thumb.
     *
     * Switch is controlled: supply `value` and `on_changed` from the parent
     * state. Set `on_changed` to nullptr to disable.
     *
     * @code
     * auto sw = std::make_shared<Switch>();
     * sw->value      = is_on;
     * sw->on_changed = [this](bool v) { setState([&]{ is_on = v; }); };
     * @endcode
     */
    class Switch : public StatefulWidget
    {
    public:
        bool                      value      = false;
        std::function<void(bool)> on_changed;

        Color active_track_color   = Color::fromRGBA(0.098f, 0.463f, 0.824f, 0.5f);
        Color inactive_track_color = Color::fromRGBA(0.0f, 0.0f, 0.0f, 0.26f);
        Color active_thumb_color   = Color::fromRGBA(0.098f, 0.463f, 0.824f, 1.0f);
        Color inactive_thumb_color = Color::white();

        float width  = 44.0f;
        float height = 24.0f;

        std::unique_ptr<StateBase> createState() const override;
    };

} // namespace systems::leal::campello_widgets
