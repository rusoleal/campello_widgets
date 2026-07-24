#pragma once

#include <campello_widgets/ui/box_constraints.hpp>
#include <campello_widgets/ui/size.hpp>
#include <campello_widgets/ui/offset.hpp>
#include <campello_widgets/ui/paint_context.hpp>
#include <campello_widgets/ui/draw_backend.hpp>
#include <campello_widgets/diagnostics/diagnosticable.hpp>
#include <atomic>
#include <unordered_set>

namespace systems::leal::campello_widgets
{

    /**
     * @brief Base class for objects that handle layout and painting.
     *
     * The RenderObject tree runs in parallel with the Widget/Element trees.
     * Each RenderObject:
     *  - Receives BoxConstraints from its parent during the layout pass.
     *  - Reports its chosen Size back to the parent.
     *  - Paints itself into a PaintContext during the paint pass.
     *
     * Subclass `RenderBox` for the box layout model. Override `performLayout()`
     * to compute `size_` from `constraints_`, and `performPaint()` to issue draw
     * calls.
     */
    class RenderObject : public Diagnosticable
    {
    public:
        RenderObject();
        virtual ~RenderObject();

        // ------------------------------------------------------------------
        // Layout
        // ------------------------------------------------------------------

        /**
         * @brief Runs the layout pass for this render object.
         *
         * Stores the incoming constraints, then delegates to `performLayout()`
         * if the object is dirty or the constraints have changed. Clears the
         * layout-dirty flag on completion.
         *
         * @param constraints The constraints imposed by the parent.
         */
        void layout(const BoxConstraints& constraints);

        /**
         * @brief Computes `size_` based on `constraints_` and child sizes.
         *
         * Override in subclasses. Must set `size_` before returning.
         */
        virtual void performLayout() = 0;

        /** @brief The size computed by the most recent layout pass. */
        Size size() const noexcept { return size_; }

        /** @brief The constraints from the most recent layout pass. */
        const BoxConstraints& constraints() const noexcept { return constraints_; }

        // ------------------------------------------------------------------
        // Paint
        // ------------------------------------------------------------------

        /**
         * @brief Runs the paint pass for this render object.
         *
         * Calls `performPaint()`, then clears the paint-dirty flag. Virtual so
         * `RenderRepaintBoundary` can intercept: when clean, it replays a
         * cached draw-command list instead of calling through to
         * `performPaint()` at all, skipping the subtree walk entirely.
         *
         * @param context Canvas context to draw into.
         * @param offset  Position of this object's top-left corner in its
         *                parent's coordinate space.
         */
        virtual void paint(PaintContext& context, const Offset& offset);

        /**
         * @brief Override to paint debug visualizations.
         *
         * Called automatically by paint() after performPaint() when any
         * debug overlay flag is enabled. The default implementation does nothing.
         */
        virtual void debugPaint(PaintContext& /*context*/, const Offset& /*offset*/) const {}

        /**
         * @brief Issues draw calls for this render object.
         *
         * Override in subclasses. `offset` is this object's origin in the
         * coordinate space of the current paint layer.
         */
        virtual void performPaint(PaintContext& context, const Offset& offset) = 0;

        // ------------------------------------------------------------------
        // Dirty flags
        // ------------------------------------------------------------------

        /**
         * @brief Marks this object as needing a layout pass.
         *
         * Propagates upward so ancestors know a descendant is dirty.
         * Also marks this subtree as needing paint, since a layout change
         * always requires a subsequent repaint.
         */
        void markNeedsLayout() noexcept;

        /**
         * @brief Marks this object as needing a paint pass.
         *
         * Propagates the *full* signal (this ancestor must fully re-record,
         * not just replay its cache) upward only until it reaches a node
         * for which `isRepaintBoundary()` is true (or the root) — that node
         * owns its own paint cache (see `OffsetLayer`) and will
         * independently decide whether to replay or re-record next paint.
         * Matches Flutter's `RenderObject.markNeedsPaint()`, which stops at
         * the nearest `isRepaintBoundary` ancestor rather than bubbling all
         * the way to root. A frame is still requested
         * (`FrameScheduler::scheduleFrame()`) regardless of where
         * propagation stops.
         *
         * Once it does stop at a boundary, propagation continues past it
         * (all the way to root) as the weaker `markNeedsDescendantPaint()`
         * signal instead — see that method's doc for why this second signal
         * is necessary: without it, a boundary nested inside another
         * boundary can have its dirty state silently stranded forever.
         */
        void markNeedsPaint() noexcept;

