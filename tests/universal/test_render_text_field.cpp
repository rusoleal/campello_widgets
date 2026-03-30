#include <gtest/gtest.h>
#include <campello_widgets/ui/render_text_field.hpp>
#include <campello_widgets/ui/text_editing_controller.hpp>
#include <campello_widgets/ui/box_constraints.hpp>

namespace cw = systems::leal::campello_widgets;

// -----------------------------------------------------------------------
// Layout — height
// -----------------------------------------------------------------------

TEST(RenderTextField, HeightAtLeastMinHeight)
{
    auto ctrl = std::make_shared<cw::TextEditingController>();
    cw::RenderTextField field;
    field.controller = ctrl;
    field.min_height = 44.0f;
    field.layout(cw::BoxConstraints::loose(400, 400));
    EXPECT_GE(field.size().height, 44.0f);
}

TEST(RenderTextField, DefaultMinHeightRespected)
{
    auto ctrl = std::make_shared<cw::TextEditingController>();
    cw::RenderTextField field;
    field.controller = ctrl;
    // Default min_height is 36
    field.layout(cw::BoxConstraints::loose(300, 300));
    EXPECT_GE(field.size().height, 36.0f);
}

TEST(RenderTextField, WidthFillsAvailableSpace)
{
    auto ctrl = std::make_shared<cw::TextEditingController>();
    cw::RenderTextField field;
    field.controller = ctrl;
    field.layout(cw::BoxConstraints::loose(320, 200));
    EXPECT_FLOAT_EQ(field.size().width, 320.0f);
}

TEST(RenderTextField, TightConstraints)
{
    auto ctrl = std::make_shared<cw::TextEditingController>();
    cw::RenderTextField field;
    field.controller = ctrl;
    field.layout(cw::BoxConstraints::tight({200, 50}));
    EXPECT_FLOAT_EQ(field.size().width,  200.0f);
    EXPECT_FLOAT_EQ(field.size().height,  50.0f);
}

TEST(RenderTextField, FontSizeAffectsNaturalHeight)
{
    auto ctrl = std::make_shared<cw::TextEditingController>();

    cw::RenderTextField small, large;
    small.controller = ctrl;
    large.controller = ctrl;
    small.style.font_size = 10.0f;
    large.style.font_size = 40.0f;
    small.min_height = 0.0f;
    large.min_height = 0.0f;

    small.layout(cw::BoxConstraints::loose(400, 400));
    large.layout(cw::BoxConstraints::loose(400, 400));

    EXPECT_LT(small.size().height, large.size().height);
}

// -----------------------------------------------------------------------
// Layout — null controller doesn't crash
// -----------------------------------------------------------------------

TEST(RenderTextField, NullControllerLayoutSafe)
{
    cw::RenderTextField field;
    // controller is null — layout should not crash
    EXPECT_NO_THROW(field.layout(cw::BoxConstraints::loose(300, 300)));
}

// -----------------------------------------------------------------------
// Keyboard — key events update controller
// -----------------------------------------------------------------------

TEST(RenderTextField, HandleKeyPrintableChar)
{
    auto ctrl = std::make_shared<cw::TextEditingController>();
    cw::RenderTextField field;
    field.controller = ctrl;

    cw::KeyEvent ev;
    ev.kind      = cw::KeyEventKind::down;
    ev.key_code  = cw::KeyCode::unknown;
    ev.character = 'A';
    bool consumed = field.handleKeyEvent(ev);

    EXPECT_TRUE(consumed);
    EXPECT_EQ(ctrl->text(), "A");
    EXPECT_EQ(ctrl->selectionEnd(), 1);
}

TEST(RenderTextField, HandleKeyBackspace)
{
    auto ctrl = std::make_shared<cw::TextEditingController>("hello");
    cw::RenderTextField field;
    field.controller = ctrl;

    cw::KeyEvent ev;
    ev.kind     = cw::KeyEventKind::down;
    ev.key_code = cw::KeyCode::backspace;
    field.handleKeyEvent(ev);

    EXPECT_EQ(ctrl->text(), "hell");
}

TEST(RenderTextField, HandleKeyDeleteForward)
{
    auto ctrl = std::make_shared<cw::TextEditingController>("hello");
    ctrl->setSelection(0, 0);
    cw::RenderTextField field;
    field.controller = ctrl;

    cw::KeyEvent ev;
    ev.kind     = cw::KeyEventKind::down;
    ev.key_code = cw::KeyCode::delete_forward;
    field.handleKeyEvent(ev);

    EXPECT_EQ(ctrl->text(), "ello");
}

