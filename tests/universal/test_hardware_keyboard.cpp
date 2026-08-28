#include <gtest/gtest.h>
#include <campello_widgets/ui/hardware_keyboard.hpp>

namespace cw = systems::leal::campello_widgets;

// HardwareKeyboard::current() is a true process-wide singleton (mirrors
// Flutter's HardwareKeyboard.instance) -- every test must reset it first so
// state from an earlier test (or platform-adapter code exercised elsewhere
// in this binary) doesn't leak in.
namespace
{
    struct ResetHardwareKeyboard
    {
        ResetHardwareKeyboard() { cw::HardwareKeyboard::current().updateModifiers(cw::KeyModifiers::none); }
        ~ResetHardwareKeyboard() { cw::HardwareKeyboard::current().updateModifiers(cw::KeyModifiers::none); }
    };
}

TEST(HardwareKeyboard, DefaultsToNoModifiers)
{
    ResetHardwareKeyboard guard;
    EXPECT_EQ(cw::HardwareKeyboard::current().modifiers(), cw::KeyModifiers::none);
    EXPECT_FALSE(cw::HardwareKeyboard::current().isShiftPressed());
    EXPECT_FALSE(cw::HardwareKeyboard::current().isControlPressed());
    EXPECT_FALSE(cw::HardwareKeyboard::current().isAltPressed());
    EXPECT_FALSE(cw::HardwareKeyboard::current().isMetaPressed());
}

TEST(HardwareKeyboard, UpdateModifiersReflectsEachBitIndependently)
{
    ResetHardwareKeyboard guard;
    auto& kb = cw::HardwareKeyboard::current();

    kb.updateModifiers(cw::KeyModifiers::shift);
    EXPECT_TRUE(kb.isShiftPressed());
    EXPECT_FALSE(kb.isControlPressed());
    EXPECT_FALSE(kb.isAltPressed());
    EXPECT_FALSE(kb.isMetaPressed());

    kb.updateModifiers(cw::KeyModifiers::ctrl);
    EXPECT_FALSE(kb.isShiftPressed());
    EXPECT_TRUE(kb.isControlPressed());

    kb.updateModifiers(cw::KeyModifiers::alt);
    EXPECT_TRUE(kb.isAltPressed());
    EXPECT_FALSE(kb.isControlPressed());

    kb.updateModifiers(cw::KeyModifiers::meta);
    EXPECT_TRUE(kb.isMetaPressed());
    EXPECT_FALSE(kb.isAltPressed());
}

TEST(HardwareKeyboard, UpdateModifiersHandlesCombinations)
{
    ResetHardwareKeyboard guard;
    auto& kb = cw::HardwareKeyboard::current();

    kb.updateModifiers(cw::KeyModifiers::meta | cw::KeyModifiers::shift);
    EXPECT_TRUE(kb.isMetaPressed());
    EXPECT_TRUE(kb.isShiftPressed());
    EXPECT_FALSE(kb.isControlPressed());
    EXPECT_FALSE(kb.isAltPressed());
    EXPECT_EQ(kb.modifiers(), cw::KeyModifiers::meta | cw::KeyModifiers::shift);
}

TEST(HardwareKeyboard, UpdateModifiersReplacesNotAccumulates)
{
    // Each call is a full snapshot (matching how the platform layer always
    // reports the live, complete modifier state), not a delta -- releasing
    // one modifier while another is still held must clear only the
    // released one, not require an explicit "clear" call.
    ResetHardwareKeyboard guard;
    auto& kb = cw::HardwareKeyboard::current();

    kb.updateModifiers(cw::KeyModifiers::meta | cw::KeyModifiers::ctrl);
    ASSERT_TRUE(kb.isMetaPressed());
    ASSERT_TRUE(kb.isControlPressed());

    kb.updateModifiers(cw::KeyModifiers::meta);
    EXPECT_TRUE(kb.isMetaPressed());
    EXPECT_FALSE(kb.isControlPressed());

    kb.updateModifiers(cw::KeyModifiers::none);
    EXPECT_FALSE(kb.isMetaPressed());
}

TEST(HardwareKeyboard, IsASingleProcessWideInstance)
{
    ResetHardwareKeyboard guard;
    cw::HardwareKeyboard::current().updateModifiers(cw::KeyModifiers::alt);
    EXPECT_TRUE(cw::HardwareKeyboard::current().isAltPressed());
    EXPECT_EQ(&cw::HardwareKeyboard::current(), &cw::HardwareKeyboard::current());
}
