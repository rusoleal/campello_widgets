#pragma once

#include <campello_widgets/widgets/single_child_render_object_widget.hpp>

namespace systems::leal::campello_widgets
{

    /**
     * @brief Prevents its subtree from receiving pointer events while
     * `absorbing`, but still claims (and thus blocks) the hit itself.
     *
     * Unlike `IgnorePointer`, an event over this widget's bounds does not
     * fall through to whatever is behind it -- it's simply swallowed.
     *
     * @code
     * auto w = std::make_shared<AbsorbPointer>();
     * w->absorbing = isBusy;
     * w->child     = someInteractiveChild;
     * @endcode
     */
    class AbsorbPointer : public SingleChildRenderObjectWidget
    {
    public:
        bool absorbing = true;

        AbsorbPointer() = default;
        explicit AbsorbPointer(bool ab, WidgetRef c = nullptr)
        {
            absorbing = ab;
            child     = std::move(c);
        }

        std::shared_ptr<RenderObject> createRenderObject() const override;
        void updateRenderObject(RenderObject& ro) const override;
    };

} // namespace systems::leal::campello_widgets
