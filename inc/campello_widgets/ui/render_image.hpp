#pragma once

#include <memory>
#include <campello_widgets/ui/render_box.hpp>
#include <campello_widgets/ui/box_fit.hpp>
#include <campello_widgets/ui/alignment.hpp>

namespace systems::leal::campello_gpu { class Texture; }

namespace systems::leal::campello_widgets
{

    /**
     * @brief RenderBox that paints a GPU texture.
     *
     * Sizes itself to `explicit_size` when set; otherwise fills the tightest
     * available constraint (same behaviour as `RenderRectangle` with fill=true).
     */
    class RenderImage : public RenderBox
    {
    public:
        void setTexture(std::shared_ptr<campello_gpu::Texture> texture) noexcept;
        void setExplicitSize(Size size) noexcept;
        void setFit(BoxFit fit) noexcept;
        void setAlignment(Alignment alignment) noexcept;
        void setOpacity(float opacity) noexcept;

        campello_gpu::Texture* texture()      const noexcept { return texture_.get(); }
        Size                   explicitSize() const noexcept { return explicit_size_; }
        BoxFit                 fit()          const noexcept { return fit_; }
        Alignment              alignment()    const noexcept { return alignment_; }
        float                  opacity()      const noexcept { return opacity_; }

        void performLayout() override;
        void performPaint(PaintContext& context, const Offset& offset) override;

    private:
        std::shared_ptr<campello_gpu::Texture> texture_;
        Size                                   explicit_size_; ///< Zero = use constraints.
        BoxFit                                 fit_       = BoxFit::fill;
        Alignment                              alignment_ = Alignment::center();
        float                                  opacity_   = 1.0f;
    };

} // namespace systems::leal::campello_widgets
