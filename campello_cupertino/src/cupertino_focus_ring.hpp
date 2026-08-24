#pragma once

#include <campello_widgets/widgets/stateful_widget.hpp>
#include <campello_widgets/ui/color.hpp>
#include <campello_widgets/ui/focus_node.hpp>

#include <functional>
#include <memory>

namespace systems::leal::campello_widgets
{

    /**
     * @brief Cupertino keyboard-focus ring.
     *
     * Cupertino has no ripple (Material) or Reveal (Fluent) press/hover
     * feedback of its own yet -- buildButton() still wraps content in a
     * bare GestureDetector+Opacity. This widget only adds the focus-ring
     * half of the contract, mirroring CupertinoButton's real
     * FocusableActionDetector + DecoratedBox-with-Border-when-focused
     * design: a solid 3.5pt stroke in the focus color, only while
     * `FocusManager::highlightMode() == FocusHighlightMode::keyboard`
     * (never for a mouse click) -- see focus_manager.hpp's
     * FocusHighlightMode doc comment.
     *
     * Clips its overlay (not the content) to border_radius, same as
     * MaterialInkResponse/FluentRevealResponse -- see their identical
     * comment for why clipping the content itself is wrong.
     *
     * No feedback at all when on_tap is null -- matches a disabled
     * control, which callers already additionally fade via Opacity same
     * as before.
     */
    class CupertinoFocusRing : public StatefulWidget
    {
    public:
        WidgetRef              child;
        std::function<void()>  on_tap;
        float                  border_radius = 0.0f;
        Color                  focus_color;

        std::shared_ptr<FocusNode> focus_node;
        bool                        autofocus = false;

        std::unique_ptr<StateBase> createState() const override;
    };

} // namespace systems::leal::campello_widgets
