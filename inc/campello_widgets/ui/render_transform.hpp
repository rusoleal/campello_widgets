#pragma once

#include <campello_widgets/ui/render_box.hpp>
#include <campello_widgets/ui/alignment.hpp>
#include <vector_math/matrix4.hpp>

namespace systems::leal::campello_widgets
{

    /**
     * @brief A RenderBox that applies a Matrix4 transform to its child.
     *
     * Mirrors Flutter's Transform widget. The transform is applied relative to
     * the pivot point specified by `alignment` (default: Alignment::center()).
     *
     * Layout is transparent: the box reports the child's unconstrained size and
     * passes the full incoming constraints down. The transform only affects
     * painting, not layout geometry.
     *
     * Use the static helpers to build common matrices:
     * @code
     *   auto t = std::make_shared<RenderTransform>();
     *   t->transform = RenderTransform::rotation(0.3f);         // 0.3 rad CW
     *   t->transform = RenderTransform::scaling(2.0f);          // 2× uniform
     *   t->transform = RenderTransform::translation(100, 50);   // translate
     *   t->alignment = Alignment::center();                      // pivot
     * @endcode
     */
    class RenderTransform : public RenderBox
    {
    public:
        using Matrix4 = systems::leal::vector_math::Matrix4<float>;

        /** The transformation to apply. Default: identity (no-op). */
        Matrix4   transform = Matrix4::identity();

        /**
         * @brief Pivot for the transform, expressed as an alignment within the
         * child's bounding box.
         *
         * Default: Alignment::center() — matches Flutter's Transform.rotate and
         * Transform.scale factories.
         *
         * Set to Alignment::topLeft() for Transform.translate behaviour
         * (origin at top-left, no pivot adjustment needed).
         */
        Alignment alignment = Alignment::center();

        // ----------------------------------------------------------------
        // Matrix helpers — return a Matrix4 to assign to `transform`.
        // ----------------------------------------------------------------

        /** @brief Clockwise rotation matrix for the given angle in radians. */
        static Matrix4 rotation(float radians);

        /** @brief Uniform scale matrix. */
        static Matrix4 scaling(float s);

        /** @brief Non-uniform scale matrix. */
        static Matrix4 scaling(float sx, float sy);

        /**
         * @brief Pure translation matrix.
         *
         * When using a translate-only transform, set `alignment` to
         * Alignment::topLeft() so no pivot offset is applied.
         */
        static Matrix4 translation(float dx, float dy);

        // ----------------------------------------------------------------
        // RenderBox interface
        // ----------------------------------------------------------------

        void performLayout() override;
        void performPaint(PaintContext& context, const Offset& offset) override;
    };

} // namespace systems::leal::campello_widgets
