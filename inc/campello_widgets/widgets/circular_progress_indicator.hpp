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

        Color background_color = Color::transparent();
        Color value_color      = Color::fromRGBA(0.098f, 0.463f, 0.824f, 1.0f);
        float stroke_width     = 4.0f;
        float size             = 36.0f;   ///< Diameter in logical pixels
        double duration_ms     = 1200.0;  ///< Indeterminate rotation period

        std::unique_ptr<StateBase> createState() const override;
    };

} // namespace systems::leal::campello_widgets
