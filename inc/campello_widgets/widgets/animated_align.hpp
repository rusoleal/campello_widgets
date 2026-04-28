#pragma once

#include <campello_widgets/widgets/stateful_widget.hpp>
#include <campello_widgets/ui/alignment.hpp>
#include <campello_widgets/ui/curves.hpp>

#include <functional>
#include <optional>
#include <campello_widgets/diagnostics/debug_assert.hpp>

namespace systems::leal::campello_widgets
{

    /**
     * @brief Implicitly animates the alignment of a child whenever `alignment`
     * changes across widget updates.
     *
     * @code
     * auto w = std::make_shared<AnimatedAlign>();
     * w->alignment   = centered ? Alignment::center() : Alignment::topLeft();
     * w->duration_ms = 300.0;
     * w->curve       = Curves::easeInOut;
     * w->child       = someChild;
     * @endcode
     */
    class AnimatedAlign : public StatefulWidget
    {
    public:
        Alignment                     alignment    = Alignment::center();
        double                        duration_ms  = 300.0;
        std::function<double(double)> curve        = Curves::easeInOut;
        std::optional<float>          width_factor;
        std::optional<float>          height_factor;
        WidgetRef                     child;

        AnimatedAlign() = default;
        explicit AnimatedAlign(Alignment align, WidgetRef c = nullptr)
            : alignment(align), child(std::move(c))
        {
            CW_ASSERT_MSG(duration_ms >= 0.0, "AnimatedAlign.duration_ms must be non-negative");
}
        explicit AnimatedAlign(
            Alignment align,
            double duration,
            WidgetRef c = nullptr)
            : alignment(align), duration_ms(duration), child(std::move(c))
        {
            CW_ASSERT_MSG(duration >= 0.0, "AnimatedAlign.duration_ms must be non-negative");
}

        std::unique_ptr<StateBase> createState() const override;
        void debugValidate() const override
        {
            CW_ASSERT_MSG(duration_ms >= 0.0, "AnimatedAlign.duration_ms must be non-negative");
            if (width_factor.has_value())
                CW_ASSERT_MSG(*width_factor >= 0.0f, "AnimatedAlign.width_factor must be non-negative");
            if (height_factor.has_value())
                CW_ASSERT_MSG(*height_factor >= 0.0f, "AnimatedAlign.height_factor must be non-negative");

        }

    };

} // namespace systems::leal::campello_widgets
