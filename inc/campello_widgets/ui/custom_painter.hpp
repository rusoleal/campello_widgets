#pragma once

#include <campello_widgets/ui/size.hpp>
#include <campello_widgets/ui/canvas.hpp>

namespace systems::leal::campello_widgets
{

    /**
     * @brief Interface for custom painting via RawCustomPaint.
     *
     * Implement this interface and pass it to `RawCustomPaint` to issue
     * arbitrary draw commands directly onto the Canvas.
     * The `shouldRepaint()` method lets the framework skip repaints when
     * the painter's inputs have not changed.
     *
     * **Usage:**
     * @code
     * class MyPainter : public CustomPainter {
     * public:
     *     void paint(Canvas& canvas, Size size) override {
     *         canvas.drawRect(Rect::fromLTWH(0, 0, size.width, size.height),
     *                         Paint::filled(Color::blue()));
     *     }
     *     bool shouldRepaint(const CustomPainter&) const override { return false; }
     * };
     * @endcode
     */
    class CustomPainter
    {
    public:
        virtual ~CustomPainter() = default;

        /**
         * @brief Called once per frame to paint onto `canvas`.
         *
         * @param canvas  The canvas to draw into.
         * @param size    The size of the area to paint into (matches the
         *                RenderCustomPaint's layout size).
         */
        virtual void paint(Canvas& canvas, Size size) = 0;

        /**
         * @brief Returns true if this painter's output differs from `old_painter`.
         *
         * When false, the framework may skip calling `paint()` and reuse the
         * previous frame's output.
         *
         * @param old_painter The painter used in the previous frame.
         */
        virtual bool shouldRepaint(const CustomPainter& old_painter) const = 0;
    };

} // namespace systems::leal::campello_widgets
