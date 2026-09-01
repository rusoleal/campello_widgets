#pragma once

#include <functional>
#include <memory>
#include <campello_widgets/widgets/stateful_widget.hpp>
#include <campello_widgets/ui/scroll_controller.hpp>

namespace systems::leal::campello_widgets
{

    /**
     * @brief Pull-to-refresh: wraps a scrollable `child`, revealing a themed
     * spinner as the user pulls past the top boundary and firing `on_refresh`
     * once released past `trigger_distance`.
     *
     * Matches Flutter's Material-only `RefreshIndicator`, but themed across
     * all four sibling DesignSystem implementations here (see
     * DesignSystem::buildRefreshIndicator()) rather than being tied to one.
     *
     * Composition and explicit-controller wiring mirror Scrollbar exactly —
     * `controller` must be the same ScrollController passed to `child`
     * (SingleChildScrollView or ListView; GridView/PageView/TreeView are not
     * wired to the underlying overscroll signal yet, see
     * ScrollController::addOverscrollListener()'s doc).
     *
     * @code
     * auto ctrl = std::make_shared<ScrollController>();
     * auto ri = std::make_shared<RefreshIndicator>();
     * ri->controller = ctrl;
     * ri->child = std::make_shared<ListView>(...); // must share `ctrl`
     * ri->on_refresh = [](std::function<void()> done) {
     *     // do async work, then:
     *     done();
     * };
     * @endcode
     */
    class RefreshIndicator : public StatefulWidget
    {
    public:
        std::shared_ptr<ScrollController> controller;
        WidgetRef                         child;
        std::function<void(std::function<void()> done)> on_refresh;
        float                             trigger_distance = 80.0f;

        RefreshIndicator() = default;
        RefreshIndicator(std::shared_ptr<ScrollController> c, WidgetRef ch)
            : controller(std::move(c)), child(std::move(ch))
        {
        }

        std::unique_ptr<StateBase> createState() const override;
    };

} // namespace systems::leal::campello_widgets
