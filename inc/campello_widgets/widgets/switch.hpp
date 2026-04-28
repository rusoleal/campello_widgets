#pragma once

#include <campello_widgets/widgets/stateful_widget.hpp>
#include <campello_widgets/ui/color.hpp>

#include <functional>
#include <optional>
#include <campello_widgets/diagnostics/debug_assert.hpp>

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

        std::optional<Color> active_track_color;
        std::optional<Color> inactive_track_color;
        std::optional<Color> active_thumb_color;
        std::optional<Color> inactive_thumb_color;

        float width  = 44.0f;
        float height = 24.0f;

        Switch() = default;
        explicit Switch(bool val, std::function<void(bool)> on_change = nullptr)
            : value(val), on_changed(std::move(on_change))
        {
            CW_ASSERT_MSG(width > 0.0f, "Switch.width must be greater than 0");
            CW_ASSERT_MSG(height > 0.0f, "Switch.height must be greater than 0");
        }

        std::unique_ptr<StateBase> createState() const override;
        void debugValidate() const override
        {
            CW_ASSERT_MSG(width > 0.0f, "Switch.width must be greater than 0");
            CW_ASSERT_MSG(height > 0.0f, "Switch.height must be greater than 0");

        }

    };

} // namespace systems::leal::campello_widgets
