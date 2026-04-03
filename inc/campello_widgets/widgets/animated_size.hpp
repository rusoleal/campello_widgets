#pragma once

#include <campello_widgets/widgets/single_child_render_object_widget.hpp>
#include <campello_widgets/ui/alignment.hpp>
#include <campello_widgets/ui/curves.hpp>
#include <campello_widgets/ui/render_animated_size.hpp>

#include <functional>

namespace systems::leal::campello_widgets
{

    /**
     * @brief Animates its own size when its child's size changes.
     *
     * On the first layout the widget snaps to the child's natural size.
     * On subsequent layouts, if the child's size has changed, the widget
     * smoothly interpolates from the old size to the new one over
     * `duration_ms` using `curve`.
     *
     * @code
     * auto w = std::make_shared<AnimatedSize>();
     * w->duration_ms = 250.0;
     * w->curve       = Curves::easeInOut;
     * w->child       = expanded ? bigWidget : smallWidget;
     * @endcode
     */
    class AnimatedSize : public SingleChildRenderObjectWidget
    {
    public:
        double                        duration_ms = 200.0;
        std::function<double(double)> curve       = Curves::easeInOut;
        Alignment                     alignment   = Alignment::center();

        AnimatedSize() = default;
        explicit AnimatedSize(WidgetRef c) { child = std::move(c); }
        explicit AnimatedSize(double duration, WidgetRef c = nullptr)
            : duration_ms(duration)
        {
            child = std::move(c);
        }

        std::shared_ptr<RenderObject> createRenderObject() const override
        {
            return std::make_shared<RenderAnimatedSize>(
                duration_ms,
                curve ? curve : Curves::easeInOut,
                alignment);
        }

        void updateRenderObject(RenderObject& ro) const override
        {
            auto& r = static_cast<RenderAnimatedSize&>(ro);
            r.setDuration(duration_ms);
            r.setCurve(curve ? curve : Curves::easeInOut);
            r.setAlignment(alignment);
        }
    };

} // namespace systems::leal::campello_widgets
