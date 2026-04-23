#pragma once

#include <campello_widgets/diagnostics/diagnostic_property.hpp>
#include <campello_widgets/widgets/single_child_render_object_widget.hpp>
#include <campello_widgets/ui/alignment.hpp>
#include <vector_math/matrix4.hpp>

namespace systems::leal::campello_widgets
{

    using Matrix4 = systems::leal::vector_math::Matrix4<float>;

    /**
     * @brief A widget that applies a transformation matrix to its child.
     *
     * Mirrors Flutter's Transform widget. The transform is applied relative to
     * the pivot point specified by `alignment` (default: Alignment::center()).
     *
     * Layout is transparent: the box reports the child's unconstrained size and
     * passes the full incoming constraints down. The transform only affects
     * painting, not layout geometry.
     *
     * @code
     * // Rotate by 0.5 radians around the center
     * auto t = std::make_shared<Transform>();
     * t->transform = Transform::rotation(0.5f);
     * t->child = someChildWidget;
     *
     * // Scale by 2x around the center
     * auto t2 = std::make_shared<Transform>();
     * t2->transform = Transform::scaling(2.0f);
     * t2->child = someChildWidget;
     *
     * // Translate by (100, 50) - note the alignment change
     * auto t3 = std::make_shared<Transform>();
     * t3->transform = Transform::translation(100, 50);
     * t3->alignment = Alignment::topLeft();  // No pivot adjustment for translate
     * t3->child = someChildWidget;
     * @endcode
     */
    class Transform : public SingleChildRenderObjectWidget
    {
    public:
        /** The transformation matrix to apply. Default: identity (no-op). */
        Matrix4 transform = Matrix4::identity();

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

        Transform() = default;

        /**
         * @brief Constructs a Transform with the given matrix, alignment, and child.
         *
         * @param t The transformation matrix.
         * @param a The alignment (pivot point) for the transform. Default: center.
         * @param c The child widget. Default: nullptr.
         */
        explicit Transform(Matrix4 t, Alignment a = Alignment::center(), WidgetRef c = nullptr)
            : transform(std::move(t))
            , alignment(a)
        {
            child = std::move(c);
        }

        std::shared_ptr<RenderObject> createRenderObject() const override;
        void updateRenderObject(RenderObject& ro) const override;

        // ----------------------------------------------------------------
        // Matrix helpers — return a Matrix4 for convenience.
        // These mirror the static helpers on RenderTransform.
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
        // Factory methods for common use cases
        // ----------------------------------------------------------------

        /**
         * @brief Creates a rotation Transform widget.
         *
         * @param radians Angle in radians (clockwise).
         * @param child Optional child widget.
         * @return A shared pointer to the Transform widget.
         */
        static std::shared_ptr<Transform> rotate(float radians, WidgetRef child = nullptr)
        {
            auto t = std::make_shared<Transform>();
            t->transform = rotation(radians);
            t->alignment = Alignment::center();
            t->child = std::move(child);
            return t;
        }

        /**
         * @brief Creates a scale Transform widget.
         *
         * @param scale Uniform scale factor.
         * @param child Optional child widget.
         * @return A shared pointer to the Transform widget.
         */
        static std::shared_ptr<Transform> scale(float scale, WidgetRef child = nullptr)
        {
            auto t = std::make_shared<Transform>();
            t->transform = scaling(scale);
            t->alignment = Alignment::center();
            t->child = std::move(child);
            return t;
        }

        /**
         * @brief Creates a non-uniform scale Transform widget.
         *
         * @param sx X-axis scale factor.
         * @param sy Y-axis scale factor.
         * @param child Optional child widget.
         * @return A shared pointer to the Transform widget.
         */
        static std::shared_ptr<Transform> scale(float sx, float sy, WidgetRef child = nullptr)
        {
            auto t = std::make_shared<Transform>();
            t->transform = scaling(sx, sy);
            t->alignment = Alignment::center();
            t->child = std::move(child);
            return t;
        }

        /**
         * @brief Creates a translation Transform widget.
         *
         * Note: The alignment is set to topLeft to avoid pivot adjustment.
         *
         * @param dx X-axis translation.
         * @param dy Y-axis translation.
         * @param child Optional child widget.
         * @return A shared pointer to the Transform widget.
         */
        static std::shared_ptr<Transform> translate(float dx, float dy, WidgetRef child = nullptr)
        {
            auto t = std::make_shared<Transform>();
            t->transform = translation(dx, dy);
            t->alignment = Alignment::topLeft();  // No pivot for translate
            t->child = std::move(child);
            return t;
        }
        void debugFillProperties(DiagnosticsPropertyBuilder& properties) const override;

    };

} // namespace systems::leal::campello_widgets
