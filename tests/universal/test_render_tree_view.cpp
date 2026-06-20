#include <gtest/gtest.h>
#include <campello_widgets/ui/render_tree_view.hpp>
#include <campello_widgets/ui/tree_node.hpp>
#include <campello_widgets/ui/box_constraints.hpp>
#include <campello_widgets/ui/pointer_dispatcher.hpp>
#include <campello_widgets/ui/pointer_event.hpp>
#include <campello_widgets/widgets/draggable.hpp>

namespace cw = systems::leal::campello_widgets;

namespace
{
    // Builds a root node with `child_count` direct children, all expanded,
    // so the tree has enough rows to actually have scroll room.
    std::shared_ptr<cw::TreeNode> makeExpandedTree(cw::RenderTreeView& tv, int child_count)
    {
        auto root = std::make_shared<cw::TreeNode>();
        for (int i = 0; i < child_count; ++i)
            root->children.push_back(std::make_shared<cw::TreeNode>());

        tv.root       = root;
        tv.controller = std::make_shared<cw::TreeController>();
        tv.controller->expand(root.get());
        return root;
    }
} // namespace

// ---------------------------------------------------------------------------
// Plain pan-to-scroll still works after the gesture-arena migration
// ---------------------------------------------------------------------------

TEST(RenderTreeView, PanGestureScrollsWithoutCompetingRecognizer)
{
    cw::RenderTreeView tv;
    tv.row_height = 50.0f;
    makeExpandedTree(tv, 10); // 11 rows total: root + 10 children

    auto root_box = std::shared_ptr<cw::RenderBox>(&tv, [](cw::RenderBox*) {});
    cw::PointerDispatcher dispatcher(root_box);
    cw::PointerDispatcher::setActiveDispatcher(&dispatcher);

    tv.layout(cw::BoxConstraints::tight(400.0f, 200.0f));
    tv.attach();

    EXPECT_EQ(tv.visibleRange().first_row, 0);

    cw::PointerEvent down;
    down.kind     = cw::PointerEventKind::down;
    down.position = {50.0f, 100.0f};
    dispatcher.handlePointerEvent(down);

    // Single jump well past the 8px tap slop, dragging upward to scroll down.
    cw::PointerEvent move;
    move.kind     = cw::PointerEventKind::move;
    move.position = {50.0f, 40.0f}; // 60px up -> scroll_y += 60 -> first_row = floor(60/50) = 1
    dispatcher.handlePointerEvent(move);

    EXPECT_EQ(tv.visibleRange().first_row, 1);

    cw::PointerEvent up;
    up.kind = cw::PointerEventKind::up;
    dispatcher.handlePointerEvent(up);

    tv.detach();
    cw::PointerDispatcher::setActiveDispatcher(nullptr);
}

// Regresses a bug introduced by the first arena migration pass: with no
// competing recognizer, the arena resolves in TreeView's favor immediately
// at pointer-down (it's the sole member) — but accepting the arena must not
// itself start panning before any real movement, or sub-slop pointer jitter
// during a plain tap/click would scroll the view.
TEST(RenderTreeView, SubSlopJitterDoesNotScrollEvenWhenUncontested)
{
    cw::RenderTreeView tv;
    tv.row_height = 50.0f;
    makeExpandedTree(tv, 10);

    auto root_box = std::shared_ptr<cw::RenderBox>(&tv, [](cw::RenderBox*) {});
    cw::PointerDispatcher dispatcher(root_box);
    cw::PointerDispatcher::setActiveDispatcher(&dispatcher);

    tv.layout(cw::BoxConstraints::tight(400.0f, 200.0f));
    tv.attach();

    cw::PointerEvent down;
    down.kind     = cw::PointerEventKind::down;
    down.position = {50.0f, 100.0f};
    dispatcher.handlePointerEvent(down);

    // Several sub-slop jitter samples (well under the 8px kTapSlop each).
    cw::PointerEvent move;
    move.kind = cw::PointerEventKind::move;
    for (float dy : {1.0f, -1.0f, 2.0f, -1.0f})
    {
        move.position = {50.0f, 100.0f + dy};
        dispatcher.handlePointerEvent(move);
    }

    EXPECT_EQ(tv.visibleRange().first_row, 0); // never scrolled

    cw::PointerEvent up;
    up.kind = cw::PointerEventKind::up;
    dispatcher.handlePointerEvent(up);

    tv.detach();
    cw::PointerDispatcher::setActiveDispatcher(nullptr);
}

