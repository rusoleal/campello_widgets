#include <campello_widgets/widgets/overflow_box.hpp>
#include <campello_widgets/ui/render_constraints_transform_box.hpp>

namespace systems::leal::campello_widgets
{

    std::shared_ptr<RenderObject> OverflowBox::createRenderObject() const
    {
        auto r       = std::make_shared<RenderConstraintsTransformBox>();
        r->alignment = alignment;

        const auto min_w = min_width, max_w = max_width, min_h = min_height, max_h = max_height;
        r->constraints_transform = [min_w, max_w, min_h, max_h](const BoxConstraints& c) -> BoxConstraints
        {
            return {
                min_w.value_or(c.min_width),  max_w.value_or(c.max_width),
                min_h.value_or(c.min_height), max_h.value_or(c.max_height),
            };
        };
        return r;
    }

    void OverflowBox::updateRenderObject(RenderObject& ro) const
    {
        auto& r      = static_cast<RenderConstraintsTransformBox&>(ro);
        r.alignment  = alignment;

        const auto min_w = min_width, max_w = max_width, min_h = min_height, max_h = max_height;
        r.constraints_transform = [min_w, max_w, min_h, max_h](const BoxConstraints& c) -> BoxConstraints
        {
            return {
                min_w.value_or(c.min_width),  max_w.value_or(c.max_width),
                min_h.value_or(c.min_height), max_h.value_or(c.max_height),
            };
        };
    }

} // namespace systems::leal::campello_widgets
