#include <campello_widgets/ui/render_image.hpp>
#include <campello_widgets/ui/paint_context.hpp>
#include <campello_widgets/ui/canvas.hpp>
#include <campello_widgets/ui/rect.hpp>
#include <campello_gpu/texture.hpp>
#include <algorithm>
#include <cmath>
#include <iostream>

namespace systems::leal::campello_widgets
{

    void RenderImage::setTexture(
        std::shared_ptr<campello_gpu::Texture> texture) noexcept
    {
        std::cerr << "[RenderImage] setTexture: old=" << texture_.get() << " new=" << texture.get() << "\n";
        texture_ = std::move(texture);
        markNeedsPaint();
    }

    void RenderImage::setExplicitSize(Size size) noexcept
    {
        explicit_size_ = size;
        markNeedsLayout();
    }

    void RenderImage::setFit(BoxFit fit) noexcept
    {
        if (fit_ == fit) return;
        fit_ = fit;
        markNeedsPaint();
    }

    void RenderImage::setAlignment(Alignment alignment) noexcept
    {
        if (alignment_ == alignment) return;
        alignment_ = alignment;
        markNeedsPaint();
    }

    void RenderImage::setOpacity(float opacity) noexcept
    {
        if (opacity_ == opacity) return;
        opacity_ = opacity;
        markNeedsPaint();
    }

    void RenderImage::performLayout()
    {
        const bool has_explicit = explicit_size_.width > 0.0f || explicit_size_.height > 0.0f;
        if (has_explicit)
        {
            size_ = constraints_.constrain(explicit_size_);
        }
        else
        {
            const float w = std::isinf(constraints_.max_width)  ? 0.0f : constraints_.max_width;
            const float h = std::isinf(constraints_.max_height) ? 0.0f : constraints_.max_height;
            size_ = constraints_.constrain(Size{w, h});
        }
        // Force a minimum size for debugging
        if (size_.width <= 0) size_.width = 100;
        if (size_.height <= 0) size_.height = 100;
        std::cerr << "[RenderImage] performLayout: size=" << size_.width << "x" << size_.height << "\n";
    }

    void RenderImage::performPaint(PaintContext& context, const Offset& offset)
    {
        if (!texture_) {
            std::cerr << "[RenderImage] No texture!\n";
            return;
        }

        const float bw = size_.width;
        const float bh = size_.height;
        const float tw = static_cast<float>(texture_->getWidth());
        const float th = static_cast<float>(texture_->getHeight());

        std::cerr << "[RenderImage] Paint: box=" << bw << "x" << bh 
                  << " tex=" << tw << "x" << th << " offset=" << offset.x << "," << offset.y << "\n";

        if (bw <= 0.0f || bh <= 0.0f || tw <= 0.0f || th <= 0.0f) {
            std::cerr << "[RenderImage] Invalid sizes, skipping\n";
            return;
        }

        // Normalized full-texture source rect
        Rect src = Rect::fromLTWH(0.0f, 0.0f, 1.0f, 1.0f);
        Rect dst;
        bool need_clip = false;

        switch (fit_)
        {
            case BoxFit::fill:
            {
                dst = Rect::fromOffsetAndSize(offset, size_);
                break;
            }
            case BoxFit::contain:
            {
                const float scale = std::min(bw / tw, bh / th);
                const float dw    = tw * scale;
                const float dh    = th * scale;
                const Offset pos  = alignment_.inscribe({dw, dh}, {bw, bh});
                dst = Rect::fromLTWH(offset.x + pos.x, offset.y + pos.y, dw, dh);
                break;
            }
            case BoxFit::cover:
            {
                // Scale uniformly to cover the box; use src_rect to crop overflow.
                const float scale  = std::max(bw / tw, bh / th);
                const float sw     = tw * scale;
                const float sh     = th * scale;
                // Fraction of the texture that the box reveals
                const float frac_x = bw / sw;
                const float frac_y = bh / sh;
                // Alignment determines which portion of the texture is visible
                const float off_x  = (1.0f - frac_x) * (1.0f + alignment_.x) * 0.5f;
                const float off_y  = (1.0f - frac_y) * (1.0f + alignment_.y) * 0.5f;
                src = Rect::fromLTWH(off_x, off_y, frac_x, frac_y);
                dst = Rect::fromOffsetAndSize(offset, size_);
                break;
            }
            case BoxFit::fitWidth:
            {
                const float scale = bw / tw;
                const float dh    = th * scale;
                const Offset pos  = alignment_.inscribe({bw, dh}, {bw, bh});
                dst       = Rect::fromLTWH(offset.x + pos.x, offset.y + pos.y, bw, dh);
                need_clip = (dh > bh);
                break;
            }
            case BoxFit::fitHeight:
            {
                const float scale = bh / th;
                const float dw    = tw * scale;
                const Offset pos  = alignment_.inscribe({dw, bh}, {bw, bh});
                dst       = Rect::fromLTWH(offset.x + pos.x, offset.y + pos.y, dw, bh);
                need_clip = (dw > bw);
                break;
            }
            case BoxFit::none:
            {
                const Offset pos = alignment_.inscribe({tw, th}, {bw, bh});
                dst       = Rect::fromLTWH(offset.x + pos.x, offset.y + pos.y, tw, th);
                need_clip = (tw > bw || th > bh);
                break;
            }
            case BoxFit::scaleDown:
            {
                const float scale = std::min(1.0f, std::min(bw / tw, bh / th));
                const float dw    = tw * scale;
                const float dh    = th * scale;
                const Offset pos  = alignment_.inscribe({dw, dh}, {bw, bh});
                dst = Rect::fromLTWH(offset.x + pos.x, offset.y + pos.y, dw, dh);
                break;
            }
        }

        auto& canvas = context.canvas();

        std::cerr << "[RenderImage] Drawing image: src=" << src.x << "," << src.y << " " << src.width << "x" << src.height
                  << " dst=" << dst.x << "," << dst.y << " " << dst.width << "x" << dst.height << "\n";

        canvas.save();
        if (opacity_ < 1.0f)
            canvas.setOpacity(opacity_);
        if (need_clip)
            canvas.clipRect(Rect::fromOffsetAndSize(offset, size_));
        canvas.drawImage(texture_, src, dst);
        canvas.restore();
        
        std::cerr << "[RenderImage] Draw call issued\n";
    }

} // namespace systems::leal::campello_widgets