// ---------------------------------------------------------------------------
// Gesture arena: a Draggable row wins over the enclosing TreeView's pan
// ---------------------------------------------------------------------------
//
// Regresses the bug from TODO.md's Gesture Arena section: dragging a
// Draggable-wrapped row inside a TreeView used to never produce visual
// feedback because RenderTreeView's smaller tap slop claimed the gesture
// for scrolling before RenderDraggable's larger one fired. Now both
// register with the shared arena and only one of them may act.

TEST(RenderTreeView, DraggableRowWinsArenaOverEnclosingPan)
{
    cw::RenderTreeView tv;
    tv.row_height = 50.0f;
    makeExpandedTree(tv, 10); // plenty of scroll room if the arena lets TreeView win

    auto root_box = std::shared_ptr<cw::RenderBox>(&tv, [](cw::RenderBox*) {});
    cw::PointerDispatcher dispatcher(root_box);
    cw::PointerDispatcher::setActiveDispatcher(&dispatcher);

    tv.layout(cw::BoxConstraints::tight(400.0f, 200.0f));
    tv.attach();

    auto row = std::make_shared<cw::RenderDraggable<int>>();
    int  drag_start_count = 0;
    row->on_drag_start = [&](cw::Offset) { ++drag_start_count; };
    tv.setRowBox(0, row); // row 0 = the root node's row

    tv.layout(cw::BoxConstraints::tight(400.0f, 200.0f)); // position the row box

    cw::PointerEvent down;
    down.kind     = cw::PointerEventKind::down;
    down.position = {50.0f, 25.0f}; // inside row 0 (y in [0, 50))
    dispatcher.handlePointerEvent(down);

    cw::PointerEvent move;
    move.kind     = cw::PointerEventKind::move;
    move.position = {50.0f, 85.0f}; // 60px jump, same magnitude as the scroll test above
    dispatcher.handlePointerEvent(move);

    EXPECT_EQ(drag_start_count, 1);
    EXPECT_EQ(tv.visibleRange().first_row, 0); // TreeView must NOT have scrolled

    cw::PointerEvent up;
    up.kind     = cw::PointerEventKind::up;
    up.position = move.position;
    dispatcher.handlePointerEvent(up);

    tv.detach();
    cw::PointerDispatcher::setActiveDispatcher(nullptr);
}

TEST(RenderTreeView, TreeViewStillScrollsWhenDraggableRowIsNotHit)
{
    // Sanity check: a drag that starts outside the Draggable row (e.g. on a
    // plain sibling row) must still let the TreeView scroll normally.
    cw::RenderTreeView tv;
    tv.row_height = 50.0f;
    makeExpandedTree(tv, 10);

    auto root_box = std::shared_ptr<cw::RenderBox>(&tv, [](cw::RenderBox*) {});
    cw::PointerDispatcher dispatcher(root_box);
    cw::PointerDispatcher::setActiveDispatcher(&dispatcher);

    tv.layout(cw::BoxConstraints::tight(400.0f, 200.0f));
    tv.attach();

    auto row = std::make_shared<cw::RenderDraggable<int>>();
    int  drag_start_count = 0;
    row->on_drag_start = [&](cw::Offset) { ++drag_start_count; };
    tv.setRowBox(0, row); // only row 0 is draggable

    tv.layout(cw::BoxConstraints::tight(400.0f, 200.0f));

    // Start the gesture in row 1's area (y in [50, 100)) where there is no
    // Draggable registered, so the TreeView is the sole arena member.
    cw::PointerEvent down;
    down.kind     = cw::PointerEventKind::down;
    down.position = {50.0f, 75.0f};
    dispatcher.handlePointerEvent(down);

    cw::PointerEvent move;
    move.kind     = cw::PointerEventKind::move;
    move.position = {50.0f, 15.0f}; // 60px up
    dispatcher.handlePointerEvent(move);

    EXPECT_EQ(drag_start_count, 0);
    EXPECT_EQ(tv.visibleRange().first_row, 1);

    cw::PointerEvent up;
    up.kind = cw::PointerEventKind::up;
    dispatcher.handlePointerEvent(up);

    tv.detach();
    cw::PointerDispatcher::setActiveDispatcher(nullptr);
}
