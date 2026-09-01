#include <campello_widgets/widgets/sliver_to_box_adapter.hpp>
#include <campello_widgets/ui/render_sliver_to_box_adapter.hpp>

namespace systems::leal::campello_widgets
{

    std::shared_ptr<RenderObject> SliverToBoxAdapter::createRenderObject() const
    {
        return std::make_shared<RenderSliverToBoxAdapter>();
    }

    void SliverToBoxAdapter::insertRenderObjectChild(
        RenderObject&              parent,
        std::shared_ptr<RenderBox> child_box) const
    {
        static_cast<RenderSliverToBoxAdapter&>(parent).setChild(std::move(child_box));
    }

    void SliverToBoxAdapter::removeRenderObjectChild(RenderObject& parent) const
    {
        static_cast<RenderSliverToBoxAdapter&>(parent).setChild(nullptr);
    }

} // namespace systems::leal::campello_widgets
