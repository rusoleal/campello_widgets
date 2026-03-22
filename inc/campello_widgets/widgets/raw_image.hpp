#pragma once

#include <memory>
#include <campello_widgets/widgets/render_object_widget.hpp>
#include <campello_widgets/ui/size.hpp>
#include <campello_widgets/ui/box_fit.hpp>
#include <campello_widgets/ui/alignment.hpp>

namespace systems::leal::campello_gpu { class Texture; }

namespace systems::leal::campello_widgets
{

    /**
     * @brief A widget that renders a campello_gpu texture.
     *
     * RawImage maps directly to `RenderImage` and issues a `DrawImageCmd`
     * each paint pass. For most use cases prefer the higher-level `Image`
     * widget (Phase 6), which adds fit, alignment, and placeholder support.
     *
     * **Usage:**
     * @code
     * auto img = RawImage::create(my_texture, Size{128, 128});
     * @endcode
     */
    class RawImage : public RenderObjectWidget
    {
    public:
        std::shared_ptr<campello_gpu::Texture> texture;

        /**
         * @brief Explicit display size in logical pixels.
         *
         * If both components are zero the image fills the available constraints.
         */
        Size      size      = Size::zero();
        BoxFit    fit       = BoxFit::fill;
        Alignment alignment = Alignment::center();
        float     opacity   = 1.0f;

        static std::shared_ptr<RawImage> create(
            std::shared_ptr<campello_gpu::Texture> texture,
            Size                                   size      = Size::zero(),
            BoxFit                                 fit       = BoxFit::fill,
            Alignment                              alignment = Alignment::center(),
            float                                  opacity   = 1.0f)
        {
            auto w     = std::make_shared<RawImage>();
            w->texture  = std::move(texture);
            w->size     = size;
            w->fit      = fit;
            w->alignment = alignment;
            w->opacity  = opacity;
            return w;
        }

        std::shared_ptr<RenderObject> createRenderObject() const override;
        void updateRenderObject(RenderObject& render_object) const override;
    };

} // namespace systems::leal::campello_widgets
