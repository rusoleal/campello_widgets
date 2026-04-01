#include <campello_widgets/widgets/page_view.hpp>
#include <campello_widgets/ui/render_page_view.hpp>

namespace systems::leal::campello_widgets
{

    std::shared_ptr<RenderObject> PageView::createRenderObject() const
    {
        auto rv = std::make_shared<RenderPageView>();
        rv->scroll_direction = scroll_direction;
        rv->on_page_changed  = on_page_changed;
        if (controller) rv->setController(controller);
        return rv;
    }

    void PageView::updateRenderObject(RenderObject& ro) const
    {
        auto& rv = static_cast<RenderPageView&>(ro);
        rv.scroll_direction = scroll_direction;
        rv.on_page_changed  = on_page_changed;
        if (controller) rv.setController(controller);
    }

    void PageView::insertRenderObjectChild(
        RenderObject&             parent,
        std::shared_ptr<RenderBox> child_box,
        int                        index) const
    {
        static_cast<RenderPageView&>(parent).insertChild(std::move(child_box), index);
    }

    void PageView::clearRenderObjectChildren(RenderObject& parent) const
    {
        static_cast<RenderPageView&>(parent).clearChildren();
    }

} // namespace systems::leal::campello_widgets
