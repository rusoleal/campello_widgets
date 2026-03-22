#pragma once

#include <memory>
#include <campello_widgets/widgets/render_object_widget.hpp>
#include <campello_widgets/ui/custom_painter.hpp>

namespace systems::leal::campello_widgets
{

    /**
     * @brief A widget that provides a CustomPainter with a canvas to draw on.
     *
     * Use RawCustomPaint when the built-in widgets cannot express your desired
     * visual. The painter receives a `PaintContext` and the painted `Size`
     * each frame and can issue any draw commands directly.
     *
     * The widget fills the available constraints. Wrap it in a `SizedBox`
     * (Phase 6) to give it an explicit size.
     *
     * **Usage:**
     * @code
     * auto canvas = RawCustomPaint::create(std::make_shared<MyPainter>());
     * @endcode
     */
    class RawCustomPaint : public RenderObjectWidget
    {
    public:
        std::shared_ptr<CustomPainter> painter;

        static std::shared_ptr<RawCustomPaint> create(
            std::shared_ptr<CustomPainter> painter)
        {
            auto w    = std::make_shared<RawCustomPaint>();
            w->painter = std::move(painter);
            return w;
        }

        std::shared_ptr<RenderObject> createRenderObject() const override;
        void updateRenderObject(RenderObject& render_object) const override;
    };

} // namespace systems::leal::campello_widgets
