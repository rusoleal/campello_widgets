#pragma once

#include <memory>
#include <campello_widgets/widgets/render_object_widget.hpp>
#include <campello_widgets/ui/color.hpp>

namespace systems::leal::campello_widgets
{

    /**
     * @brief A widget that paints a solid colored rectangle.
     *
     * RawRectangle is the lowest-level colored box. It maps directly to a
     * `RenderRectangle` and issues a `DrawRectCmd` each paint pass.
     * For most use cases prefer `Container` (Phase 6), which composes
     * padding, decoration, and a child on top of this primitive.
     *
     * **Usage:**
     * @code
     * auto box = RawRectangle::create(Color::blue());
     * @endcode
     */
    class RawRectangle : public RenderObjectWidget
    {
    public:
        /** @brief Rectangle fill color. */
        Color color = Color::black();

        /**
         * @brief Corner radius in logical pixels.
         *
         * Rounded-rect drawing requires extended PaintContext support —
         * this field is stored now and will take effect in a later phase.
         */
        float corner_radius = 0.0f;

        /**
         * @brief When true, expands to the maximum allowed constraint.
         *        When false, collapses to zero in unconstrained axes.
         */
        bool fill = true;

        static std::shared_ptr<RawRectangle> create(
            Color color,
            bool  fill          = true,
            float corner_radius = 0.0f)
        {
            auto w           = std::make_shared<RawRectangle>();
            w->color         = color;
            w->fill          = fill;
            w->corner_radius = corner_radius;
            return w;
        }

        std::shared_ptr<RenderObject> createRenderObject() const override;
        void updateRenderObject(RenderObject& render_object) const override;
    };

} // namespace systems::leal::campello_widgets
