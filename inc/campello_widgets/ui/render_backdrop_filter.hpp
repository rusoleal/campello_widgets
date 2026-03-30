#pragma once

#include <campello_widgets/ui/render_box.hpp>
#include <campello_widgets/ui/image_filter.hpp>

namespace systems::leal::campello_widgets
{

    /**
     * @brief RenderObject that blurs the scene behind it and renders its child on top.
     *
     * @ref BackdropFilter is the Widget counterpart.
     *
     * During the layout pass the render object calls `Renderer::noteBackdropFilter()`
     * so the Renderer knows to run a two-pass frame (backdrop capture + main).
     *
     * During the paint pass:
     *  - The draw list receives `DrawBackdropFilterBeginCmd` + child commands +
     *    `DrawBackdropFilterEndCmd`.
     *  - The Renderer skips the child commands when rendering the backdrop-capture
     *    pass, and draws the pre-blurred backdrop when replaying the main pass.
     */
    class RenderBackdropFilter final : public RenderBox
    {
    public:
        explicit RenderBackdropFilter(ImageFilter filter = ImageFilter::blur(8.0f))
            : filter_(filter) {}

        void setFilter(const ImageFilter& filter) noexcept
        {
            if (filter_.sigma_x != filter.sigma_x ||
                filter_.sigma_y != filter.sigma_y)
            {
                filter_ = filter;
                markNeedsPaint();
            }
        }

        const ImageFilter& filter() const noexcept { return filter_; }

        // ------------------------------------------------------------------
        // RenderBox overrides
        // ------------------------------------------------------------------

        void performLayout() override;
        void performPaint(PaintContext& ctx, const Offset& offset) override;
        bool hitTestChildren(HitTestResult& result, const Offset& position) override;

    private:
        ImageFilter filter_;
    };

} // namespace systems::leal::campello_widgets
