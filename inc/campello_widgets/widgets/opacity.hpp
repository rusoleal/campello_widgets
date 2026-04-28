#pragma once

#include <campello_widgets/diagnostics/diagnostic_property.hpp>
#include <campello_widgets/diagnostics/debug_assert.hpp>
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
            CW_ASSERT_MSG(op >= 0.0f && op <= 1.0f, "Opacity.opacity must be in [0.0, 1.0]");
            opacity = op;
            child   = std::move(c);
        }

        
        void debugValidate() const override
        {
            CW_ASSERT_MSG(opacity >= 0.0f && opacity <= 1.0f, "Opacity.opacity must be in [0.0, 1.0]");

        }
        std::shared_ptr<RenderObject> createRenderObject() const override;
        void updateRenderObject(RenderObject& ro) const override;
        void debugFillProperties(DiagnosticsPropertyBuilder& properties) const override;

    };

} // namespace systems::leal::campello_widgets
