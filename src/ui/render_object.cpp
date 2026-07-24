#include <campello_widgets/ui/render_object.hpp>
#include <campello_widgets/ui/debug_flags.hpp>
#include <campello_widgets/ui/frame_scheduler.hpp>
#include <campello_widgets/ui/thread_checker.hpp>
#include <campello_widgets/ui/renderer.hpp>
#include <campello_widgets/diagnostics/diagnostic_property.hpp>

#include <iostream>
#include <cstdio>
#include <typeinfo>

namespace systems::leal::campello_widgets
{

    std::unordered_set<const RenderObject*> RenderObject::s_alive_;

    RenderObject::RenderObject()
    {
        s_alive_.insert(this);
    }

    RenderObject::~RenderObject()
    {
        s_alive_.erase(this);
    }

    bool RenderObject::isAlive(const RenderObject* obj) noexcept
    {
        return obj && s_alive_.find(obj) != s_alive_.end();
    }

    // -----------------------------------------------------------------------
    // Debug helpers (translation unit local)
    // -----------------------------------------------------------------------

    static Color nextRainbowColor() noexcept
    {
        // Six semi-transparent palette entries that cycle across dirty repaints.
        static const Color kColors[] = {
            Color::fromRGBA(0.90f, 0.20f, 0.20f, 0.20f), // red
            Color::fromRGBA(0.90f, 0.55f, 0.10f, 0.20f), // orange
            Color::fromRGBA(0.80f, 0.80f, 0.10f, 0.20f), // yellow
            Color::fromRGBA(0.15f, 0.75f, 0.20f, 0.20f), // green
            Color::fromRGBA(0.15f, 0.45f, 0.90f, 0.20f), // blue
            Color::fromRGBA(0.55f, 0.15f, 0.85f, 0.20f), // purple
        };
        static int idx = 0;
        return kColors[idx++ % 6];
    }

    namespace
    {
        // Zero only for the externally-triggered call that starts a given
        // markNeedsLayout() bubble-up chain — nonzero for every recursive
        // parent_->markNeedsLayout() call within that same chain. Lets the
        // trace below distinguish "this node is the true origin" from
        // "this node is just being told a descendant changed."
        int s_layout_propagation_depth = 0;
    }

    void RenderObject::markNeedsLayout() noexcept
    {
        ThreadChecker::instance().assertOnBoundThread("RenderObject::markNeedsLayout");
        if (!needs_layout_)
        {
            if (DebugFlags::printDirtyRegionTrace)
                std::fprintf(stderr, "[dirty] markNeedsLayout() %s on %s\n",
                             s_layout_propagation_depth == 0 ? "TRUE_ORIGIN" : "propagated",
                             typeid(*this).name());
            needs_layout_ = true;
            if (parent_)
            {
                ++s_layout_propagation_depth;
                parent_->markNeedsLayout();
                --s_layout_propagation_depth;
            }
        }
        markNeedsPaint();
    }

    void RenderObject::markNeedsPaint() noexcept
    {
        ThreadChecker::instance().assertOnBoundThread("RenderObject::markNeedsPaint");
        if (needs_paint_) return;
        needs_paint_ = true;

        // A frame is needed regardless of where propagation below stops —
        // mirrors Flutter's PipelineOwner.requestVisualUpdate(), called at
        // every point markNeedsPaint() would otherwise stop bubbling, not
        // just when reaching the true root. Both calls are idempotent/cheap
        // to call repeatedly (see FrameScheduler's and Renderer::
        // notePaintRequested()'s doc comments).
        FrameScheduler::scheduleFrame();
        if (auto* r = detail::currentRenderer().load(std::memory_order_acquire))
            r->notePaintRequested();

        // Stop bubbling the full signal once we reach a node that owns its
        // own paint cache (OffsetLayer) — its paint() will independently
        // decide replay-vs-record next paint without needing ancestors
        // above it to know. That node still needs to inform ITS ancestors
        // that something below it needs a real (non-replayed) visit,
        // though — see markNeedsDescendantPaint()'s doc for why skipping
        // this stranded a nested boundary's dirty state forever.
        if (parent_)
        {
            if (!isRepaintBoundary())
                parent_->markNeedsPaint();
            else
                parent_->markNeedsDescendantPaint();
        }
    }

