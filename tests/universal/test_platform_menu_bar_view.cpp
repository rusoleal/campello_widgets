#include <gtest/gtest.h>

#include <campello_widgets/widgets/platform_menu_bar_view.hpp>

namespace cw = systems::leal::campello_widgets;

// ---------------------------------------------------------------------------
// detail::parseMenuShortcut()
// ---------------------------------------------------------------------------

TEST(ParseMenuShortcut, SingleModifierAndLetter)
{
    cw::KeyCode code;
    uint32_t    mods;
    ASSERT_TRUE(cw::detail::parseMenuShortcut("Ctrl+O", code, mods));
    EXPECT_EQ(code, cw::KeyCode::o);
    EXPECT_EQ(mods, cw::KeyModifiers::ctrl);
}

TEST(ParseMenuShortcut, CmdIsTreatedAsCtrl)
{
    cw::KeyCode ctrl_code, cmd_code;
    uint32_t    ctrl_mods, cmd_mods;
    ASSERT_TRUE(cw::detail::parseMenuShortcut("Ctrl+O", ctrl_code, ctrl_mods));
    ASSERT_TRUE(cw::detail::parseMenuShortcut("Cmd+O", cmd_code, cmd_mods));
    EXPECT_EQ(cmd_code, ctrl_code);
    EXPECT_EQ(cmd_mods, ctrl_mods);
    EXPECT_EQ(cmd_mods, cw::KeyModifiers::ctrl);
}

TEST(ParseMenuShortcut, MultipleModifiers)
{
    cw::KeyCode code;
    uint32_t    mods;
    ASSERT_TRUE(cw::detail::parseMenuShortcut("Ctrl+Shift+S", code, mods));
    EXPECT_EQ(code, cw::KeyCode::s);
    EXPECT_EQ(mods, cw::KeyModifiers::ctrl | cw::KeyModifiers::shift);
}

TEST(ParseMenuShortcut, AltAndFunctionKey)
{
    cw::KeyCode code;
    uint32_t    mods;
    ASSERT_TRUE(cw::detail::parseMenuShortcut("Alt+F4", code, mods));
    EXPECT_EQ(code, cw::KeyCode::f4);
    EXPECT_EQ(mods, cw::KeyModifiers::alt);
}

TEST(ParseMenuShortcut, Digit)
{
    cw::KeyCode code;
    uint32_t    mods;
    ASSERT_TRUE(cw::detail::parseMenuShortcut("Ctrl+1", code, mods));
    EXPECT_EQ(code, cw::KeyCode::digit_1);
    EXPECT_EQ(mods, cw::KeyModifiers::ctrl);
}

TEST(ParseMenuShortcut, NamedSpecialKeyNoModifier)
{
    cw::KeyCode code;
    uint32_t    mods;
    ASSERT_TRUE(cw::detail::parseMenuShortcut("Escape", code, mods));
    EXPECT_EQ(code, cw::KeyCode::escape);
    EXPECT_EQ(mods, cw::KeyModifiers::none);
}

TEST(ParseMenuShortcut, EnterAliasReturn)
{
    cw::KeyCode code;
    uint32_t    mods;
    ASSERT_TRUE(cw::detail::parseMenuShortcut("Ctrl+Return", code, mods));
    EXPECT_EQ(code, cw::KeyCode::enter);
}

TEST(ParseMenuShortcut, EmptyStringFails)
{
    cw::KeyCode code;
    uint32_t    mods;
    EXPECT_FALSE(cw::detail::parseMenuShortcut("", code, mods));
}

TEST(ParseMenuShortcut, UnknownModifierFails)
{
    cw::KeyCode code;
    uint32_t    mods;
    EXPECT_FALSE(cw::detail::parseMenuShortcut("Foo+O", code, mods));
}

TEST(ParseMenuShortcut, UnrepresentablePunctuationKeyFails)
{
    // This engine's KeyCode has no punctuation entries (no '=', '-', etc.),
    // so shortcuts like macOS's "Cmd+=" for zoom-in can't be represented.
    cw::KeyCode code;
    uint32_t    mods;
    EXPECT_FALSE(cw::detail::parseMenuShortcut("Cmd+=", code, mods));
}

TEST(ParseMenuShortcut, TrailingPlusWithNoKeyFails)
{
    cw::KeyCode code;
    uint32_t    mods;
    EXPECT_FALSE(cw::detail::parseMenuShortcut("Ctrl+", code, mods));
}
