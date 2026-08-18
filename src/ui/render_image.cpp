#include <campello_widgets/ui/render_image.hpp>
#include <campello_widgets/ui/paint_context.hpp>
#include <campello_widgets/ui/canvas.hpp>
#include <campello_widgets/ui/rect.hpp>
#include <campello_gpu/texture.hpp>
#include <algorithm>
#include <cmath>

namespace systems::leal::campello_widgets
{

    namespace
    {
        // Mirrors Flutter's BoxConstraints.constrainSizeAndAttemptToPreserveAspectRatio:
        // fits `natural` within `constraints`, shrinking/growing it towards
        // whichever bound it exceeds while keeping its aspect ratio, rather
        // than stretching it independently on each axis.
        Size constrainPreservingAspectRatio(const BoxConstraints& constraints, Size natural)
        {
            if (constraints.isTight())
                return {constraints.min_width, constraints.min_height};

            float width  = natural.width;
            float height = natural.height;
            const float aspect_ratio = width / height;

            if (width > constraints.max_width) {
                width  = constraints.max_width;
                height = width / aspect_ratio;
            }
            if (height > constraints.max_height) {
                height = constraints.max_height;
                width  = height * aspect_ratio;
            }
            if (width < constraints.min_width) {
                width  = constraints.min_width;
                height = width / aspect_ratio;
            }
            if (height < constraints.min_height) {
                height = constraints.min_height;
                width  = height * aspect_ratio;
            }

            return constraints.constrain({width, height});
        }
    }

    void RenderImage::setTexture(
        std::shared_ptr<campello_gpu::Texture> texture) noexcept
    {
        if (texture_ == texture) return;
        texture_ = std::move(texture);
        markNeedsPaint();
    }

    void RenderImage::setExplicitSize(Size size) noexcept
    {
        if (explicit_size_ == size) return;
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

    void RenderImage::setColor(std::optional<Color> color) noexcept
    {
        if (color_ == color) return;
        color_ = color;
        markNeedsPaint();
    }

    void RenderImage::performLayout()
    {
        const bool has_explicit = explicit_size_.width > 0.0f || explicit_size_.height > 0.0f;
        const float natural_w = texture_ ? static_cast<float>(texture_->getWidth())  : 0.0f;
        const float natural_h = texture_ ? static_cast<float>(texture_->getHeight()) : 0.0f;

        if (has_explicit)
        {
            size_ = constraints_.constrain(explicit_size_);
        }
        else if (texture_ && natural_w > 0.0f && natural_h > 0.0f)
        {
            // No explicit size: mirror Flutter's Image, which sizes itself to
            // the source image's natural dimensions, preserving aspect ratio
            // as it fits within the incoming constraints.
            size_ = constrainPreservingAspectRatio(constraints_, Size{natural_w, natural_h});
        }
        else
        {
            // No explicit size and no (usable) texture yet — e.g.
            // RenderDrawSurface sizes itself on its first layout, before it
            // has created a texture to report a natural size from. There's
            // nothing to preserve the aspect ratio of, so fall back to
            // filling the available space, as before.
            const float w = std::isinf(constraints_.max_width)  ? 0.0f : constraints_.max_width;
            const float h = std::isinf(constraints_.max_height) ? 0.0f : constraints_.max_height;
            size_ = constraints_.constrain(Size{w, h});
        }
        // Force a minimum size for debugging
        if (size_.width <= 0) size_.width = 100;
        if (size_.height <= 0) size_.height = 100;
    }

    void RenderImage::performPaint(PaintContext& context, const Offset& offset)
    {
        if (!texture_)
            return;

        const float bw = size_.width;
        const float bh = size_.height;
        const float tw = static_cast<float>(texture_->getWidth());
        const float th = static_cast<float>(texture_->getHeight());

        if (bw <= 0.0f || bh <= 0.0f || tw <= 0.0f || th <= 0.0f)
            return;

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

        canvas.save();
        if (opacity_ < 1.0f)
            canvas.setOpacity(opacity_);
        if (need_clip)
            canvas.clipRect(Rect::fromOffsetAndSize(offset, size_));
        if (color_.has_value())
            canvas.drawTintedImage(texture_, src, dst, *color_);
        else
            canvas.drawImage(texture_, src, dst);
        canvas.restore();
    }

} // namespace systems::leal::campello_widgets
