#pragma once

#include <campello_widgets/widgets/stateful_widget.hpp>
#include <campello_widgets/ui/color.hpp>

#include <optional>

namespace systems::leal::campello_widgets
{

    /**
     * @brief A circular spinning indicator that shows progress or activity.
     *
     * When `value` is set (0–1) a fixed arc is drawn proportionally.
     * When `value` is `std::nullopt` a 270° arc rotates continuously.
     *
     * @code
     * // Indeterminate spinner
     * auto spinner = std::make_shared<CircularProgressIndicator>();
     *
     * // Determinate – 75 %
     * auto p = std::make_shared<CircularProgressIndicator>();
     * p->value       = 0.75f;
     * p->value_color = Color::green();
     * @endcode
     */
    class CircularProgressIndicator : public StatefulWidget
    {
    public:
        std::optional<float> value;

        std::optional<Color> background_color;
        std::optional<Color> value_color;
        float stroke_width     = 4.0f;
        float size             = 36.0f;   ///< Diameter in logical pixels
        double duration_ms     = 1200.0;  ///< Indeterminate rotation period

        CircularProgressIndicator() = default;
        explicit CircularProgressIndicator(float val)
            : value(val)
        {}
        explicit CircularProgressIndicator(float val, Color val_color)
            : value(val), value_color(val_color)
        {}

        std::unique_ptr<StateBase> createState() const override;
    };

} // namespace systems::leal::campello_widgets