        /**
         * @brief Marks that *some* descendant repaint boundary has
         * unconsumed dirty state, without marking this node's own content
         * dirty.
         *
         * `markNeedsPaint()`, on reaching the first `isRepaintBoundary()`
         * ancestor, stops propagating the full "re-record me" signal but
         * switches to this weaker one and keeps going — through and past
         * every further ancestor boundary, all the way to root. Every
         * repaint-boundary `paint()` override must OR this into its replay
         * decision (alongside its own `needsPaint()`): a boundary whose own
         * content is unchanged must still force a real record — not a
         * cached replay — whenever this flag is set, because a replay skips
         * calling `paintChild()` entirely, and any nested boundary
         * underneath would then never get visited to consume its own
         * pending `needs_paint_`. Left unconsumed, that nested boundary's
         * `needs_paint_` stays stuck true forever (the `if (needs_paint_)
         * return;` guard in `markNeedsPaint()` makes every future mark a
         * silent no-op on it), which starves `Renderer::paint_requested_`
         * of any future signal and freezes the app — this happened in
         * practice with a `RenderDecoratedBox` shadow boundary nested
         * outside a `RenderClipRRect` boundary before this flag existed.
         *
         * Consumed (reset to false) by a boundary's `paint()` exactly when
         * `needs_paint_` is: after a call that actually descended (recorded
         * fresh rather than replayed), since that descent is what gives any
         * nested boundary its chance to run its own replay-vs-record logic.
         */
        void markNeedsDescendantPaint() noexcept;

        bool needsLayout() const noexcept { return needs_layout_; }
        bool needsPaint()  const noexcept { return needs_paint_;  }
        bool needsDescendantPaint() const noexcept { return needs_descendant_paint_; }

        /**
         * @brief True for nodes that own their own paint cache (`OffsetLayer`)
         * and independently decide replay-vs-record — `RenderRepaintBoundary`
         * and the self-boundaring scrollables (`RenderListView`,
         * `RenderGridView`, `RenderPageView`,
         * `RenderSingleChildScrollView`). `markNeedsPaint()` stops
         * propagating once it reaches such a node, since that node's own
         * paint() will handle repainting its subtree without needing its
         * ancestors to also be told. Default false.
         */
        virtual bool isRepaintBoundary() const noexcept { return false; }

        // ------------------------------------------------------------------
        // Lifetime tracking
        // ------------------------------------------------------------------

        /**
         * @brief Returns true if |obj| is a live RenderObject (i.e. its
         *        constructor has run and its destructor has not yet completed).
         *
         * This is used by PointerDispatcher to avoid dispatching events to
         * render boxes that have been destroyed during a tree mutation.
         */
        static bool isAlive(const RenderObject* obj) noexcept;

        // ------------------------------------------------------------------
        // Parent tracking (set by child-management methods in RenderBox)
        // ------------------------------------------------------------------

        virtual void attach() {}
        virtual void detach() {}

        void setParent(RenderObject* parent) noexcept
        {
            RenderObject* old_parent = parent_;
            if (old_parent && !parent)
                detach();
            parent_ = parent;
            if (parent && !old_parent)
                attach();
        }

        /**
         * @brief This object's parent in the render tree, or nullptr at the root.
         *
         * Lets code outside the normal layout/paint traversal (e.g. an
         * overlay anchoring itself to a widget's position) walk up to the
         * root to read its current size — the root's size always equals
         * the live viewport, so this is a reliable, always-fresh
         * alternative to caching a viewport size that can go stale between
         * whenever it was captured and whenever it's actually used.
         */
        RenderObject* parent() const noexcept { return parent_; }

