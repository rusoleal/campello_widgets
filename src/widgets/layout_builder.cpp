#include <campello_widgets/widgets/layout_builder.hpp>
#include <campello_widgets/widgets/single_child_render_object_widget.hpp>
#include <campello_widgets/ui/render_box.hpp>
#include <campello_widgets/ui/paint_context.hpp>

#include <functional>
#include <memory>

namespace systems::leal::campello_widgets
{

    // ------------------------------------------------------------------
    // RenderConstraintCapture
    // ------------------------------------------------------------------

    /**
     * Transparent RenderBox that forwards its constraints to a callback
     * during performLayout, then lays out and sizes to its child.
     */
    class RenderConstraintCapture : public RenderBox
    {
    public:
        std::function<void(BoxConstraints)> on_constraints;

        void performLayout() override
        {
            if (on_constraints)
                on_constraints(constraints_);

            if (child_)
            {
                layoutChild(*child_, constraints_);
                size_ = child_->size();
            }
            else
            {
                size_ = constraints_.constrain({0.0f, 0.0f});
            }
        }

        void performPaint(PaintContext& ctx, const Offset& off) override
        {
            RenderBox::performPaint(ctx, off);
        }
    };

    // ------------------------------------------------------------------
    // ConstraintCapture (widget wrapper)
    // ------------------------------------------------------------------

    class ConstraintCapture : public SingleChildRenderObjectWidget
    {
    public:
        std::function<void(BoxConstraints)> on_constraints;

        std::shared_ptr<RenderObject> createRenderObject() const override
        {
            auto r = std::make_shared<RenderConstraintCapture>();
            r->on_constraints = on_constraints;
            return r;
        }

        void updateRenderObject(RenderObject& ro) const override
        {
            static_cast<RenderConstraintCapture&>(ro).on_constraints = on_constraints;
        }
    };

    // ------------------------------------------------------------------
    // LayoutBuilderState
    // ------------------------------------------------------------------

    class LayoutBuilderState : public State<LayoutBuilder>
    {
    public:
        WidgetRef build(BuildContext& ctx) override
        {
            const auto& w = widget();

            auto probe          = std::make_shared<ConstraintCapture>();
            probe->on_constraints = [this](BoxConstraints c) {
                if (c != last_constraints_)
                {
                    last_constraints_ = c;
                    setState([](){});
                }
            };
            probe->child = w.builder(ctx, last_constraints_);
            return probe;
        }

    private:
        BoxConstraints last_constraints_;
    };

    // ------------------------------------------------------------------

    std::unique_ptr<StateBase> LayoutBuilder::createState() const
    {
        return std::make_unique<LayoutBuilderState>();
    }

} // namespace systems::leal::campello_widgets
