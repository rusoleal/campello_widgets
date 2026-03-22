#pragma once

#include <campello_widgets/widgets/single_child_render_object_widget.hpp>

namespace systems::leal::campello_widgets
{

    /**
     * @brief A widget that paints its child at the given opacity.
     *
     * Wraps a `RenderOpacity` render object. For animated opacity transitions
     * use `AnimatedOpacity` instead.
     *
     * @code
     * auto w = std::make_shared<Opacity>();
     * w->opacity = 0.5f;
     * w->child   = someChildWidget;
     * @endcode
     */
    class Opacity : public SingleChildRenderObjectWidget
    {
    public:
        float opacity = 1.0f;

        Opacity() = default;
        explicit Opacity(float op, WidgetRef c = nullptr)
        {
            opacity = op;
            child   = std::move(c);
        }

        std::shared_ptr<RenderObject> createRenderObject() const override;
        void updateRenderObject(RenderObject& ro) const override;
    };

} // namespace systems::leal::campello_widgets
