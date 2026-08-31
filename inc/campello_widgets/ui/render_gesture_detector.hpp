#pragma once

#include <cstdint>
#include <functional>
#include <memory>
#include <campello_widgets/ui/render_box.hpp>
#include <campello_widgets/ui/pointer_event.hpp>
#include <campello_widgets/ui/focus_node.hpp>
#include <campello_widgets/ui/tap_gesture_recognizer.hpp>
#include <campello_widgets/ui/long_press_gesture_recognizer.hpp>
#include <campello_widgets/ui/drag_gesture_recognizer.hpp>
#include <campello_widgets/ui/scale_gesture_recognizer.hpp>
#include <campello_widgets/ui/force_press_gesture_recognizer.hpp>
#include <campello_widgets/ui/gesture_details.hpp>

namespace systems::leal::campello_widgets
{

    /**
     * @brief RenderBox that recognises tap, double-tap, long-press, pan, and scroll.
     *
     * On construction, registers itself with PointerDispatcher::activeDispatcher()
     * for both pointer events and per-frame ticks (used for long-press timing).
     * On destruction, deregisters both. The active dispatcher must be set before
     * any GestureDetector widget is mounted.
     *
     * Each recognised gesture family is its own GestureRecognizer
     * (tap_recognizer_/long_press_recognizer_/pan_recognizer_), independently
     * joining the pointer's gesture arena and competing there both against
     * each other and against ancestor/descendant recognizers (e.g. an
     * enclosing scrollable) -- mirroring Flutter's real GestureDetector
     * rather than one hard-coded state machine. Slop thresholds are
     * device-aware (see gesture_constants.hpp): touch input tolerates more
     * incidental movement than a mouse.
     *
     * Recognised gestures:
     *  - on_tap        — down + up within hit slop
     *  - on_tap_down/on_tap_up/on_tap_cancel — granular tap timing points
     *                    (down eagerly, up alongside on_tap for a *single*
     *                    tap only, cancel when a pending tap is abandoned).
     *                    See TapGestureRecognizer's doc comment.
     *  - on_secondary_tap / on_tertiary_tap_* — same shape for a right-click
     *                    / middle-click, backed by two more
     *                    PointerButton-filtered TapGestureRecognizer
     *                    instances. No secondary double-tap and no plain
     *                    on_tertiary_tap, matching Flutter.
     *  - on_double_tap — two qualifying taps within kDoubleTapMs (300 ms) and hit slop
     *  - on_long_press — finger held ≥ kLongPressMs (500 ms) without moving past hit slop
     *  - on_long_press_down/cancel/start/move_update/end — granular
     *                    long-press timing points, including drag velocity
     *                    on release if the press was held and moved. See
     *                    LongPressGestureRecognizer's doc comment.
     *  - on_pan_*, on_horizontal_drag_*, on_vertical_drag_*, on_scale_* —
     *                    four mutually exclusive gesture families (at most
     *                    one may have any callback set — debug-asserted in
     *                    onPointerEvent(), mirroring Flutter's own
     *                    GestureDetector assertion). The three drag
     *                    families each fire down (eagerly, before the slop
     *                    gate) / start (once slop is exceeded and the
     *                    arena is won) / update (every subsequent move) /
     *                    end (on release, carrying release velocity from a
     *                    VelocityTracker). Pan is omnidirectional;
     *                    horizontal/vertical only count movement along
     *                    their own axis toward exceeding slop. See
     *                    drag_gesture_recognizer.hpp. on_scale_* tracks any
     *                    number of concurrent pointers for pinch/rotate,
     *                    degrading to pan-like single-finger behavior (with
     *                    scale pinned at 1.0) when only one pointer is
     *                    down — see scale_gesture_recognizer.hpp.
     *  - on_force_press_start/peak/update/end — staged pressure for a
     *                    stylus pointer only (see
     *                    ForcePressGestureRecognizer's doc comment for why
     *                    other device kinds can't support this, and why
     *                    this recognizer deliberately never joins the
     *                    gesture arena — it coexists with tap/pan rather
     *                    than competing with them). Thresholds are
     *                    force_press_start_pressure/force_press_peak_pressure
     *                    (defaults 0.4/0.85).
     *  - behavior      — HitTestBehavior (opaque/translucent/deferToChild),
     *                    see hit_test.hpp. Defaults to opaque.
     *  - on_scroll     — scroll-wheel / trackpad scroll; called with Offset{dx, dy}
     *  - on_press_change — fires with true immediately on pointer-down
     *                    (same eager timing as on_pan_down — before slop or
     *                    arena resolution, matching Flutter's InkResponse
     *                    highlighting on tap-down rather than waiting for
     *                    tap-up), and with false on release, cancel, losing
     *                    the gesture arena to a competing recognizer (e.g.
     *                    an ancestor ScrollView), or the gesture
     *                    reclassifying as a pan. Intended for press-state
     *                    visual feedback (opacity fade, state-layer
     *                    overlay, ripple) — a plain on_tap alone can't
     *                    express "still held down" or "was released
     *                    without becoming a tap".
     *
     * Keyboard focus (opt-in via `focusable`): when true, this detector
     * registers a FocusNode with the active FocusManager (an externally
     * supplied one via `focus_node`, or a lazily-created internal one so
     * every focusable control still participates in Tab traversal without
     * its caller having to own a FocusNode) — mirroring how TextField
     * manages its own FocusNode directly rather than going through the
     * separate `Focus` widget. Space/Enter while focused fires `on_tap`,
     * matching Flutter's ActivateIntent on a focused button. A successful
     * tap also requests focus for this detector, same as clicking a
     * Flutter button focuses it. `on_focus_change` fires whenever this
     * detector's focus state flips, for a caller to drive a focus-ring
     * visual (not implemented here — this is plumbing only; painting a
     * ring per theme is a separate follow-up).
     *
     * `focusable` defaults to false: plenty of GestureDetector usages wrap
     * non-control content (tap-absorption, drag handles, scroll rows) that
     * must NOT suddenly become Tab stops. Only opt in for actual
     * button-like controls.
     *
     * Layout and paint pass through to the single child (inherited from RenderBox).
     */
    class RenderGestureDetector : public RenderBox
    {
    public:
        std::function<void()>                    on_tap;
        std::function<void()>                    on_double_tap;
        std::function<void()>                    on_long_press;

