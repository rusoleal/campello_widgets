#pragma once

#include <campello_widgets/diagnostics/diagnostic_property.hpp>
#include <campello_widgets/widgets/single_child_render_object_widget.hpp>

namespace systems::leal::campello_widgets
{

    /**
     * @brief A widget that keeps its child mounted (Element/State alive,
     * including any resources it owns) but skips its layout and paint
     * entirely while offstage.
     *
     * Use this instead of `Opacity(0, child)` when the goal is to hide
     * content that should keep its state -- e.g. an inactive tab in a
     * Stack of always-mounted tabs (see ContentArea in campello_editor).
     * `Opacity` still fully repaints (and issues real GPU draw calls for)
     * its child every frame, just at alpha 0; `Offstage` costs nothing
     * per frame while hidden. See `RenderOffstage`'s doc comment for the
     * full rationale, including a real correctness bug `Opacity` has that
     * this avoids.
     *
     * Matches Flutter's `Offstage` widget.
     *
     * @code
     * auto w = std::make_shared<Offstage>();
     * w->offstage = !tab.isActive;
     * w->child    = tabContent;
     * @endcode
     */
    class Offstage : public SingleChildRenderObjectWidget
    {
    public:
        bool offstage = true;

        Offstage() = default;
        explicit Offstage(bool off, WidgetRef c = nullptr)
        {
            offstage = off;
            child    = std::move(c);
        }

        std::shared_ptr<RenderObject> createRenderObject() const override;
        void updateRenderObject(RenderObject& ro) const override;
        void debugFillProperties(DiagnosticsPropertyBuilder& properties) const override;
    };

} // namespace systems::leal::campello_widgets
