#pragma once

#include <campello_widgets/ui/render_box.hpp>
#include <campello_widgets/ui/rect.hpp>

namespace systems::leal::campello_widgets
{

    /**
     * @brief Transparent single-child RenderBox backing the `Hero` widget --
     * captures its own on-screen rect during paint (via
     * `RenderBox::computeGlobalRect()`, the same recipe `RenderGestureDetector`
     * uses for `globalOffset()`) so `HeroController` can read it once the
     * frame's paint pass has finished (see `PostFrameCallbacks`).
     *
     * `setHidden(true)` skips painting the child while still laying out and
     * capturing its rect normally -- the placeholder-swap mechanism a Hero
     * flight uses to hide the real content at both endpoints while the
     * flight's own shuttle overlay is what's actually visible.
     */
    class RenderHero : public RenderBox
    {
    public:
        // Transparent layout: pass constraints through unchanged, same as
        // RenderGestureDetector -- see its own performLayout() doc.
        void performLayout() override;
        void performPaint(PaintContext& context, const Offset& offset) override;

        /** @brief This box's on-screen rect as of the last paint. */
        Rect globalRect() const noexcept { return global_rect_; }

        /** @brief When true, the child is not painted (but still laid out and measured). */
        void setHidden(bool hidden) noexcept;

        bool hidden() const noexcept { return hidden_; }

    private:
        Rect global_rect_;
        bool hidden_ = false;
    };

} // namespace systems::leal::campello_widgets
