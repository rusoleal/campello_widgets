#pragma once

#include <campello_widgets/ui/render_box.hpp>

namespace systems::leal::campello_widgets
{

    /**
     * @brief A RenderBox that, when offstage, contributes zero size and is
     * never laid out, painted, or hit-tested -- while its child's Element/
     * State (and anything it owns, e.g. a GPU device) stays mounted.
     *
     * Unlike `Opacity` (which still fully repaints its child every frame,
     * just with alpha baked to 0 -- real per-frame CPU/GPU cost for content
     * nobody can see, and, if a descendant `RenderRepaintBoundary` decides
     * to replay its cache on some frame instead of repainting, a real
     * correctness bug: the replay bypasses the ambient opacity multiplier
     * entirely and can flash stale full-alpha content), `RenderOffstage`
     * overrides `paint()` itself to never call through to `performPaint()`
     * (or reach any descendant) at all while offstage. Reactivating later
     * lays out and repaints normally -- including any descendant boundary
     * legitimately replaying its still-valid cache, since nothing below an
     * offstage subtree ever gets a chance to go stale mid-hide the way it
     * could under `Opacity`.
     *
     * Matches Flutter's `Offstage` widget.
     */
    class RenderOffstage : public RenderBox
    {
    public:
        explicit RenderOffstage(bool offstage = true);

        void setOffstage(bool offstage) noexcept;
        bool offstage() const noexcept { return offstage_; }

        void performLayout() override;
        void performPaint(PaintContext& context, const Offset& offset) override;
        void paint(PaintContext& context, const Offset& offset) override;
        bool hitTestChildren(HitTestResult& result, const Offset& position) override;

    private:
        bool offstage_;
    };

} // namespace systems::leal::campello_widgets
