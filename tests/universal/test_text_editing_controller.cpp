#include <gtest/gtest.h>
#include <campello_widgets/ui/text_editing_controller.hpp>

namespace cw = systems::leal::campello_widgets;

// -----------------------------------------------------------------------
// Construction
// -----------------------------------------------------------------------

TEST(TextEditingController, DefaultConstructorEmpty)
{
    cw::TextEditingController ctrl;
    EXPECT_EQ(ctrl.text(), "");
    EXPECT_EQ(ctrl.selectionStart(), 0);
    EXPECT_EQ(ctrl.selectionEnd(),   0);
    EXPECT_FALSE(ctrl.hasSelection());
}

TEST(TextEditingController, InitialTextCursorAtEnd)
{
    cw::TextEditingController ctrl("hello");
    EXPECT_EQ(ctrl.text(), "hello");
    EXPECT_EQ(ctrl.selectionStart(), 5);
    EXPECT_EQ(ctrl.selectionEnd(),   5);
}

// -----------------------------------------------------------------------
// setText / clear
// -----------------------------------------------------------------------

TEST(TextEditingController, SetTextMovesCursorToEnd)
{
    cw::TextEditingController ctrl;
    ctrl.setText("world");
    EXPECT_EQ(ctrl.text(), "world");
    EXPECT_EQ(ctrl.selectionStart(), 5);
    EXPECT_EQ(ctrl.selectionEnd(),   5);
}

TEST(TextEditingController, SetTextNoOpWhenSame)
{
    cw::TextEditingController ctrl("abc");
    int calls = 0;
    ctrl.addListener([&]{ ++calls; });
    ctrl.setText("abc");
    EXPECT_EQ(calls, 0);
}

TEST(TextEditingController, ClearResetsEverything)
{
    cw::TextEditingController ctrl("hello");
    ctrl.clear();
    EXPECT_EQ(ctrl.text(), "");
    EXPECT_EQ(ctrl.selectionStart(), 0);
    EXPECT_EQ(ctrl.selectionEnd(),   0);
}

// -----------------------------------------------------------------------
// setSelection / selectAll / selectedText
// -----------------------------------------------------------------------

TEST(TextEditingController, SetSelectionRange)
{
    cw::TextEditingController ctrl("hello");
    ctrl.setSelection(1, 3);
    EXPECT_EQ(ctrl.selectionStart(), 1);
    EXPECT_EQ(ctrl.selectionEnd(),   3);
    EXPECT_TRUE(ctrl.hasSelection());
    EXPECT_EQ(ctrl.selectedText(), "el");
}

TEST(TextEditingController, SetSelectionCollapsed)
{
    cw::TextEditingController ctrl("hello");
    ctrl.setSelection(2, 2);
    EXPECT_FALSE(ctrl.hasSelection());
    EXPECT_EQ(ctrl.selectedText(), "");
}

TEST(TextEditingController, SetSelectionClampsToTextBounds)
{
    cw::TextEditingController ctrl("hi");
    ctrl.setSelection(-5, 100);
    EXPECT_EQ(ctrl.selectionStart(), 0);
    EXPECT_EQ(ctrl.selectionEnd(),   2);
}

TEST(TextEditingController, SetSelectionNoOpWhenSame)
{
    cw::TextEditingController ctrl("hello");
    ctrl.setSelection(1, 3);
    int calls = 0;
    ctrl.addListener([&]{ ++calls; });
    ctrl.setSelection(1, 3);
    EXPECT_EQ(calls, 0);
}

TEST(TextEditingController, SelectAll)
{
    cw::TextEditingController ctrl("hello");
    ctrl.setSelection(2, 2);
    ctrl.selectAll();
    EXPECT_EQ(ctrl.selectionStart(), 0);
    EXPECT_EQ(ctrl.selectionEnd(),   5);
    EXPECT_EQ(ctrl.selectedText(), "hello");
}

TEST(TextEditingController, SelectedTextReversedRange)
{
    // selectionEnd < selectionStart is still valid
    cw::TextEditingController ctrl("hello");
    ctrl.setSelection(4, 1);
    EXPECT_EQ(ctrl.selectedText(), "ell");
}

// -----------------------------------------------------------------------
// insertText
// -----------------------------------------------------------------------

TEST(TextEditingController, InsertAtCursor)
{
    cw::TextEditingController ctrl("helo");
    ctrl.setSelection(3, 3);
    ctrl.insertText("l");
    EXPECT_EQ(ctrl.text(), "hello");
    EXPECT_EQ(ctrl.selectionStart(), 4);
    EXPECT_EQ(ctrl.selectionEnd(),   4);
}

TEST(TextEditingController, InsertReplacesSelection)
{
    cw::TextEditingController ctrl("hello world");
    ctrl.setSelection(6, 11);
    ctrl.insertText("there");
    EXPECT_EQ(ctrl.text(), "hello there");
    EXPECT_EQ(ctrl.selectionStart(), 11);
}

