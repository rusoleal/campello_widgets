#pragma once

#include <campello_widgets/ui/render_box.hpp>
#include <campello_widgets/ui/rrect.hpp>
#include <campello_widgets/ui/offset_layer.hpp>

namespace systems::leal::campello_widgets
{

    /**
     * @brief Clips its child to a rounded rectangle.
     *
     * `border_radius` is applied to all four corners. Layout is pass-through.
     *
     * Self-boundaring (`isRepaintBoundary() == true`), the same mechanism
     * `RenderRepaintBoundary` uses (see its class doc for the full
     * rationale) — a ClipRRect's actual composite (its GPU offscreen
     * texture, produced by `Renderer::applyClipShape()`) is expensive to
     * rebuild: a fresh texture allocation plus two extra render-pass
     * restarts on the enclosing pass. Without owning an `OffsetLayer`,
     * `RenderObject::paint()`'s default implementation calls
     * `performPaint()` unconditionally on every visit from an ancestor's
     * repaint — e.g. a `Transform` above it animating every tick — even
     * when this node's own `needsPaint()` is false and its on-screen
     * offset hasn't moved. Owning the cache directly here means any
     * ClipRRect gets this for free, without callers needing to separately
     * wrap it in a `RepaintBoundary`.
     */
    class RenderClipRRect : public RenderBox
    {
    public:
        float border_radius = 0.0f;

        void performLayout() override;
        void performPaint(PaintContext& context, const Offset& offset) override;
        void paint(PaintContext& context, const Offset& offset) override;
        bool isRepaintBoundary() const noexcept override { return true; }

    private:
        OffsetLayer offset_layer_;
    };

} // namespace systems::leal::campello_widgets