TEST(RenderTextField, HandleKeyArrowRight)
{
    auto ctrl = std::make_shared<cw::TextEditingController>("hello");
    ctrl->setSelection(0, 0);
    cw::RenderTextField field;
    field.controller = ctrl;

    cw::KeyEvent ev;
    ev.kind     = cw::KeyEventKind::down;
    ev.key_code = cw::KeyCode::right;
    field.handleKeyEvent(ev);

    EXPECT_EQ(ctrl->selectionEnd(), 1);
}

TEST(RenderTextField, HandleKeyArrowLeft)
{
    auto ctrl = std::make_shared<cw::TextEditingController>("hello");
    // Cursor at end (position 5)
    cw::RenderTextField field;
    field.controller = ctrl;

    cw::KeyEvent ev;
    ev.kind     = cw::KeyEventKind::down;
    ev.key_code = cw::KeyCode::left;
    field.handleKeyEvent(ev);

    EXPECT_EQ(ctrl->selectionEnd(), 4);
}

TEST(RenderTextField, HandleKeyHome)
{
    auto ctrl = std::make_shared<cw::TextEditingController>("hello");
    cw::RenderTextField field;
    field.controller = ctrl;

    cw::KeyEvent ev;
    ev.kind     = cw::KeyEventKind::down;
    ev.key_code = cw::KeyCode::home;
    field.handleKeyEvent(ev);

    EXPECT_EQ(ctrl->selectionEnd(), 0);
}

TEST(RenderTextField, HandleKeyEnd)
{
    auto ctrl = std::make_shared<cw::TextEditingController>("hello");
    ctrl->setSelection(0, 0);
    cw::RenderTextField field;
    field.controller = ctrl;

    cw::KeyEvent ev;
    ev.kind     = cw::KeyEventKind::down;
    ev.key_code = cw::KeyCode::end;
    field.handleKeyEvent(ev);

    EXPECT_EQ(ctrl->selectionEnd(), 5);
}

TEST(RenderTextField, HandleKeyUpIsNotConsumed)
{
    cw::RenderTextField field;
    cw::KeyEvent ev;
    ev.kind     = cw::KeyEventKind::up;
    ev.key_code = cw::KeyCode::backspace;
    EXPECT_FALSE(field.handleKeyEvent(ev));
}

TEST(RenderTextField, HandleKeyCtrlA_SelectAll)
{
    auto ctrl = std::make_shared<cw::TextEditingController>("hello");
    ctrl->setSelection(2, 2);
    cw::RenderTextField field;
    field.controller = ctrl;

    cw::KeyEvent ev;
    ev.kind      = cw::KeyEventKind::down;
    ev.key_code  = cw::KeyCode::a;
    ev.modifiers = cw::KeyModifiers::ctrl;
    field.handleKeyEvent(ev);

    EXPECT_EQ(ctrl->selectionStart(), 0);
    EXPECT_EQ(ctrl->selectionEnd(),   5);
}

TEST(RenderTextField, HandleKeyShiftRight_ExtendsSelection)
{
    auto ctrl = std::make_shared<cw::TextEditingController>("hello");
    ctrl->setSelection(1, 1);
    cw::RenderTextField field;
    field.controller = ctrl;

    cw::KeyEvent ev;
    ev.kind      = cw::KeyEventKind::down;
    ev.key_code  = cw::KeyCode::right;
    ev.modifiers = cw::KeyModifiers::shift;
    field.handleKeyEvent(ev);

    EXPECT_EQ(ctrl->selectionStart(), 1);
    EXPECT_EQ(ctrl->selectionEnd(),   2);
    EXPECT_TRUE(ctrl->hasSelection());
}

TEST(RenderTextField, OnChangedCalledOnInsert)
{
    auto ctrl = std::make_shared<cw::TextEditingController>();
    cw::RenderTextField field;
    field.controller = ctrl;

    std::string last;
    field.on_changed = [&](const std::string& t){ last = t; };

    cw::KeyEvent ev;
    ev.kind      = cw::KeyEventKind::down;
    ev.key_code  = cw::KeyCode::unknown;
    ev.character = 'x';
    field.handleKeyEvent(ev);

    EXPECT_EQ(last, "x");
}

TEST(RenderTextField, OnSubmittedCalledOnEnter)
{
    auto ctrl = std::make_shared<cw::TextEditingController>("done");
    cw::RenderTextField field;
    field.controller = ctrl;

    std::string submitted;
    field.on_submitted = [&](const std::string& t){ submitted = t; };

    cw::KeyEvent ev;
    ev.kind     = cw::KeyEventKind::down;
    ev.key_code = cw::KeyCode::enter;
    field.handleKeyEvent(ev);

    EXPECT_EQ(submitted, "done");
}