        // ------------------------------------------------------------------
        // Layout-time backend access
        // ------------------------------------------------------------------

        /**
         * @brief Set the draw backend available during the current layout pass.
         *
         * Called by Renderer before root_->layout() so that render objects
         * (e.g. RenderText) can query real font metrics during performLayout().
         */
        static void setActiveBackend(IDrawBackend* backend) noexcept
        {
            s_active_backend_.store(backend, std::memory_order_relaxed);
        }

        /** @brief Returns the draw backend set for the current layout pass, or nullptr. */
        static IDrawBackend* activeBackend() noexcept { return s_active_backend_.load(std::memory_order_relaxed); }

        // ------------------------------------------------------------------
        // Device pixel ratio access
        // ------------------------------------------------------------------

        /**
         * @brief Set the device pixel ratio for the current layout/paint pass.
         *
         * Called by Renderer before layout() and paint() so that render objects
         * (e.g. RenderText) can scale text to physical pixels during painting.
         */
        static void setActiveDevicePixelRatio(float dpr) noexcept
        {
            s_active_dpr_.store(dpr, std::memory_order_relaxed);
        }

        /**
         * @brief Returns the device pixel ratio for the current pass.
         *
         * Text render objects use this to scale font_size to physical pixels
         * before passing to the draw backend, ensuring crisp text rendering.
         */
        static float activeDevicePixelRatio() noexcept { return s_active_dpr_.load(std::memory_order_relaxed); }

        // ------------------------------------------------------------------
        // Paint-origin offset access
        // ------------------------------------------------------------------

        /**
         * @brief Set the offset the current paint pass starts from.
         *
         * Renderer::generateDrawList() seeds root_->paint() with
         * Offset{view_insets_.left, view_insets_.top} — the safe-area inset
         * (status bar / Dynamic Island) — so the whole tree paints shifted
         * down from the physical screen origin. That makes the `offset`
         * parameter performPaint() receives "screen space", NOT the
         * tree-local space PointerDispatcher's hit-testing and PointerEvent
         * positions use. Render objects that capture their own paint offset
         * to later compare or combine it with a pointer position (e.g.
         * RenderDraggable/RenderDragTarget tracking where a drag started
         * relative to their own bounds) must subtract this to convert back
         * to tree-local space first, or the two disagree by exactly the
         * inset whenever it's non-zero (invisible on platforms/orientations
         * where the inset happens to be zero, e.g. macOS — very visible on
         * any iPhone).
         */
        static void setActivePaintOriginOffset(Offset offset) noexcept
        {
            s_active_paint_origin_x_.store(offset.x, std::memory_order_relaxed);
            s_active_paint_origin_y_.store(offset.y, std::memory_order_relaxed);
        }

        /** @brief Returns the offset set for the current paint pass (see setActivePaintOriginOffset). */
        static Offset activePaintOriginOffset() noexcept
        {
            return Offset{
                s_active_paint_origin_x_.load(std::memory_order_relaxed),
                s_active_paint_origin_y_.load(std::memory_order_relaxed)};
        }

    protected:
        BoxConstraints  constraints_;
        Size            size_;
        bool            needs_layout_          = true;
        bool            needs_paint_           = true;
        // See markNeedsDescendantPaint()'s doc. Defaults true so the very
        // first paint always descends, matching needs_paint_'s own default.
        bool            needs_descendant_paint_ = true;
        RenderObject*   parent_       = nullptr;

        void debugFillProperties(DiagnosticsPropertyBuilder& properties) const override;
        std::string toStringShort() const override;

    private:
        inline static std::atomic<IDrawBackend*> s_active_backend_{nullptr};
        inline static std::atomic<float> s_active_dpr_{1.0f};
        inline static std::atomic<float> s_active_paint_origin_x_{0.0f};
        inline static std::atomic<float> s_active_paint_origin_y_{0.0f};
        static std::unordered_set<const RenderObject*> s_alive_;
    };

} // namespace systems::leal::campello_widgets
