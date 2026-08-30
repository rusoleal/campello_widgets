#pragma once

#include <campello_widgets/widgets/single_child_render_object_widget.hpp>
#include <campello_widgets/ui/box_fit.hpp>
#include <campello_widgets/ui/alignment.hpp>

namespace systems::leal::campello_widgets
{

    /**
     * @brief Scales and positions its child within the available space.
     *
     * Matches Flutter's `FittedBox` widget.
     *
     * @code
     * auto w = std::make_shared<FittedBox>();
     * w->fit   = BoxFit::contain;
     * w->child = someChild;
     * @endcode
     */
    class FittedBox : public SingleChildRenderObjectWidget
    {
    public:
        BoxFit    fit       = BoxFit::contain;
        Alignment alignment = Alignment::center();

        FittedBox() = default;
        explicit FittedBox(BoxFit f, WidgetRef c = nullptr) : fit(f)
        {
            child = std::move(c);
        }

        std::shared_ptr<RenderObject> createRenderObject() const override;
        void updateRenderObject(RenderObject& ro) const override;
    };

} // namespace systems::leal::campello_widgets
