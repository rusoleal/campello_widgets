#pragma once

#include <campello_widgets/widgets/stateful_widget.hpp>
#include <campello_widgets/ui/alignment.hpp>
#include <campello_widgets/ui/curves.hpp>

#include <functional>
#include <optional>

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

        std::unique_ptr<StateBase> createState() const override;
    };

} // namespace systems::leal::campello_widgets
