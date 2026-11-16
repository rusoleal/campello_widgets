#include <gtest/gtest.h>

#include <campello_widgets/ui/shared_preferences.hpp>

namespace cw = systems::leal::campello_widgets;

namespace
{
    // An in-memory store: what the platform would keep.
    struct Memory
    {
        std::string text;
        int saves = 0;
        cw::SharedPreferences::Store store()
        {
            return {[this] { return text; }, [this](const std::string& t) { text = t; ++saves; }};
        }
    };
}

TEST(SharedPreferences, TypedValuesSurviveAReload)
{
    Memory memory;
    {
        cw::SharedPreferences prefs(memory.store());
        EXPECT_FALSE(prefs.getBool("dark").has_value());
        EXPECT_TRUE(prefs.setBool("dark", true));
        prefs.setInt("count", -42);
        prefs.setDouble("scale", 1.25);
        prefs.setString("name", "tab\there\nnew line \\ back");
        EXPECT_EQ(memory.saves, 4) << "every change is written";
    }
    cw::SharedPreferences again(memory.store());
    EXPECT_EQ(again.getBool("dark"), true);
    EXPECT_EQ(again.getInt("count"), -42);
    EXPECT_DOUBLE_EQ(again.getDouble("scale").value(), 1.25);
    EXPECT_EQ(again.getString("name"), "tab\there\nnew line \\ back");
    EXPECT_EQ(again.keys().size(), 4u);
}

TEST(SharedPreferences, WrongTypeIsAbsentAndIntsReadAsDoubles)
{
    Memory memory;
    cw::SharedPreferences prefs(memory.store());
    prefs.setInt("n", 3);
    EXPECT_FALSE(prefs.getString("n").has_value());
    EXPECT_FALSE(prefs.getBool("n").has_value());
    EXPECT_DOUBLE_EQ(prefs.getDouble("n").value(), 3.0);
    prefs.setString("n", "three");  // replaces, whatever the type
    EXPECT_EQ(prefs.getString("n"), "three");
    EXPECT_FALSE(prefs.getInt("n").has_value());
}

TEST(SharedPreferences, RemoveAndClear)
{
    Memory memory;
    cw::SharedPreferences prefs(memory.store());
    prefs.setBool("a", true);
    prefs.setBool("b", false);
    EXPECT_TRUE(prefs.remove("a"));
    EXPECT_FALSE(prefs.containsKey("a"));
    EXPECT_TRUE(prefs.containsKey("b"));
    EXPECT_TRUE(prefs.clear());
    EXPECT_TRUE(cw::SharedPreferences(memory.store()).keys().empty());
}

TEST(SharedPreferences, IgnoresLinesItDoesntUnderstand)
{
    const auto values = cw::SharedPreferences::decode("garbage\nb\tok\t1\nx\tunknown\tz\n");
    ASSERT_EQ(values.size(), 1u);
    EXPECT_EQ(std::get<bool>(values.at("ok")), true);
}

TEST(SharedPreferences, ThePlatformStoreLoads)
{
    // The app's own store exists on every platform (here, the test host's).
    auto store = cw::detail::platformPreferencesStore();
    EXPECT_TRUE(static_cast<bool>(store.load));
    EXPECT_TRUE(static_cast<bool>(store.save));
}
