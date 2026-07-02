#pragma once

#include <functional>
#include <utility>

namespace systems::leal::campello_widgets
{

    /**
     * @brief Ensures only one handler acts on a given discrete pointer
     * signal (currently: scroll/wheel events), even though
     * `PointerDispatcher::dispatch()` calls every handler along the
     * hit-test path.
     *
     * Unlike down/move/up gestures — which are inherently multi-frame and
     * mediated by `GestureArenaManager` via pan-slop-based competition —
     * a scroll event is instantaneous: there's no "movement over time" to
     * arbitrate on. Nested scrollables with different axes (e.g. a
     * horizontal `ListView` inside a vertical page) would otherwise both
     * apply their own axis's delta to the very same scroll event, with no
     * coordination at all — visible as erratic/jumpy scrolling, since two
     * unrelated scroll positions shift in lockstep with every event.
     *
     * Extends Flutter's `PointerSignalResolver` with an axis-dominance
     * tier: a candidate may register as `dominant` (its own scroll axis
     * is the larger component of this event) or not. `resolve()` prefers
     * the first *dominant* registration — deepest-first, so an inner
     * scrollable whose axis genuinely matches the swipe still wins over
     * an outer one — but falls back to the very first registration
     * (dominant or not) if nothing claimed dominance. That fallback
     * matters for the common case of a single scrollable with no nested
     * competitor: a trackpad swipe's cross-axis component is rarely
     * exactly zero, so requiring dominance unconditionally would drop
     * some fraction of scroll events outright (felt as stutter/jumpiness)
     * even though there was no ancestor/sibling to arbitrate against.
     */
    class PointerSignalResolver
    {
    public:
        /** @brief Registers a candidate handler; `dominant` marks this as the event's likely intended axis. */
        void registerHandler(std::function<void()> handler, bool dominant) noexcept
        {
            if (!fallback_handler_) fallback_handler_ = handler;
            if (dominant && !dominant_handler_) dominant_handler_ = std::move(handler);
        }

        /** @brief Runs the winning handler (if any) and clears state for the next event. */
        void resolve()
        {
            if (dominant_handler_) dominant_handler_();
            else if (fallback_handler_) fallback_handler_();
            dominant_handler_ = nullptr;
            fallback_handler_ = nullptr;
        }

    private:
        std::function<void()> dominant_handler_;
        std::function<void()> fallback_handler_;
    };

} // namespace systems::leal::campello_widgets
