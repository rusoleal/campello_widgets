#pragma once

#include <functional>
#include <memory>
#include <campello_widgets/ui/key_event.hpp>
#include <campello_widgets/ui/rect.hpp>

namespace systems::leal::campello_widgets
{

    class FocusManager;
    class RenderFocus;
    class RenderGestureDetector;
    class RenderObject;
    class FocusTraversalPolicy;

    /**
     * @brief Represents a focusable entity in the widget tree.
     *
     * Owned by a `Focus` widget (or created manually). Register it with a
     * `Focus` widget to participate in keyboard routing and tab traversal.
     *
     * Call `requestFocus()` to give this node keyboard focus.
     * Call `unfocus()` to release it. The `FocusManager` will call
     * `on_focus_changed` when the focus state changes.
     *
     * The `on_key` handler is called for every keyboard event while this
     * node has focus. Return true to mark the event as consumed (stops
     * further routing); return false to let it propagate.
     */
    class FocusNode
    {
    public:
        /** @brief Called when a key event arrives while this node is focused.
         *  Return true to consume the event, false to propagate. */
        std::function<bool(const KeyEvent&)> on_key;

        /** @brief Called with the new focus state whenever focus changes. */
        std::function<void(bool has_focus)> on_focus_changed;

        // ------------------------------------------------------------------
        // Focus control
        // ------------------------------------------------------------------

        /** @brief Requests keyboard focus from the active FocusManager. */
        void requestFocus();

        /** @brief Releases focus from this node. */
        void unfocus();

        /** @brief Returns true if this node currently has keyboard focus. */
        bool hasFocus() const noexcept { return focused_; }

        /**
         * @brief This node's on-screen bounds, in the window's global
         * (post-transform) coordinate space, as of the last paint.
         *
         * Updated every frame by whichever render object owns this node —
         * `RenderFocus` (nodes registered via the `Focus` widget) or
         * `RenderGestureDetector` (its own lazily-created/externally
         * supplied node when `focusable` is set — see its class doc).
         * Used by `FocusManager` for directional (arrow-key/D-pad)
         * navigation. `Rect::zero()` if this node hasn't been painted yet.
         */
        Rect bounds() const noexcept { return bounds_; }

        /**
         * @brief The nearest ancestor FocusNode in the render tree, or
         * nullptr at the root (or if this node isn't currently attached).
         *
         * Computed on demand by walking owner()'s RenderObject::parent()
         * chain looking for the first ancestor whose ownedFocusNode() is
         * non-null -- deliberately NOT cached at attach() time. An earlier
         * version cached this eagerly (set once when the owning render
         * object attached), which broke: a render object's own attach()
         * fires as soon as IT gets a parent, which happens bottom-up while
         * a fresh subtree is being assembled -- e.g. RenderMouseRegion
         * (wrapping RichTextField/TextField's actual render object) adopts
         * its child, firing the child's attach() and thus this walk,
         * *before* RenderMouseRegion itself has been attached to its own
         * ancestor further up. The walk would see a truncated chain and
         * wrongly conclude there was no ancestor. Computing it lazily here
         * instead -- only ever called from FocusManager::handleKeyEvent(),
         * i.e. in response to a real key event, which can only happen once
         * the visible tree has long since finished mounting -- sidesteps
         * the ordering problem entirely: by dispatch time every attach()
         * in the tree has already run.
         *
         * `FocusManager::handleKeyEvent()` walks this chain from the
         * focused leaf outward so an unconsumed key event actually
         * propagates to an ancestor's `on_key`, matching both this class's
         * own `on_key` doc comment ("return false to let it propagate")
         * and Flutter's real `FocusNode` behavior — neither of which this
         * implemented before, confirmed via a real bug: a `KeyboardListener`
         * wrapping a focused child (e.g. a search bar around a TextField)
         * never saw a key the child's own `on_key` didn't consume, because
         * only the single focused leaf node was ever tried.
         */
        FocusNode* parent() const noexcept;

        /**
         * @brief The RenderObject that owns this node (RenderFocus,
         * RenderGestureDetector, RenderTextField, RenderRichTextField, ...).
         * Set once, unconditionally, in the owner's attach() (and cleared
         * in detach()) -- unlike the old parent_ cache, this never needs to
         * walk anything at attach time, so mount order can't affect it.
         */
        void setOwner(RenderObject* owner) noexcept { owner_ = owner; }

        // ------------------------------------------------------------------
        // Scopes and traversal (Flutter-parity FocusScope) -- see
        // FocusManager::nearestEnclosingScope()'s doc comment for how these
        // are actually used.
        // ------------------------------------------------------------------

        /**
         * @brief True if this node is a traversal/restoration scope
         * boundary rather than an ordinary focusable stop.
         *
         * Tab/Shift+Tab and directional focus movement never cross out of
         * the nearest enclosing scope of whatever currently has focus (see
         * FocusManager::nearestEnclosingScope()) -- a scope is a container,
         * never itself a Tab stop. Set via the `Focus` widget's `scope`
         * prop (or, more conveniently, by using the `FocusScope` widget,
         * which sets this automatically) -- see focus_scope.hpp.
         */
        bool isScope() const noexcept { return is_scope_; }
        void setScope(bool value) noexcept { is_scope_ = value; }

        /** @brief Excludes this node from Tab/Shift+Tab and directional
         *  traversal candidate lists (it can still be focused
         *  programmatically via requestFocus()). Mirrors Flutter's
         *  FocusNode.skipTraversal. */
        bool skipTraversal() const noexcept { return skip_traversal_; }
        void setSkipTraversal(bool value) noexcept { skip_traversal_ = value; }

        /** @brief When false, requestFocus() (and thus all traversal) is a
         *  no-op for this node -- it can never hold focus at all. Mirrors
         *  Flutter's FocusNode.canRequestFocus. */
        bool canRequestFocus() const noexcept { return can_request_focus_; }
        void setCanRequestFocus(bool value) noexcept { can_request_focus_ = value; }

        /**
         * @brief Scope-only bookkeeping: whatever held focus immediately
         * before this scope registered, captured by
         * FocusManager::registerNode() and consumed by
         * FocusManager::unregisterNode() to restore focus there when the
         * scope closes while focus was still trapped inside it. Read/
         * written exclusively by FocusManager -- not meant to be touched
         * from application code.
         */
        FocusNode* previouslyFocusedOutside() const noexcept { return previously_focused_outside_; }
        void setPreviouslyFocusedOutside(FocusNode* node) noexcept { previously_focused_outside_ = node; }

        /**
         * @brief Governs the order Tab/Shift+Tab visits candidates within
         * this scope (only meaningful when isScope()) -- null means
         * "inherit FocusManager's default" (registration order, matching
         * every currently-working Tab flow in the app). See
         * focus_traversal_policy.hpp.
         */
        std::shared_ptr<FocusTraversalPolicy> traversal_policy;

    private:
        friend class FocusManager;
        friend class RenderFocus;
        friend class RenderGestureDetector;

        bool focused_ = false;
        Rect bounds_;
        RenderObject* owner_ = nullptr;
        bool is_scope_ = false;
        bool skip_traversal_ = false;
        bool can_request_focus_ = true;
        FocusNode* previously_focused_outside_ = nullptr;
    };

} // namespace systems::leal::campello_widgets
