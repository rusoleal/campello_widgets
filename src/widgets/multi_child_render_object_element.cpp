#include <campello_widgets/widgets/multi_child_render_object_element.hpp>
#include <campello_widgets/ui/render_box.hpp>
#include <campello_widgets/ui/key.hpp>
#include <unordered_map>

namespace systems::leal::campello_widgets
{

    MultiChildRenderObjectElement::MultiChildRenderObjectElement(
        std::shared_ptr<const MultiChildRenderObjectWidget> widget)
        : RenderObjectElement(std::move(widget))
    {
    }

    void MultiChildRenderObjectElement::update(WidgetRef new_widget)
    {
        RenderObjectElement::update(std::move(new_widget));
        rebuild();
    }

    void MultiChildRenderObjectElement::unmount()
    {
        for (auto& child : child_elements_)
            if (child) child->unmount();
        child_elements_.clear();
        RenderObjectElement::unmount();
    }

    void MultiChildRenderObjectElement::performBuild()
    {
        const auto& w = static_cast<const MultiChildRenderObjectWidget&>(*widget_);

        // ------------------------------------------------------------------
        // Key-aware reconciliation
        //
        // Partition old children into two pools:
        //   • keyed   — matched by key equality (order-independent)
        //   • unkeyed — matched positionally (original behaviour)
        // ------------------------------------------------------------------

        std::unordered_map<Key*, std::shared_ptr<Element>,
                           KeyPtrHash, KeyPtrEqual> old_keyed;
        std::vector<std::shared_ptr<Element>> old_unkeyed;

        for (auto& e : child_elements_)
        {
            if (!e) continue;
            Key* k = e->widget().key.get();
            if (k) old_keyed[k] = e;
            else   old_unkeyed.push_back(e);
        }

        std::vector<std::shared_ptr<Element>> new_elements;
        new_elements.reserve(w.children.size());
        std::size_t unkeyed_pos = 0;

        for (const auto& new_widget : w.children)
        {
            Key* new_key = new_widget->key.get();

            if (new_key)
            {
                // Try to reuse an old element with a matching key
                auto it = old_keyed.find(new_key);
                if (it != old_keyed.end())
                {
                    auto old_el = it->second;
                    old_keyed.erase(it);

                    if (old_el->widget().widgetType() == new_widget->widgetType())
                        old_el->update(new_widget);
                    else
                    {
                        old_el->unmount();
                        old_el = new_widget->createElement();
                        old_el->mount(this);
                    }
                    new_elements.push_back(std::move(old_el));
                }
                else
                {
                    // No old element with this key — create fresh
                    auto el = new_widget->createElement();
                    el->mount(this);
                    new_elements.push_back(std::move(el));
                }
            }
            else
            {
                // Unkeyed: positional match
                std::shared_ptr<Element> existing =
                    (unkeyed_pos < old_unkeyed.size())
                        ? old_unkeyed[unkeyed_pos++]
                        : nullptr;
                new_elements.push_back(updateChild(std::move(existing), new_widget, this));
            }
        }

        // Unmount any old keyed elements that were not reused
        for (auto& [k, e] : old_keyed)
            if (e) e->unmount();

        // Unmount any remaining positional elements
        for (; unkeyed_pos < old_unkeyed.size(); ++unkeyed_pos)
            if (old_unkeyed[unkeyed_pos]) old_unkeyed[unkeyed_pos]->unmount();

        child_elements_ = std::move(new_elements);
        syncChildRenderObjects();
    }

    void MultiChildRenderObjectElement::syncChildRenderObjects()
    {
        const auto& w = static_cast<const MultiChildRenderObjectWidget&>(*widget_);
        w.clearRenderObjectChildren(*render_object_);

        for (int i = 0; i < static_cast<int>(child_elements_.size()); ++i)
        {
            if (!child_elements_[i]) continue;

            auto* child_roe =
                child_elements_[i]->findDescendantRenderObjectElement();
            if (!child_roe) continue;

            auto child_box =
                std::dynamic_pointer_cast<RenderBox>(child_roe->sharedRenderObject());
            if (child_box)
                w.insertRenderObjectChild(*render_object_, std::move(child_box), i);
        }
    }

} // namespace systems::leal::campello_widgets
