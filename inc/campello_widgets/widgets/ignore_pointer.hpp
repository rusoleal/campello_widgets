#pragma once

#include <campello_widgets/widgets/single_child_render_object_widget.hpp>

namespace systems::leal::campello_widgets
{

    /**
     * @brief Removes its child subtree from hit-testing while `ignoring`.
     *
     * Events fall through to whatever is behind this widget. Layout and
     * paint are unaffected.
     *
     * @code
     * auto w = std::make_shared<IgnorePointer>();
     * w->ignoring = isBusy;
     * w->child    = someInteractiveChild;
     * @endcode
     */
    class IgnorePointer : public SingleChildRenderObjectWidget
    {
    public:
        bool ignoring = true;

        IgnorePointer() = default;
        explicit IgnorePointer(bool ig, WidgetRef c = nullptr)
        {
            ignoring = ig;
            child    = std::move(c);
        }

        std::shared_ptr<RenderObject> createRenderObject() const override;
        void updateRenderObject(RenderObject& ro) const override;
    };

} // namespace systems::leal::campello_widgets
