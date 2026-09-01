#include <algorithm>
#include <unordered_map>
#include <vector>
#include <campello_widgets/widgets/sliver_fixed_extent_list.hpp>
#include <campello_widgets/ui/render_sliver_fixed_extent_list.hpp>
#include <campello_widgets/widgets/render_object_element.hpp>

namespace systems::leal::campello_widgets
{

    // =========================================================================
    // SliverFixedExtentListElement — builds only items in the current visible
    // range. Direct structural copy of ListViewElement (see list_view.cpp),
    // substituting RenderSliverFixedExtentList for RenderListView.
    // =========================================================================

    class SliverFixedExtentListElement : public RenderObjectElement
    {
    public:
        explicit SliverFixedExtentListElement(std::shared_ptr<const SliverFixedExtentList> widget)
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

            renderSliverList().on_visible_range_changed = nullptr;
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
            const auto& w  = static_cast<const SliverFixedExtentList&>(*widget_);
            auto&       rv = renderSliverList();

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
            // with this build's fresh builder() output (see ListViewElement's
            // own doc for why this refresh happens every performBuild(), not
            // just on first mount).
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
        RenderSliverFixedExtentList& renderSliverList() const
        {
            return static_cast<RenderSliverFixedExtentList&>(*render_object_);
        }

        void wireCallback()
        {
            renderSliverList().on_visible_range_changed = [this]()
            {
                markNeedsBuild();
            };
        }

        std::unordered_map<int, std::shared_ptr<Element>> item_elements_;
    };

    // =========================================================================
    // SliverFixedExtentList widget
    // =========================================================================

    std::shared_ptr<Element> SliverFixedExtentList::createElement() const
    {
        return std::make_shared<SliverFixedExtentListElement>(
            std::static_pointer_cast<const SliverFixedExtentList>(shared_from_this()));
    }

    std::shared_ptr<RenderObject> SliverFixedExtentList::createRenderObject() const
    {
        auto render = std::make_shared<RenderSliverFixedExtentList>();
        render->item_count  = item_count;
        render->item_extent = item_extent;
        return render;
    }

    void SliverFixedExtentList::updateRenderObject(RenderObject& render_object) const
    {
        auto& rv = static_cast<RenderSliverFixedExtentList&>(render_object);
        if (rv.item_count != item_count || rv.item_extent != item_extent)
        {
            rv.item_count  = item_count;
            rv.item_extent = item_extent;
            rv.markNeedsLayout();
        }
    }

} // namespace systems::leal::campello_widgets
