#include <campello_widgets/widgets/limited_box.hpp>

namespace systems::leal::campello_widgets
{

    std::shared_ptr<RenderObject> LimitedBox::createRenderObject() const
    {
        auto ro        = std::make_shared<RenderLimitedBox>();
        ro->max_width  = max_width;
        ro->max_height = max_height;
        return ro;
    }

    void LimitedBox::updateRenderObject(RenderObject& ro) const
    {
        auto& rlb = static_cast<RenderLimitedBox&>(ro);
        if (rlb.max_width == max_width && rlb.max_height == max_height) return;
        rlb.max_width  = max_width;
        rlb.max_height = max_height;
        rlb.markNeedsLayout();
    }

} // namespace systems::leal::campello_widgets
