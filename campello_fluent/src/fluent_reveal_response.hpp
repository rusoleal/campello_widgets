#pragma once

#include <campello_widgets/widgets/stateful_widget.hpp>
#include <campello_widgets/ui/color.hpp>
#include <campello_widgets/ui/focus_node.hpp>

#include <functional>
#include <memory>

namespace systems::leal::campello_widgets
{

    /**
     * @brief Fluent "Reveal Highlight" press/hover feedback.
     *
     * Shared by every interactive FluentDesignSystem::buildXxx() builder
     * instead of each wrapping its content in a bare GestureDetector --
     * mirrors WinUI's default Reveal-styled controls (Button,
     * ListViewItem, ...). Unlike Material's ripple or Cupertino's opacity
     * fade, Fluent's Reveal is a background tint + light border glow that
     * fades in on hover and strengthens further on press -- no expanding
     * circle, no whole-control opacity change.
     *
     * Clips its overlay (not the content) to border_radius, so it respects
     * the same shape as the content it wraps without touching that
     * content's own rendering -- see the identical MaterialInkResponse
     * comment for why clipping the content itself is wrong (it would cut
     * off a centered border stroke's natural outward bleed).
     *
     * No feedback at all when on_tap is null -- matches a disabled
     * control, which callers already additionally fade via Opacity same
     * as before.
     */
    class FluentRevealResponse : public StatefulWidget
    {
    public:
        WidgetRef              child;
        std::function<void()>  on_tap;
        float                  border_radius = 0.0f;
        Color                  reveal_color; // tint/border base -- usually the accent or on-surface color

        // Keyboard focus plumbing -- forwarded to the internal
        // GestureDetector with focusable=true (every RevealResponse wraps
        // a real control). Not yet painted -- see RenderGestureDetector's
        // on_focus_change doc; a focus-ring visual is a separate follow-up.
        std::shared_ptr<FocusNode> focus_node;
        bool                       autofocus = false;

        std::unique_ptr<StateBase> createState() const override;
    };

} // namespace systems::leal::campello_widgets
