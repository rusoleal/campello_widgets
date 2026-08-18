#pragma once

#include <memory>
#include <optional>
#include <campello_widgets/ui/render_box.hpp>
#include <campello_widgets/ui/box_fit.hpp>
#include <campello_widgets/ui/alignment.hpp>
#include <campello_widgets/ui/color.hpp>

namespace systems::leal::campello_gpu { class Texture; }

namespace systems::leal::campello_widgets
{

    /**
     * @brief RenderBox that paints a GPU texture.
     *
     * Sizes itself to `explicit_size` when set. Otherwise, if a texture is
     * set, it sizes itself to the texture's natural pixel dimensions,
     * preserving aspect ratio as it fits within the incoming constraints —
     * matching Flutter's `Image` sizing behaviour. With no explicit size and
     * no texture (e.g. a `RenderImage` subclass that creates its texture
     * lazily, like `RenderDrawSurface`), it falls back to filling the
     * tightest available constraint.
     */
    class RenderImage : public RenderBox
    {
    public:
        void setTexture(std::shared_ptr<campello_gpu::Texture> texture) noexcept;
        void setExplicitSize(Size size) noexcept;
        void setFit(BoxFit fit) noexcept;
        void setAlignment(Alignment alignment) noexcept;
        void setOpacity(float opacity) noexcept;

        /**
         * @brief Recolors the texture using its alpha channel as a stencil
         * ("template image" tinting — see `DrawTintedImageCmd`'s doc
         * comment), ignoring its own RGB entirely. `std::nullopt` (the
         * default) paints the texture's real colors unmodified. Used by
         * the `Icon` widget so one monochrome asset can serve any theme
         * color.
         */
        void setColor(std::optional<Color> color) noexcept;

        campello_gpu::Texture* texture()      const noexcept { return texture_.get(); }
        Size                   explicitSize() const noexcept { return explicit_size_; }
        BoxFit                 fit()          const noexcept { return fit_; }
        Alignment              alignment()    const noexcept { return alignment_; }
        float                  opacity()      const noexcept { return opacity_; }
        std::optional<Color>   color()        const noexcept { return color_; }

        void performLayout() override;
        void performPaint(PaintContext& context, const Offset& offset) override;

    private:
        std::shared_ptr<campello_gpu::Texture> texture_;
        Size                                   explicit_size_; ///< Zero = use constraints.
        BoxFit                                 fit_       = BoxFit::fill;
        Alignment                              alignment_ = Alignment::center();
        float                                  opacity_   = 1.0f;
        std::optional<Color>                   color_;
    };

} // namespace systems::leal::campello_widgets