        std::function<void(TapDownDetails)>      on_tap_down;
        std::function<void(TapUpDetails)>        on_tap_up;
        std::function<void()>                    on_tap_cancel;

        /// Right-click. No secondary double-tap -- Flutter has none either.
        std::function<void()>                    on_secondary_tap;
        std::function<void(TapDownDetails)>      on_secondary_tap_down;
        std::function<void(TapUpDetails)>        on_secondary_tap_up;
        std::function<void()>                    on_secondary_tap_cancel;

        /// Middle-click. No plain on_tertiary_tap -- Flutter has none
        /// either, only the down/up/cancel granular callbacks.
        std::function<void(TapDownDetails)>      on_tertiary_tap_down;
        std::function<void(TapUpDetails)>        on_tertiary_tap_up;
        std::function<void()>                    on_tertiary_tap_cancel;

        std::function<void(LongPressDownDetails)>       on_long_press_down;
        std::function<void()>                           on_long_press_cancel;
        std::function<void(LongPressStartDetails)>      on_long_press_start;
        std::function<void(LongPressMoveUpdateDetails)> on_long_press_move_update;
        std::function<void(LongPressEndDetails)>        on_long_press_end;

        std::function<void(DragDownDetails)>     on_pan_down;
        std::function<void(DragStartDetails)>    on_pan_start;
        std::function<void(DragUpdateDetails)>   on_pan_update;
        std::function<void(DragEndDetails)>      on_pan_end;

        std::function<void(DragDownDetails)>     on_horizontal_drag_down;
        std::function<void(DragStartDetails)>    on_horizontal_drag_start;
        std::function<void(DragUpdateDetails)>   on_horizontal_drag_update;
        std::function<void(DragEndDetails)>      on_horizontal_drag_end;

        std::function<void(DragDownDetails)>     on_vertical_drag_down;
        std::function<void(DragStartDetails)>    on_vertical_drag_start;
        std::function<void(DragUpdateDetails)>   on_vertical_drag_update;
        std::function<void(DragEndDetails)>      on_vertical_drag_end;

        DragStartBehavior                        drag_start_behavior = DragStartBehavior::down;

        std::function<void(ScaleStartDetails)>   on_scale_start;
        std::function<void(ScaleUpdateDetails)>  on_scale_update;
        std::function<void(ScaleEndDetails)>     on_scale_end;

        /// Force-press thresholds, read live by ForcePressGestureRecognizer.
        /// Only ever meaningful for a stylus pointer -- see
        /// ForcePressGestureRecognizer's doc comment.
        float force_press_start_pressure = 0.4f;
        float force_press_peak_pressure  = 0.85f;

        std::function<void(ForcePressDetails)>   on_force_press_start;
        std::function<void(ForcePressDetails)>   on_force_press_peak;
        std::function<void(ForcePressDetails)>   on_force_press_update;
        std::function<void(ForcePressDetails)>   on_force_press_end;

