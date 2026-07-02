#pragma once

#include <campello_widgets/widgets/multi_child_render_object_element.hpp>
#include <campello_widgets/ui/render_box.hpp>
#include <vector>

namespace systems::leal::campello_widgets
{

    class Flex;

    /**
     * @brief Element for the Flex widget.
     *
     * Overrides `syncChildRenderObjects()` to inspect each child widget for a
     * `Flexible` wrapper, extract its `flex` factor, and pass that factor to
     * `RenderFlex::insertChild()`.
     */
    class FlexElement : public MultiChildRenderObjectElement
    {
    public:
        explicit FlexElement(std::shared_ptr<const Flex> widget);

    protected:
        void syncChildRenderObjects() override;

    private:
        // What was last actually pushed into RenderFlex — compared against
        // the freshly-computed list every sync so a rebuild that changes
        // nothing about this Flex's children (identity or flex factor)
        // can skip RenderFlex::clearChildren()+insertChild() entirely,
        // instead of unconditionally re-running them (and their
        // markNeedsLayout() calls) on every single rebuild regardless of
        // whether anything changed — see RenderObjectElement::update()'s
        // doc comment for the sibling fix this complements.
        struct SyncedChild
        {
            int        index;
            RenderBox* box;  // non-owning — identity only, never dereferenced
            int        flex;
            bool operator==(const SyncedChild&) const noexcept = default;
        };
        std::vector<SyncedChild> last_synced_;
    };

} // namespace systems::leal::campello_widgets
