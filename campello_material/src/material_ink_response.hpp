#pragma once

#include <campello_widgets/widgets/stateful_widget.hpp>
#include <campello_widgets/ui/color.hpp>
#include <campello_widgets/ui/focus_node.hpp>

#include <functional>
#include <memory>

namespace systems::leal::campello_widgets
{

    /**
     * @brief Material "state layer" + ink-splash press/hover feedback.
     *
     * Shared by every interactive MaterialDesignSystem::buildXxx() builder
     * instead of each wrapping its content in a bare GestureDetector --
     * mirrors Flutter's InkWell/InkResponse: a low-alpha overlay tint on
     * hover, an expanding tinted circle growing from the tap-down point
     * while pressed. Clips to border_radius so the effect respects the
     * same shape as the content it wraps (button pill, card corners, ...).
     *
     * No feedback at all (no hover cursor, no overlay, no ripple) when
     * on_tap is null -- matches a disabled/non-interactive control, which
     * callers already additionally fade via Opacity same as before.
     */
    class MaterialInkResponse : public StatefulWidget
    {
    public:
        WidgetRef              child;
        std::function<void()>  on_tap;
        float                  border_radius = 0.0f;
        Color                  overlay_color; // tint base -- usually the content's own foreground/on-color

        // Keyboard focus plumbing -- forwarded to the internal
        // GestureDetector with focusable=true (every InkResponse wraps a
        // real control). Not yet painted -- see RenderGestureDetector's
        // on_focus_change doc; a focus-ring visual is a separate follow-up.
        std::shared_ptr<FocusNode> focus_node;
        bool                       autofocus = false;

        std::unique_ptr<StateBase> createState() const override;
    };

} // namespace systems::leal::campello_widgets
