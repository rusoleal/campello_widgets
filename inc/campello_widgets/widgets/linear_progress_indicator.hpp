#pragma once

#include <campello_widgets/widgets/stateful_widget.hpp>
#include <campello_widgets/ui/color.hpp>
#include <campello_widgets/ui/curves.hpp>

#include <functional>
#include <optional>
#include <campello_widgets/diagnostics/debug_assert.hpp>

namespace systems::leal::campello_widgets
{

    /**
     * @brief A horizontal bar that indicates progress.
     *
     * When `value` is set the bar fills proportionally (0 = empty, 1 = full).
     * When `value` is `std::nullopt` the indicator animates continuously
     * to signal indeterminate progress.
     *
     * @code
     * // Determinate
     * auto p = std::make_shared<LinearProgressIndicator>();
     * p->value = 0.6f;   // 60 % filled
     *
     * // Indeterminate
     * p->value = std::nullopt;
     * @endcode
     */
    class LinearProgressIndicator : public StatefulWidget
    {
    public:
        std::optional<float> value;

        std::optional<Color> background_color;
        std::optional<Color> value_color;
        float  min_height       = 4.0f;
        double duration_ms      = 1600.0;   ///< Indeterminate animation period

        LinearProgressIndicator() = default;
        explicit LinearProgressIndicator(float val)
            : value(val)
        {
            CW_ASSERT_MSG(val >= 0.0f && val <= 1.0f, "LinearProgressIndicator.value must be in [0.0, 1.0]");
}
        explicit LinearProgressIndicator(float val, Color val_color)
            : value(val), value_color(val_color)
        {
            CW_ASSERT_MSG(val >= 0.0f && val <= 1.0f, "LinearProgressIndicator.value must be in [0.0, 1.0]");
}

        std::unique_ptr<StateBase> createState() const override;
        void debugValidate() const override
        {
            if (value.has_value())
                CW_ASSERT_MSG(*value >= 0.0f && *value <= 1.0f, "LinearProgressIndicator.value must be in [0.0, 1.0]");
            CW_ASSERT_MSG(min_height >= 0.0f, "LinearProgressIndicator.min_height must be non-negative");
            CW_ASSERT_MSG(duration_ms >= 0.0, "LinearProgressIndicator.duration_ms must be non-negative");

        }

    };

} // namespace systems::leal::campello_widgets
