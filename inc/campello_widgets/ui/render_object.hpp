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
         * Propagates upward only until it reaches a node for which
         * `isRepaintBoundary()` is true (or the root) — that node owns its
         * own paint cache (see `OffsetLayer`) and will independently decide
         * whether to replay or re-record next paint, so ancestors above it
         * don't need to be told. Matches Flutter's
         * `RenderObject.markNeedsPaint()`, which stops at the nearest
         * `isRepaintBoundary` ancestor rather than bubbling all the way to
         * root. A frame is still requested (`FrameScheduler::scheduleFrame()`)
         * regardless of where propagation stops.
         */
        void markNeedsPaint() noexcept;

        bool needsLayout() const noexcept { return needs_layout_; }
        bool needsPaint()  const noexcept { return needs_paint_;  }

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

    protected:
        BoxConstraints  constraints_;
        Size            size_;
        bool            needs_layout_ = true;
        bool            needs_paint_  = true;
        RenderObject*   parent_       = nullptr;

        void debugFillProperties(DiagnosticsPropertyBuilder& properties) const override;
        std::string toStringShort() const override;

    private:
        inline static std::atomic<IDrawBackend*> s_active_backend_{nullptr};
        inline static std::atomic<float> s_active_dpr_{1.0f};
        static std::unordered_set<const RenderObject*> s_alive_;
    };

} // namespace systems::leal::campello_widgets
