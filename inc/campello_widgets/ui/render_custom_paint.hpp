#pragma once

#include <memory>
#include <campello_widgets/ui/render_box.hpp>
#include <campello_widgets/ui/custom_painter.hpp>

namespace systems::leal::campello_widgets
{

    /**
     * @brief RenderBox that delegates painting to a CustomPainter.
     *
     * Fills the available constraints and calls `painter->paint()` each
     * frame (unless `shouldRepaint()` returns false).
     */
    class RenderCustomPaint : public RenderBox
    {
    public:
        void setPainter(std::shared_ptr<CustomPainter> painter) noexcept;
        CustomPainter* painter() const noexcept { return painter_.get(); }

        void performLayout() override;
        void performPaint(PaintContext& context, const Offset& offset) override;

    private:
        std::shared_ptr<CustomPainter> painter_;
        std::shared_ptr<CustomPainter> prev_painter_; ///< Used for shouldRepaint check.
    };

} // namespace systems::leal::campello_widgets