    void RenderObject::markNeedsDescendantPaint() noexcept
    {
        ThreadChecker::instance().assertOnBoundThread("RenderObject::markNeedsDescendantPaint");
        if (needs_descendant_paint_) return;
        needs_descendant_paint_ = true;

        FrameScheduler::scheduleFrame();
        if (auto* r = detail::currentRenderer().load(std::memory_order_acquire))
            r->notePaintRequested();

        // Unlike markNeedsPaint(), this weaker signal must cross every
        // ancestor boundary, not just the nearest one — see this method's
        // doc comment on why.
        if (parent_)
            parent_->markNeedsDescendantPaint();
    }

    void RenderObject::layout(const BoxConstraints& constraints)
    {
        const bool constraints_changed = !(constraints == constraints_);

        if (!needs_layout_ && !constraints_changed)
            return;

        const bool must_repaint = constraints_changed;
        constraints_  = constraints;
        needs_layout_ = false;
        performLayout();
        if (must_repaint)
            markNeedsPaint();
    }

    void RenderObject::paint(PaintContext& context, const Offset& offset)
    {
        const bool was_dirty = needs_paint_;

        // Report this node's on-screen bounds as dirty this frame — see
        // Renderer::noteDirtyRegion(). Only nodes that were actually
        // marked dirty contribute, so a sibling redundantly repainted
        // because its container had to fully re-record does not spuriously
        // widen the dirty region.
        if (was_dirty)
        {
            // offset-based positioning and any ambient canvas transform
            // (Transform widgets, or a scroll's canvas.translate()) are
            // independent and additive — project through the current
            // transform to get this node's true on-screen bounds, not
            // just its logical offset. See projectedBounds()'s doc.
            const Rect local_bounds = Rect::fromLTWH(offset.x, offset.y,
                                                       size_.width, size_.height);
            const Rect dirty_bounds = projectedBounds(context.canvas().currentTransform(),
                                                        local_bounds);
            if (auto* r = detail::currentRenderer().load(std::memory_order_acquire))
                r->noteDirtyRegion(dirty_bounds);

            if (DebugFlags::printDirtyRegionTrace)
                std::fprintf(stderr,
                    "[dirty] paint()  %-28s (%.0f,%.0f %.0fx%.0f)\n",
                    typeid(*this).name(), dirty_bounds.x, dirty_bounds.y,
                    dirty_bounds.width, dirty_bounds.height);
        }

        // ------------------------------------------------------------------
        // Debug overlays — zero cost when all flags are false
        // ------------------------------------------------------------------

        if (DebugFlags::repaintRainbowEnabled && was_dirty)
        {
            const Rect bounds = Rect::fromLTWH(offset.x, offset.y,
                                               size_.width, size_.height);
            context.canvas().drawRect(bounds, Paint::filled(nextRainbowColor()));
        }

        // A plain (non-boundary) node's performPaint() unconditionally
        // visits every child every time it runs — there is no cache to
        // "skip" here, unlike RenderRepaintBoundary/RenderClipRRect/the
        // self-boundaring scrollables. That means this call always fully
        // satisfies any pending "a descendant below me needs a real visit"
        // signal too, not just this node's own dirty flag. Without clearing
        // needs_descendant_paint_ here, the first time any descendant's
        // markNeedsDescendantPaint() passed through this node it would
        // latch true and never be cleared again (only the boundary classes'
        // overridden paint() reset it) — permanently blocking every future
        // descendant-dirty notification from ever reaching the real
        // boundary above this node, after its very first
        // successful repaint.
        needs_paint_             = false;
        needs_descendant_paint_  = false;
        performPaint(context, offset);

        if (DebugFlags::paintSizeEnabled || DebugFlags::paintBaselinesEnabled)
            debugPaint(context, offset);

        if (DebugFlags::paintSizeEnabled && !size_.isEmpty())
        {
            const Rect  bounds = Rect::fromLTWH(offset.x, offset.y,
                                                size_.width, size_.height);
            // Teal 1-px outline — matches Flutter's debugPaintSizeEnabled colour.
            const Paint border = Paint::stroked(
                Color::fromRGBA(0.0f, 0.55f, 1.0f, 0.85f), 1.0f);
            context.canvas().drawRect(bounds, border);
        }
    }

    void RenderObject::debugFillProperties(DiagnosticsPropertyBuilder& out) const
    {
        out.add(std::make_unique<ConstraintsProperty>("constraints", constraints_));
        out.add(std::make_unique<SizeProperty>("size", size_));
    }

    std::string RenderObject::toStringShort() const
    {
        return typeName();
    }

} // namespace systems::leal::campello_widgets
