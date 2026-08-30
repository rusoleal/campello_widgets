#include <campello_widgets/widgets/scrollbar.hpp>
#include <campello_widgets/widgets/layout_builder.hpp>
#include <campello_widgets/widgets/stack.hpp>
#include <campello_widgets/widgets/positioned.hpp>
#include <campello_widgets/widgets/colored_box.hpp>
#include <campello_widgets/widgets/sized_box.hpp>
#include <campello_widgets/widgets/gesture_detector.hpp>
#include <campello_widgets/ui/scrollbar_geometry.hpp>

#include <algorithm>

namespace systems::leal::campello_widgets
{

    class ScrollbarState : public State<Scrollbar>
    {
    public:
        void initState() override { subscribe(); }
        void dispose() override { unsubscribe(); }

        void didUpdateWidget(const Widget& old_widget_base) override
        {
            const auto& old_w = static_cast<const Scrollbar&>(old_widget_base);
            if (old_w.controller.get() != widget().controller.get())
            {
                if (old_w.controller && listener_id_ != 0)
                    old_w.controller->removeListener(listener_id_);
                listener_id_ = 0;
                subscribe();
            }
        }

        WidgetRef build(BuildContext&) override
        {
            auto lb = std::make_shared<LayoutBuilder>();
            lb->builder = [this](BuildContext&, BoxConstraints constraints) -> WidgetRef
            {
                return buildContent(constraints);
            };
            return lb;
        }

    private:
        void subscribe()
        {
            if (widget().controller)
                listener_id_ = widget().controller->addListener([this]() { setState([]() {}); });
        }

        void unsubscribe()
        {
            if (widget().controller && listener_id_ != 0)
                widget().controller->removeListener(listener_id_);
        }

        WidgetRef buildContent(const BoxConstraints& constraints)
        {
            const auto& w = widget();
            std::vector<WidgetRef> stack_children{w.child};

            if (w.controller)
            {
                const float viewport = w.is_vertical ? constraints.max_height : constraints.max_width;
                const ScrollbarThumbGeometry geo = computeScrollbarThumbGeometry(
                    viewport, w.controller->minScrollExtent(), w.controller->maxScrollExtent(),
                    w.controller->offset(), w.min_thumb_length);

                auto thumb = std::make_shared<ColoredBox>(w.thumb_color);
                WidgetRef sized = w.is_vertical
                    ? std::static_pointer_cast<Widget>(std::make_shared<SizedBox>(w.thickness, geo.length, thumb))
                    : std::static_pointer_cast<Widget>(std::make_shared<SizedBox>(geo.length, w.thickness, thumb));

                const float track_room   = viewport - geo.length;
                const float content_range = std::max(
                    0.0f, w.controller->maxScrollExtent() - w.controller->minScrollExtent());
                auto controller_ptr = w.controller;
                const bool is_vertical = w.is_vertical;

                auto gd = std::make_shared<GestureDetector>();
                gd->child = sized;
                gd->on_pan_update = [controller_ptr, track_room, content_range, is_vertical](Offset delta)
                {
                    if (track_room <= 0.0f || content_range <= 0.0f) return;
                    const float delta_main   = is_vertical ? delta.y : delta.x;
                    const float scroll_delta = delta_main / track_room * content_range;
                    controller_ptr->jumpTo(controller_ptr->offset() + scroll_delta);
                };

                auto positioned = std::make_shared<Positioned>();
                if (w.is_vertical)
                {
                    positioned->top   = geo.position;
                    positioned->right = 0.0f;
                }
                else
                {
                    positioned->left   = geo.position;
                    positioned->bottom = 0.0f;
                }
                positioned->child = gd;

                stack_children.push_back(positioned);
            }

            return std::make_shared<Stack>(std::move(stack_children));
        }

        uint64_t listener_id_ = 0;
    };

    std::unique_ptr<StateBase> Scrollbar::createState() const
    {
        return std::make_unique<ScrollbarState>();
    }

} // namespace systems::leal::campello_widgets
