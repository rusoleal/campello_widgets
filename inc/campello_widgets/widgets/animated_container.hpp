#pragma once

#include <campello_widgets/widgets/stateful_widget.hpp>
#include <campello_widgets/ui/color.hpp>
#include <campello_widgets/ui/edge_insets.hpp>
#include <campello_widgets/ui/alignment.hpp>
#include <campello_widgets/ui/curves.hpp>

#include <functional>
#include <optional>
#include <memory>

namespace systems::leal::campello_widgets
{

    /**
     * @brief A Container whose properties animate smoothly when they change.
     *
     * When a new AnimatedContainer is mounted in place of an existing one (same
     * position in the widget tree) and its properties differ, the state
     * interpolates from the old values to the new ones over `duration_ms`.
     *
     * Animatable properties: `color`, `width`, `height`, `padding`.
     * Non-animatable: `alignment`, `child` (applied immediately).
     *
     * @code
     * auto w = std::make_shared<AnimatedContainer>();
     * w->color       = expanded ? Color::blue() : Color::red();
     * w->width       = expanded ? 200.f : 100.f;
     * w->duration_ms = 300.0;
     * w->curve       = Curves::easeInOut;
     * @endcode
     */
    class AnimatedContainer : public StatefulWidget
    {
    public:
        std::optional<float>      width;
        std::optional<float>      height;
        std::optional<Color>      color;
        std::optional<EdgeInsets> padding;
        std::optional<Alignment>  alignment;
        WidgetRef                 child;

        double                          duration_ms = 300.0;
        std::function<double(double)>   curve       = Curves::easeInOut;

        std::unique_ptr<StateBase> createState() const override;
    };

} // namespace systems::leal::campello_widgets
