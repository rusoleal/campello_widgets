#include <campello_widgets/widgets/reorderable_list_view.hpp>
#include <campello_widgets/widgets/column.hpp>
#include <campello_widgets/widgets/gesture_detector.hpp>
#include <campello_widgets/widgets/transform.hpp>
#include <campello_widgets/ui/reorder_math.hpp>

namespace systems::leal::campello_widgets
{

    class ReorderableListViewState : public State<ReorderableListView>
    {
    public:
        WidgetRef build(BuildContext&) override
        {
            const auto& w = widget();

            std::vector<WidgetRef> items;
            items.reserve(w.children.size());

            for (int i = 0; i < static_cast<int>(w.children.size()); ++i)
            {
                WidgetRef item_widget = w.children[i];

                if (dragging_index_ == i)
                {
                    // Follows the finger vertically -- see class doc's
                    // "no animated sibling gap" simplification.
                    auto t = std::make_shared<Transform>(
                        Transform::translation(0.0f, drag_delta_), Alignment::topLeft(), item_widget);
                    item_widget = t;
                }

                auto gd = std::make_shared<GestureDetector>();
                gd->child = item_widget;
                gd->on_pan_down = [this, i](Offset)
                {
                    setState([this, i]()
                    {
                        dragging_index_ = i;
                        drag_delta_     = 0.0f;
                    });
                };
                gd->on_pan_update = [this](Offset delta)
                {
                    if (dragging_index_ < 0) return;
                    setState([this, delta]() { drag_delta_ += delta.y; });
                };
                gd->on_pan_end = [this]()
                {
                    if (dragging_index_ < 0) return;
                    const auto& w2 = widget();
                    const int target = computeReorderTargetIndex(
                        dragging_index_, drag_delta_, w2.item_extent,
                        static_cast<int>(w2.children.size()));
                    const int source = dragging_index_;

                    setState([this]()
                    {
                        dragging_index_ = -1;
                        drag_delta_     = 0.0f;
                    });

                    if (target != source && w2.on_reorder)
                        w2.on_reorder(source, target);
                };

                items.push_back(gd);
            }

            return std::make_shared<Column>(std::move(items));
        }

    private:
        int   dragging_index_ = -1;
        float drag_delta_     = 0.0f;
    };

    std::unique_ptr<StateBase> ReorderableListView::createState() const
    {
        return std::make_unique<ReorderableListViewState>();
    }

} // namespace systems::leal::campello_widgets
