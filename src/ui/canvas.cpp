#include <campello_widgets/ui/canvas.hpp>

#include <cassert>

namespace systems::leal::campello_widgets
{

    Canvas::Canvas(float viewport_width, float viewport_height)
        : current_transform_(Matrix4::identity())
        , current_clip_(Rect::fromLTWH(0.0f, 0.0f, viewport_width, viewport_height))
    {
    }

    // ------------------------------------------------------------------
    // Draw primitives
    // ------------------------------------------------------------------

    void Canvas::drawRect(const Rect& rect, const Paint& paint)
    {
        if (current_opacity_ >= 1.0f)
        {
            commands_.push_back(DrawRectCmd{rect, paint});
        }
        else
        {
            Paint p = paint;
            p.color.a *= current_opacity_;
            commands_.push_back(DrawRectCmd{rect, p});
        }
    }

    void Canvas::drawText(const TextSpan& span, const Offset& origin)
    {
        if (current_opacity_ >= 1.0f)
        {
            commands_.push_back(DrawTextCmd{span, origin});
        }
        else
        {
            TextSpan s = span;
            s.style.color.a *= current_opacity_;
            commands_.push_back(DrawTextCmd{s, origin});
        }
    }

    void Canvas::drawImage(
        std::shared_ptr<campello_gpu::Texture> texture,
        const Rect& src_rect,
        const Rect& dst_rect)
    {
        commands_.push_back(DrawImageCmd{std::move(texture), src_rect, dst_rect, current_opacity_});
    }

    // ------------------------------------------------------------------
    // State management
    // ------------------------------------------------------------------

    void Canvas::save()
    {
        save_stack_.push_back({current_transform_, current_clip_, current_opacity_, 0, 0});
    }

    void Canvas::restore()
    {
        assert(!save_stack_.empty() && "restore() without matching save()");

        const SaveEntry& entry = save_stack_.back();

        for (int i = 0; i < entry.pushed_transforms; ++i)
            commands_.push_back(PopTransformCmd{});

        for (int i = 0; i < entry.pushed_clips; ++i)
            commands_.push_back(PopClipRectCmd{});

        current_transform_ = entry.transform;
        current_clip_      = entry.clip;
        current_opacity_   = entry.opacity;

        save_stack_.pop_back();
    }

    void Canvas::setOpacity(float factor) noexcept
    {
        current_opacity_ *= factor;
    }

    // ------------------------------------------------------------------
    // Transform
    // ------------------------------------------------------------------

    void Canvas::translate(float dx, float dy)
    {
        transform(Matrix4::translate({dx, dy, 0.0f}));
    }

    void Canvas::transform(const Matrix4& matrix)
    {
        commands_.push_back(PushTransformCmd{matrix});
        current_transform_ = current_transform_ * matrix;

        if (!save_stack_.empty())
            ++save_stack_.back().pushed_transforms;
    }

    // ------------------------------------------------------------------
    // Clip
    // ------------------------------------------------------------------

    void Canvas::clipRect(const Rect& rect)
    {
        const Rect clipped = current_clip_.intersection(rect);
        commands_.push_back(PushClipRectCmd{clipped});
        current_clip_ = clipped;

        if (!save_stack_.empty())
            ++save_stack_.back().pushed_clips;
    }

} // namespace systems::leal::campello_widgets
