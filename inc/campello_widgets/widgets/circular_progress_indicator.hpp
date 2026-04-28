#pragma once

#include <campello_widgets/widgets/stateful_widget.hpp>
#include <campello_widgets/ui/color.hpp>

#include <optional>
#include <campello_widgets/diagnostics/debug_assert.hpp>

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
        {
            CW_ASSERT_MSG(val >= 0.0f && val <= 1.0f, "CircularProgressIndicator.value must be in [0.0, 1.0]");
}
        explicit CircularProgressIndicator(float val, Color val_color)
            : value(val), value_color(val_color)
        {
            CW_ASSERT_MSG(val >= 0.0f && val <= 1.0f, "CircularProgressIndicator.value must be in [0.0, 1.0]");
}

        std::unique_ptr<StateBase> createState() const override;
        void debugValidate() const override
        {
            if (value.has_value())
                CW_ASSERT_MSG(*value >= 0.0f && *value <= 1.0f, "CircularProgressIndicator.value must be in [0.0, 1.0]");
            CW_ASSERT_MSG(stroke_width >= 0.0f, "CircularProgressIndicator.stroke_width must be non-negative");
            CW_ASSERT_MSG(size >= 0.0f, "CircularProgressIndicator.size must be non-negative");
            CW_ASSERT_MSG(duration_ms >= 0.0, "CircularProgressIndicator.duration_ms must be non-negative");

        }

    };

} // namespace systems::leal::campello_widgets
