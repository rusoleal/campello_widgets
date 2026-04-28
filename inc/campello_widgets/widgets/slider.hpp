#pragma once

#include <campello_widgets/widgets/stateful_widget.hpp>
#include <campello_widgets/ui/color.hpp>

#include <functional>
#include <optional>
#include <campello_widgets/diagnostics/debug_assert.hpp>

namespace systems::leal::campello_widgets
{

    /**
     * @brief A horizontal slider for selecting a value within a range.
     *
     * Slider is controlled: `value` must be within [`min`, `max`] and
     * changes are reported via `on_changed`. Set `on_changed` to nullptr
     * to disable interaction.
     *
     * @code
     * auto sl = std::make_shared<Slider>();
     * sl->value      = current_vol;
     * sl->min        = 0.0f;
     * sl->max        = 100.0f;
     * sl->on_changed = [this](float v) { setState([&]{ current_vol = v; }); };
     * @endcode
     */
    class Slider : public StatefulWidget
    {
    public:
        float value = 0.0f;
        float min   = 0.0f;
        float max   = 1.0f;

        std::function<void(float)> on_changed; ///< Called with value in [min, max]

        std::optional<Color> active_color;
        std::optional<Color> inactive_color;

        float height       = 36.0f; ///< Total hit-area height
        float track_height = 4.0f;
        float thumb_radius = 10.0f;

        Slider() = default;
        explicit Slider(float val, std::function<void(float)> on_change = nullptr)
            : value(val), on_changed(std::move(on_change))
        {
            CW_ASSERT_MSG(val >= min && val <= max, "Slider.value must be within [min, max]");
        }
        explicit Slider(
            float val,
            float min_val,
            float max_val,
            std::function<void(float)> on_change = nullptr)
            : value(val), min(min_val), max(max_val), on_changed(std::move(on_change))
        {
            CW_ASSERT_MSG(min_val < max_val, "Slider.min must be less than Slider.max");
            CW_ASSERT_MSG(val >= min_val && val <= max_val, "Slider.value must be within [min, max]");
        }

        std::unique_ptr<StateBase> createState() const override;
        void debugValidate() const override
        {
            CW_ASSERT_MSG(min < max, "Slider.min must be less than Slider.max");
            CW_ASSERT_MSG(value >= min && value <= max, "Slider.value must be within [min, max]");
            CW_ASSERT_MSG(height > 0.0f, "Slider.height must be greater than 0");
            CW_ASSERT_MSG(track_height > 0.0f, "Slider.track_height must be greater than 0");
            CW_ASSERT_MSG(thumb_radius > 0.0f, "Slider.thumb_radius must be greater than 0");
        }

    };

} // namespace systems::leal::campello_widgets