TEST(TextEditingController, InsertAtBeginning)
{
    cw::TextEditingController ctrl("world");
    ctrl.setSelection(0, 0);
    ctrl.insertText("hello ");
    EXPECT_EQ(ctrl.text(), "hello world");
}

TEST(TextEditingController, InsertEmptyIsNoOp)
{
    cw::TextEditingController ctrl("abc");
    int calls = 0;
    ctrl.addListener([&]{ ++calls; });
    ctrl.insertText("");
    EXPECT_EQ(calls, 0);
    EXPECT_EQ(ctrl.text(), "abc");
}

// -----------------------------------------------------------------------
// deleteBackward
// -----------------------------------------------------------------------

TEST(TextEditingController, DeleteBackwardAtCursor)
{
    cw::TextEditingController ctrl("hello");
    ctrl.setSelection(5, 5);
    ctrl.deleteBackward();
    EXPECT_EQ(ctrl.text(), "hell");
    EXPECT_EQ(ctrl.selectionEnd(), 4);
}

TEST(TextEditingController, DeleteBackwardAtStart_NoOp)
{
    cw::TextEditingController ctrl("hello");
    ctrl.setSelection(0, 0);
    int calls = 0;
    ctrl.addListener([&]{ ++calls; });
    ctrl.deleteBackward();
    EXPECT_EQ(ctrl.text(), "hello");
    EXPECT_EQ(calls, 0);
}

TEST(TextEditingController, DeleteBackwardClearsSelection)
{
    cw::TextEditingController ctrl("hello");
    ctrl.setSelection(1, 4); // "ell"
    ctrl.deleteBackward();
    EXPECT_EQ(ctrl.text(), "ho");
    EXPECT_EQ(ctrl.selectionStart(), 1);
    EXPECT_EQ(ctrl.selectionEnd(),   1);
}

// -----------------------------------------------------------------------
// deleteForward
// -----------------------------------------------------------------------

TEST(TextEditingController, DeleteForwardAtCursor)
{
    cw::TextEditingController ctrl("hello");
    ctrl.setSelection(0, 0);
    ctrl.deleteForward();
    EXPECT_EQ(ctrl.text(), "ello");
    EXPECT_EQ(ctrl.selectionEnd(), 0);
}

TEST(TextEditingController, DeleteForwardAtEnd_NoOp)
{
    cw::TextEditingController ctrl("hello");
    ctrl.setSelection(5, 5);
    int calls = 0;
    ctrl.addListener([&]{ ++calls; });
    ctrl.deleteForward();
    EXPECT_EQ(ctrl.text(), "hello");
    EXPECT_EQ(calls, 0);
}

TEST(TextEditingController, DeleteForwardClearsSelection)
{
    cw::TextEditingController ctrl("hello");
    ctrl.setSelection(1, 4); // "ell"
    ctrl.deleteForward();
    EXPECT_EQ(ctrl.text(), "ho");
    EXPECT_EQ(ctrl.selectionStart(), 1);
}

// -----------------------------------------------------------------------
// Listeners
// -----------------------------------------------------------------------

TEST(TextEditingController, ListenerFiredOnTextChange)
{
    cw::TextEditingController ctrl;
    int calls = 0;
    ctrl.addListener([&]{ ++calls; });
    ctrl.insertText("x");
    EXPECT_EQ(calls, 1);
}

TEST(TextEditingController, ListenerFiredOnSelectionChange)
{
    cw::TextEditingController ctrl("hello");
    int calls = 0;
    ctrl.addListener([&]{ ++calls; });
    ctrl.setSelection(0, 3);
    EXPECT_EQ(calls, 1);
}

TEST(TextEditingController, ListenerRemovedStopsNotifications)
{
    cw::TextEditingController ctrl;
    int calls = 0;
    uint64_t id = ctrl.addListener([&]{ ++calls; });
    ctrl.setText("a");
    EXPECT_EQ(calls, 1);
    ctrl.removeListener(id);
    ctrl.setText("b");
    EXPECT_EQ(calls, 1); // no additional call
}

TEST(TextEditingController, MultipleListeners)
{
    cw::TextEditingController ctrl;
    int a = 0, b = 0;
    ctrl.addListener([&]{ ++a; });
    ctrl.addListener([&]{ ++b; });
    ctrl.setText("hi");
    EXPECT_EQ(a, 1);
    EXPECT_EQ(b, 1);
}

// -----------------------------------------------------------------------
// Edge cases
// -----------------------------------------------------------------------

TEST(TextEditingController, InsertMultibyteUtf8)
{
    cw::TextEditingController ctrl;
    ctrl.setSelection(0, 0);
    ctrl.insertText("\xE2\x80\xA2"); // bullet •
    EXPECT_EQ(ctrl.text().size(), 3u);
    EXPECT_EQ(ctrl.selectionEnd(), 3);
}

TEST(TextEditingController, ReplaceEntireText)
{
    cw::TextEditingController ctrl("old text");
    ctrl.selectAll();
    ctrl.insertText("new");
    EXPECT_EQ(ctrl.text(), "new");
}
