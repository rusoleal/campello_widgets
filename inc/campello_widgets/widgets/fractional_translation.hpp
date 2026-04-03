#pragma once

#include <campello_widgets/widgets/single_child_render_object_widget.hpp>
#include <campello_widgets/ui/render_box.hpp>
#include <campello_widgets/ui/paint_context.hpp>
#include <campello_widgets/ui/hit_test.hpp>
#include <campello_widgets/ui/offset.hpp>

namespace systems::leal::campello_widgets
{

    /**
     * @brief A render object that translates its child by a fraction of the
     * child's own size.
     *
     * An `translation` of `{-1, 0}` shifts the child one full width to the left;
     * `{0, -1}` shifts it one full height upward; `{0, 0}` is the identity.
     */
    class RenderFractionalTranslation final : public RenderBox
    {
    public:
        explicit RenderFractionalTranslation(Offset translation = {})
            : translation_(translation) {}

        void setTranslation(Offset t) noexcept
        {
            if (t.x != translation_.x || t.y != translation_.y) {
                translation_ = t;
                markNeedsPaint();
            }
        }

        Offset translation() const noexcept { return translation_; }

        void performLayout() override
        {
            if (child_) {
                child_->layout(constraints());
                size_ = child_->size();
            } else {
                size_ = constraints().constrain({0.0f, 0.0f});
            }
        }

        void performPaint(PaintContext& ctx, const Offset& offset) override
        {
            if (!child_) return;
            const Offset shifted{
                offset.x + translation_.x * size_.width,
                offset.y + translation_.y * size_.height
            };
            paintChild(ctx, shifted);
        }

        bool hitTestChildren(HitTestResult& result, const Offset& position) override
        {
            if (!child_) return false;
            const Offset adjusted{
                position.x - translation_.x * size_.width,
                position.y - translation_.y * size_.height
            };
            return child_->hitTest(result, adjusted);
        }

        bool hitTestSelf(const Offset&) const override { return false; }

    private:
        Offset translation_;
    };

    // -----------------------------------------------------------------------

    /**
     * @brief Translates its child by a fraction of the child's own size.
     *
     * Useful for slide-in/slide-out effects where the offset should be
     * independent of pixel density.
     *
     * @code
     * // Slide child in from the left
     * auto w = std::make_shared<FractionalTranslation>();
     * w->translation = {-1.0f, 0.0f};  // one full width to the left
     * w->child       = someChild;
     * @endcode
     */
    class FractionalTranslation : public SingleChildRenderObjectWidget
    {
    public:
        Offset translation;  ///< Fractional offset ({-1,0} = one width left)

        FractionalTranslation() = default;
        explicit FractionalTranslation(Offset trans, WidgetRef c = nullptr)
            : translation(trans)
        {
            child = std::move(c);
        }

        std::shared_ptr<RenderObject> createRenderObject() const override
        {
            return std::make_shared<RenderFractionalTranslation>(translation);
        }

        void updateRenderObject(RenderObject& ro) const override
        {
            static_cast<RenderFractionalTranslation&>(ro)
                .setTranslation(translation);
        }
    };

} // namespace systems::leal::campello_widgets
