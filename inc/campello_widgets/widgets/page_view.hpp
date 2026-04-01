#pragma once

#include <campello_widgets/widgets/multi_child_render_object_widget.hpp>
#include <campello_widgets/ui/axis.hpp>

#include <functional>
#include <memory>
#include <vector>

namespace systems::leal::campello_widgets
{

    class RenderPageView;

    // =========================================================================
    // PageController
    // =========================================================================

    /**
     * @brief Programmatic controller for a PageView.
     *
     * Attach to a PageView via `page_view->controller = controller`.
     */
    class PageController
    {
    public:
        int initial_page = 0;

        /** @brief Jumps to the given page without animation. */
        void jumpToPage(int page);

        /** @brief Returns the current page index, or initial_page if not attached. */
        int currentPage() const;

        // Internal — called by RenderPageView.
        void attach(RenderPageView* rv);
        void detach();

    private:
        RenderPageView* render_ = nullptr;
    };

    // =========================================================================
    // PageView
    // =========================================================================

    /**
     * @brief A scrollable widget that paginates its children along an axis.
     *
     * Each child fills the entire viewport. The user swipes left/right (or
     * up/down) to navigate between pages. Snaps to the nearest page on release.
     *
     * @code
     * auto pv = std::make_shared<PageView>();
     * pv->children = {pageA, pageB, pageC};
     * pv->on_page_changed = [](int page){ current = page; };
     * @endcode
     */
    class PageView : public MultiChildRenderObjectWidget
    {
    public:
        std::shared_ptr<PageController>  controller;
        Axis                             scroll_direction = Axis::horizontal;
        std::function<void(int)>         on_page_changed;
        bool                             reverse = false;

        PageView() = default;

        std::shared_ptr<RenderObject> createRenderObject() const override;
        void updateRenderObject(RenderObject& ro) const override;

        void insertRenderObjectChild(
            RenderObject&             parent,
            std::shared_ptr<RenderBox> child_box,
            int                        index) const override;

        void clearRenderObjectChildren(RenderObject& parent) const override;
    };

} // namespace systems::leal::campello_widgets
