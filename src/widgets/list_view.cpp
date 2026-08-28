#include <algorithm>
#include <unordered_map>
#include <vector>
#include <campello_widgets/widgets/list_view.hpp>
#include <campello_widgets/ui/render_list_view.hpp>
#include <campello_widgets/widgets/render_object_element.hpp>

namespace systems::leal::campello_widgets
{

    // =========================================================================
    // ListViewElement — builds only items in the current visible range
    // =========================================================================

    class ListViewElement : public RenderObjectElement
    {
    public:
        explicit ListViewElement(std::shared_ptr<const ListView> widget)
            : RenderObjectElement(std::move(widget))
        {}

        void mount(Element* parent) override
        {
            RenderObjectElement::mount(parent);
            wireCallback();
        }

        void unmount() override
        {
            for (auto& [idx, elem] : item_elements_)
                if (elem) elem->unmount();
            item_elements_.clear();

            renderListView().on_visible_range_changed = nullptr;
            RenderObjectElement::unmount();
        }

        void update(WidgetRef new_widget) override
        {
            RenderObjectElement::update(std::move(new_widget));
            wireCallback();
        }

    protected:
        void performBuild() override
        {
            const auto& w  = static_cast<const ListView&>(*widget_);
            auto&       rv = renderListView();

            if (!w.builder || w.item_count <= 0 || w.item_extent <= 0.0f) return;

            const int first = std::max(0, rv.firstVisibleIndex() - 1);
            const int last  = std::min(w.item_count - 1, rv.lastVisibleIndex() + 1);

            // Unmount items that scrolled out of view.
            std::vector<int> to_remove;
            for (auto& [idx, elem] : item_elements_)
            {
                if (idx < first || idx > last)
                {
                    if (elem) elem->unmount();
                    rv.removeItemBox(idx);
                    to_remove.push_back(idx);
                }
            }
            for (int idx : to_remove) item_elements_.erase(idx);

            // Mount items newly in view, and refresh already-mounted ones
            // with this build's fresh builder() output. A ListView whose
            // row *content* (not just count/order) can change for an index
            // that stays continuously visible needs this refresh every
            // performBuild(), not just on first mount -- skipping
            // already-mounted indices (the old behavior here) left rows
            // frozen at whatever content existed the first time that index
            // ever became visible, confirmed via a real bug: a diagnostics
            // panel whose row text kept changing while continuously
            // visible showed permanently stale text per row. updateChild()
            // itself already does the right thing either way -- reuses the
            // element in place via child->update() when the widget type/
            // key match (the common case here, cheap), or unmounts and
            // recreates when they don't -- so passing the existing element
            // through every time is both correct and no more expensive
            // than the mount-only path for indices that didn't change.
            for (int i = first; i <= last; ++i)
            {
                WidgetRef item_widget = w.builder(*this, i);
                auto it = item_elements_.find(i);
                auto existing = (it != item_elements_.end()) ? it->second : nullptr;
                auto elem = updateChild(existing, item_widget, this);
                if (elem)
                {
                    auto* roe = elem->findDescendantRenderObjectElement();
                    if (roe)
                    {
                        auto box = std::dynamic_pointer_cast<RenderBox>(
                            roe->sharedRenderObject());
                        if (box) rv.setItemBox(i, std::move(box));
                    }
                    item_elements_[i] = std::move(elem);
                }
                else if (it != item_elements_.end())
                {
                    item_elements_.erase(it);
                }
            }
        }

    private:
        RenderListView& renderListView() const
        {
            return static_cast<RenderListView&>(*render_object_);
        }

        void wireCallback()
        {
            renderListView().on_visible_range_changed = [this]()
            {
                markNeedsBuild();
            };
        }

        std::unordered_map<int, std::shared_ptr<Element>> item_elements_;
    };

    // =========================================================================
    // ListView widget
    // =========================================================================

    std::shared_ptr<Element> ListView::createElement() const
    {
        return std::make_shared<ListViewElement>(
            std::static_pointer_cast<const ListView>(shared_from_this()));
    }

    std::shared_ptr<RenderObject> ListView::createRenderObject() const
    {
        auto render = std::make_shared<RenderListView>();
        render->scroll_axis = scroll_axis;
        render->item_count  = item_count;
        render->item_extent = item_extent;
        render->setController(controller);
        render->setPhysics(physics);
        return render;
    }

    void ListView::updateRenderObject(RenderObject& render_object) const
    {
        auto& rv = static_cast<RenderListView&>(render_object);
        if (rv.scroll_axis != scroll_axis || rv.item_count != item_count ||
            rv.item_extent != item_extent)
        {
            rv.scroll_axis = scroll_axis;
            rv.item_count  = item_count;
            rv.item_extent = item_extent;
            rv.markNeedsLayout();
        }
        rv.setController(controller);
        rv.setPhysics(physics);
    }

} // namespace systems::leal::campello_widgets
