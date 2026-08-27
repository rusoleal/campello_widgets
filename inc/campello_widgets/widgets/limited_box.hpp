#pragma once

#include <limits>
#include <campello_widgets/widgets/single_child_render_object_widget.hpp>
#include <campello_widgets/ui/render_limited_box.hpp>

namespace systems::leal::campello_widgets
{

    /**
     * @brief Limits an otherwise-unbounded incoming constraint axis to
     * `max_width`/`max_height`; passes an already-bounded axis through
     * unchanged. See `RenderLimitedBox`'s doc comment for the mechanism.
     *
     * @code
     * // "As large as possible when bounded, zero when not" -- Container's
     * // own empty-box fallback:
     * auto filler = std::make_shared<LimitedBox>(0.0f, 0.0f,
     *     std::make_shared<ConstrainedBox>(BoxConstraints::expand()));
     * @endcode
     */
    class LimitedBox : public SingleChildRenderObjectWidget
    {
    public:
        float max_width  = std::numeric_limits<float>::infinity();
        float max_height = std::numeric_limits<float>::infinity();

        LimitedBox() = default;
        explicit LimitedBox(float w, float h, WidgetRef c = nullptr)
            : max_width(w), max_height(h)
        {
            child = std::move(c);
        }

        std::shared_ptr<RenderObject> createRenderObject() const override;
        void updateRenderObject(RenderObject& ro) const override;
    };

} // namespace systems::leal::campello_widgets
