#pragma once

#include <functional>
#include <memory>
#include <unordered_map>
#include <campello_widgets/ui/render_box.hpp>
#include <campello_widgets/ui/render_sliver.hpp>

namespace systems::leal::campello_widgets
{

    /**
     * @brief Sliver-protocol analog of RenderListView -- a virtualised,
     * fixed-extent list of RenderBox children indexed by position.
     *
     * Stage 4 of the sliver-scrolling initiative, "the list case" per the
     * Sliver Protocol Scoping artifact. Reactive storage only, mirroring
     * RenderListView's own division of responsibility exactly: this class
     * owns index math and a sparse std::unordered_map<int, ChildEntry> of
     * currently-loaded children, but does NOT decide which indices should be
     * mounted or unmounted as scrolling happens -- that mount/unmount policy
     * (plus any look-ahead buffer) is the caller's job, the same way
     * ListViewElement owns it for RenderListView today. No widget-layer
     * bridge exists yet for slivers, so this stage is verified the same way
     * Stages 1-3 were: real RenderObjects, hand-wired directly in tests.
     *
     * Unlike RenderListView, this class has no ambient scroll position of
     * its own -- it only ever sees its own local sliver_constraints_
     * (scroll_offset already >= 0, remaining_paint_extent as its visible
     * budget), supplied fresh by the parent RenderViewport on every layout
     * pass. The parent RenderViewport also already shifts this sliver's
     * entire paint position by -scroll_offset before calling performPaint()
     * (see RenderViewport::performPaint()'s layout_offset computation, and
     * RenderSliverToBoxAdapter::performPaint() for the confirmed precedent)
     * -- so, unlike RenderListView::performPaint()'s own ambient
     * canvas.translate(-scroll, ...), this class paints each item at a
     * plain, unshifted offset + item_pos. Applying an extra shift here would
     * double-count the scroll offset.
     */
    class RenderSliverFixedExtentList : public RenderSliver
    {
    public:
        int   item_count  = 0;

        /// Fixed size on the scroll axis per item (height for vertical
        /// lists, width for horizontal lists). Must be > 0 for virtualisation
        /// (and any non-zero geometry) to work.
        float item_extent = 0.0f;

        /// Fired when the visible item range changes. Set by the caller.
        std::function<void()> on_visible_range_changed;

        // ------------------------------------------------------------------
        // Child management — called by the caller (a future widget bridge),
        // same division of responsibility as RenderListView::setItemBox()/
        // removeItemBox().
        // ------------------------------------------------------------------

        /** @brief Attaches a render box for the given item index. */
        void setItemBox(int index, std::shared_ptr<RenderBox> box);

        /** @brief Detaches and discards the render box for the given index. */
        void removeItemBox(int index);

        /** @brief The render box currently loaded for this index, or nullptr. */
        RenderBox* itemBoxAt(int index) const noexcept;

        // ------------------------------------------------------------------
        // Visible range — queried by the caller to decide what to mount/unmount.
        // ------------------------------------------------------------------

        /** @brief Index of the first (partially) visible item. */
        int firstVisibleIndex() const noexcept;

        /** @brief Index of the last (partially) visible item (inclusive). */
        int lastVisibleIndex() const noexcept;

    protected:
        void performLayoutSliver() override;
        void performPaint(PaintContext& context, const Offset& offset) override;

    private:
        struct ChildEntry
        {
            std::shared_ptr<RenderBox> box;
            Offset                     offset;
        };

        // Sparse: only currently-loaded items are present.
        std::unordered_map<int, ChildEntry> item_boxes_;

        // Cached visible range — used to detect changes.
        int cached_first_ = -1;
        int cached_last_  = -1;

        void checkVisibleRangeChanged();
    };

} // namespace systems::leal::campello_widgets
