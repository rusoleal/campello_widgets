#pragma once

#include <campello_widgets/widgets/stateless_widget.hpp>
#include <campello_widgets/widgets/stateful_widget.hpp>
#include <campello_widgets/widgets/inherited_widget.hpp>
#include <campello_widgets/ui/color.hpp>

#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace systems::leal::campello_widgets
{

    class TabControllerState;

    // =========================================================================
    // TabScope — InheritedWidget that propagates the active tab index
    // =========================================================================

    /**
     * @brief InheritedWidget placed by DefaultTabController above the subtree.
     *
     * Descendants (TabBar, TabBarView) look up the scope via:
     * @code
     * const TabScope* scope = ctx.dependOnInheritedWidgetOfExactType<TabScope>();
     * @endcode
     */
    class TabScope : public InheritedWidget
    {
    public:
        int                  index  = 0;
        int                  length = 0;
        TabControllerState*  state  = nullptr;   ///< Back-pointer to change the index.

        bool updateShouldNotify(const InheritedWidget& old) const override;

        /** @brief Looks up the nearest TabScope; returns nullptr if none. */
        static const TabScope* of(BuildContext& ctx);
    };

    // =========================================================================
    // DefaultTabController — creates and owns a tab controller
    // =========================================================================

    /**
     * @brief Wraps a subtree with a tab controller.
     *
     * Place DefaultTabController above TabBar and TabBarView in the widget tree.
     *
     * @code
     * auto ctrl = std::make_shared<DefaultTabController>();
     * ctrl->length = 3;
     * ctrl->child  = Column::create({tabBar, tabBarView});
     * @endcode
     */
    class DefaultTabController : public StatefulWidget
    {
    public:
        int       length        = 0;
        int       initial_index = 0;
        WidgetRef child;

        DefaultTabController() = default;
        explicit DefaultTabController(int len, WidgetRef c = nullptr)
            : length(len), child(std::move(c))
        {}
        explicit DefaultTabController(
            int len,
            int init_idx,
            WidgetRef c = nullptr)
            : length(len), initial_index(init_idx), child(std::move(c))
        {}

        std::unique_ptr<StateBase> createState() const override;
    };

    // =========================================================================
    // Tab — descriptor for a single tab label
    // =========================================================================

    struct Tab
    {
        std::string text;
        WidgetRef   icon;
        WidgetRef   child;   ///< Fully custom label (overrides text and icon).
    };

    // =========================================================================
    // TabBar — row of tab labels with an animated indicator
    // =========================================================================

    /**
     * @brief A horizontal row of tab labels.
     *
     * TabBar must be a descendant of DefaultTabController (or another widget
     * that provides a TabScope inherited widget).
     *
     * @code
     * auto bar = std::make_shared<TabBar>();
     * bar->tabs = {Tab{"One"}, Tab{"Two"}, Tab{"Three"}};
     * @endcode
     */
    class TabBar : public StatelessWidget
    {
    public:
        std::vector<Tab> tabs;
        Color            indicator_color         = Color::fromRGBA(0.098f, 0.463f, 0.824f, 1.0f);
        Color            label_color             = Color::fromRGBA(0.098f, 0.463f, 0.824f, 1.0f);
        Color            unselected_label_color  = Color::fromRGBA(0.0f, 0.0f, 0.0f, 0.54f);
        float            indicator_weight        = 2.0f;
        Color            background_color        = Color::transparent();

        TabBar() = default;
        explicit TabBar(std::vector<Tab> t)
            : tabs(std::move(t))
        {}

        WidgetRef build(BuildContext& ctx) const override;
    };

    // =========================================================================
    // TabBarView — displays the child widget for the current tab
    // =========================================================================

    /**
     * @brief Shows the child corresponding to the active tab.
     *
     * TabBarView must be a descendant of DefaultTabController. It rebuilds
     * automatically whenever the active tab changes.
     *
     * @code
     * auto view = std::make_shared<TabBarView>();
     * view->children = {pageOne, pageTwo, pageThree};
     * @endcode
     */
    class TabBarView : public StatelessWidget
    {
    public:
        std::vector<WidgetRef> children;
        double                 animation_duration_ms = 200.0;

        TabBarView() = default;
        explicit TabBarView(std::vector<WidgetRef> ch)
            : children(std::move(ch))
        {}

        WidgetRef build(BuildContext& ctx) const override;
    };

} // namespace systems::leal::campello_widgets
