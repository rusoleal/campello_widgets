#include <campello_widgets/widgets/sliver_persistent_header.hpp>
#include <campello_widgets/ui/render_sliver_persistent_header.hpp>

namespace systems::leal::campello_widgets
{

    std::shared_ptr<RenderObject> SliverPersistentHeader::createRenderObject() const
    {
        auto r = std::make_shared<RenderSliverPersistentHeader>();
        r->min_extent = min_extent;
        r->max_extent = max_extent;
        return r;
    }

    void SliverPersistentHeader::updateRenderObject(RenderObject& render_object) const
    {
        auto& r = static_cast<RenderSliverPersistentHeader&>(render_object);
        if (r.min_extent != min_extent || r.max_extent != max_extent)
        {
            r.min_extent = min_extent;
            r.max_extent = max_extent;
            r.markNeedsLayout();
        }
    }

    void SliverPersistentHeader::insertRenderObjectChild(
        RenderObject&              parent,
        std::shared_ptr<RenderBox> child_box) const
    {
        static_cast<RenderSliverPersistentHeader&>(parent).setChild(std::move(child_box));
    }

    void SliverPersistentHeader::removeRenderObjectChild(RenderObject& parent) const
    {
        static_cast<RenderSliverPersistentHeader&>(parent).setChild(nullptr);
    }

} // namespace systems::leal::campello_widgets