        /// See HitTestBehavior's doc comment (hit_test.hpp). Defaults to
        /// `opaque`, matching this detector's pre-existing (only) behavior.
        HitTestBehavior                          behavior = HitTestBehavior::opaque;

        std::function<void(Offset)>     on_scroll;
        std::function<void(bool)>       on_press_change;
        std::function<void(bool)>       on_focus_change;

        RenderGestureDetector();
        ~RenderGestureDetector();

        /**
         * @brief Configures keyboard focus participation.
         *
         * @param node      External FocusNode to register, or nullptr to use
         *                  (and lazily create) an internal one.
         * @param autofocus Request focus immediately once registered.
         * @param focusable Opt-in switch; when false, no FocusNode is
         *                  registered at all regardless of `node`/`autofocus`.
         *
         * Safe to call repeatedly (e.g. every widget rebuild) — swaps
         * registration only when the effective node actually changes.
         */
        void setFocusConfig(std::shared_ptr<FocusNode> node, bool autofocus, bool focusable);

        void attach() override;
        void detach() override;

        /**
         * @brief The currently-registered FocusNode (external or lazily-
         * created own_focus_node_), if this detector is focusable() -- see
         * RenderObject::ownedFocusNode()'s doc comment. Used the same way
         * RenderFocus uses its own focus_node: to find the nearest ancestor
         * FocusNode for key-event bubbling.
         */
        FocusNode* ownedFocusNode() const noexcept override { return registered_node_.get(); }

        // ------------------------------------------------------------------
        // Called by this detector's own GestureRecognizers (tap/long-press/
        // pan) to drive shared owner-level state. Public because the
        // recognizers are separate classes, not because these are meant to
        // be called from outside the gesture subsystem.
        // ------------------------------------------------------------------

        /** @brief Toggles press-visual state; no-ops if already at `pressed`. */
        void setPressed(bool pressed);

        /** @brief Requests keyboard focus for this detector, as a completed tap does. */
        void requestFocusOnTap();

        // Transparent layout: size to child without loosening constraints.
        void performLayout() override;
        void performPaint(PaintContext& ctx, const Offset& offset) override;

        /**
         * @brief Behavior-aware hit test — see `behavior`'s doc comment.
         *
         * Overrides hitTest() directly (not just hitTestSelf()) because
         * `deferToChild` needs to skip adding this detector's own entry
         * entirely when a child already claimed the point, and
         * `translucent` needs to tag the entry it does add as non-opaque —
         * neither is expressible through RenderBox::hitTest()'s fixed
         * `hitTestChildren() || hitTestSelf()` sequencing, since
         * hitTestSelf() is only ever consulted (and only ever contributes
         * to whether an entry is added) in the short-circuit case where
         * hitTestChildren() already returned false.
         */
        bool hitTest(HitTestResult& result, const Offset& position) override;

        /**
         * @brief This box's on-screen position as of the last paint.
         *
         * Lets a tap callback (e.g. DropdownButton opening an anchored
         * overlay menu) find out where it is on screen without a general
         * localToGlobal() facility — mirrors the same pattern used by
         * RenderDraggable for its feedback-anchor offset.
         */
        Offset globalOffset() const noexcept { return global_offset_; }

    private:
        void onPointerEvent(const PointerEvent& event);
        void onTick(uint64_t now_ms);

        void syncFocusRegistration();
        bool handleFocusKey(const KeyEvent& event);

        Offset   global_offset_;

        // Focus state
        std::shared_ptr<FocusNode> ext_focus_node_;   // last externally-supplied node (may be null)
        std::shared_ptr<FocusNode> own_focus_node_;   // lazily-created fallback when focusable && no external node
        std::shared_ptr<FocusNode> registered_node_;  // node currently registered with FocusManager, if any
        bool     focusable_ = false;
        bool     autofocus_ = false;
        bool     attached_  = false;

        bool     pressed_ = false;

        std::unique_ptr<TapGestureRecognizer>             tap_recognizer_;
        std::unique_ptr<TapGestureRecognizer>             secondary_tap_recognizer_;
        std::unique_ptr<TapGestureRecognizer>             tertiary_tap_recognizer_;
        std::unique_ptr<LongPressGestureRecognizer>       long_press_recognizer_;
        std::unique_ptr<PanGestureRecognizer>             pan_recognizer_;
        std::unique_ptr<HorizontalDragGestureRecognizer>  horizontal_drag_recognizer_;
        std::unique_ptr<VerticalDragGestureRecognizer>    vertical_drag_recognizer_;
        std::unique_ptr<ScaleGestureRecognizer>           scale_recognizer_;
        std::unique_ptr<ForcePressGestureRecognizer>      force_press_recognizer_;
    };

} // namespace systems::leal::campello_widgets
