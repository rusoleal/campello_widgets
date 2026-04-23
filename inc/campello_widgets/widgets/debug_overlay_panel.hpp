#pragma once

#include <campello_widgets/widgets/stateful_widget.hpp>

namespace systems::leal::campello_widgets
{

    /**
     * @brief A floating debug settings panel that toggles debug overlays at runtime.
     *
     * Shows switches for all DebugFlags, plus buttons to dump the widget and
     * render object trees to the console.
     *
     * Usage:
     * @code
     *   auto panel = std::make_shared<DebugOverlayPanel>();
     *   panel->child = myAppContent;
     * @endcode
     *
     * The panel renders as a floating card on top of `child`.
     */
    class DebugOverlayPanel : public StatefulWidget
    {
    public:
        WidgetRef child;

        std::unique_ptr<StateBase> createState() const override;
    };

} // namespace systems::leal::campello_widgets
