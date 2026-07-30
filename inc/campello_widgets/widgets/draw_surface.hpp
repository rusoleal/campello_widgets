#pragma once

#include <campello_widgets/widgets/render_object_widget.hpp>
#include <campello_widgets/ui/color.hpp>

namespace systems::leal::campello_widgets
{

    /**
     * @brief A freehand-drawing canvas widget.
     *
     * Maps directly to `RenderDrawSurface`. Fills the available constraints
     * (like a `SizedBox.expand()`'d child) and accumulates pointer-driven
     * strokes into its own persistent GPU texture — see
     * `RenderDrawSurface`'s doc comment for why this is incremental rather
     * than a full re-render every frame.
     *
     * **Usage:**
     * @code
     * auto surface = DrawSurface::create();
     * @endcode
     */
    class DrawSurface : public RenderObjectWidget
    {
    public:
        Color stroke_color     = Color::black();
        Color background_color = Color::white();
        float stroke_width     = 4.0f;

        static std::shared_ptr<DrawSurface> create(
            Color stroke_color     = Color::black(),
            Color background_color = Color::white(),
            float stroke_width     = 4.0f)
        {
            auto w = std::make_shared<DrawSurface>();
            w->stroke_color     = stroke_color;
            w->background_color = background_color;
            w->stroke_width     = stroke_width;
            return w;
        }

        std::shared_ptr<RenderObject> createRenderObject() const override;
        void updateRenderObject(RenderObject& render_object) const override;
    };

} // namespace systems::leal::campello_widgets
