#pragma once

#include <campello_widgets/widgets/stateful_widget.hpp>
#include <campello_widgets/ui/color.hpp>
#include <campello_widgets/ui/curves.hpp>

#include <functional>
#include <optional>

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

        Color  background_color = Color::fromRGBA(0.098f, 0.463f, 0.824f, 0.24f);
        Color  value_color      = Color::fromRGBA(0.098f, 0.463f, 0.824f, 1.0f);
        float  min_height       = 4.0f;
        double duration_ms      = 1600.0;   ///< Indeterminate animation period

        LinearProgressIndicator() = default;
        explicit LinearProgressIndicator(float val)
            : value(val)
        {}
        explicit LinearProgressIndicator(float val, Color val_color)
            : value(val), value_color(val_color)
        {}

        std::unique_ptr<StateBase> createState() const override;
    };

} // namespace systems::leal::campello_widgets
