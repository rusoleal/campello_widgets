#include <gtest/gtest.h>

#include <campello_widgets/ui/locale.hpp>
#include <campello_widgets/widgets/localizations.hpp>
#include <campello_widgets/widgets/sized_box.hpp>
#include <campello_widgets/widgets/stateless_widget.hpp>
#include <campello_widgets/widgets/element.hpp>

namespace cw = systems::leal::campello_widgets;

TEST(Locale, ParsesTagsAndPosixNames)
{
    EXPECT_EQ(cw::Locale::parse("es-ES"), (cw::Locale{"es", "", "ES"}));
    EXPECT_EQ(cw::Locale::parse("es_es.UTF-8@euro"), (cw::Locale{"es", "", "ES"}));
    EXPECT_EQ(cw::Locale::parse("zh-hans-cn"), (cw::Locale{"zh", "Hans", "CN"}));
    EXPECT_EQ(cw::Locale::parse("es-419"), (cw::Locale{"es", "", "419"}));
    EXPECT_EQ(cw::Locale::parse("EN"), (cw::Locale{"en", "", ""}));
    EXPECT_EQ(cw::Locale::parse("C"), (cw::Locale{"en", "", ""}));
    EXPECT_EQ(cw::Locale::parse("de-DE-u-ca-gregory"), (cw::Locale{"de", "", "DE"}));
    EXPECT_TRUE(cw::Locale::parse("").language.empty());
    EXPECT_EQ((cw::Locale{"zh", "Hans", "CN"}).toLanguageTag(), "zh-Hans-CN");
}

TEST(Locale, ResolvesAgainstSupported)
{
    const std::vector<cw::Locale> supported{{"en", "", ""}, {"es", "", ""}, {"pt", "", "BR"}};
    EXPECT_EQ(cw::resolveLocale({cw::Locale::parse("es-ES")}, supported).language, "es");
    EXPECT_EQ(cw::resolveLocale({cw::Locale::parse("fr-FR"), cw::Locale::parse("es-MX")}, supported).language, "es")
        << "the first preferred one supported";
    EXPECT_EQ(cw::resolveLocale({cw::Locale::parse("ja")}, supported).language, "en") << "the first supported";
    EXPECT_EQ(cw::resolveLocale({cw::Locale::parse("pt-PT")}, supported), (cw::Locale{"pt", "", "BR"}));
    EXPECT_TRUE(cw::resolveLocale({cw::Locale::parse("es")}, {}).language.empty());
}

TEST(Locale, SystemLocalesNeverEmpty)
{
    const auto locales = cw::Locale::systemLocales();
    ASSERT_FALSE(locales.empty());
    EXPECT_FALSE(locales.front().language.empty());
}

namespace
{
    struct Reader : cw::StatelessWidget
    {
        cw::Locale* seen;
        explicit Reader(cw::Locale* s) : seen(s) {}
        cw::WidgetRef build(cw::BuildContext& context) const override
        {
            *seen = cw::Localizations::localeOf(context);
            return std::make_shared<cw::SizedBox>();
        }
    };
}

TEST(Localizations, ProvidesTheLocaleBelowIt)
{
    cw::Locale seen;
    auto root = std::make_shared<cw::Localizations>(cw::Locale{"es", "", "ES"}, std::make_shared<Reader>(&seen))->createElement();
    root->mount(nullptr);
    EXPECT_EQ(seen, (cw::Locale{"es", "", "ES"}));

    cw::Locale fallback;
    auto lone = std::make_shared<Reader>(&fallback)->createElement();
    lone->mount(nullptr);
    EXPECT_EQ(fallback.language, "en") << "English without a Localizations";

    const cw::Localizations a(cw::Locale{"es", "", ""}, nullptr), b(cw::Locale{"en", "", ""}, nullptr), c(cw::Locale{"es", "", ""}, nullptr);
    EXPECT_TRUE(a.updateShouldNotify(b));
    EXPECT_FALSE(a.updateShouldNotify(c));
}

#include <campello_widgets/ui/widgets_localizations.hpp>

TEST(WidgetsLocalizations, EnglishSpanishAndFallback)
{
    EXPECT_EQ(cw::WidgetsLocalizations::forLocale({"en", "", ""}).more, "More");
    EXPECT_EQ(cw::WidgetsLocalizations::forLocale({"es", "", "MX"}).more, "M\xC3\xA1s");
    EXPECT_EQ(cw::WidgetsLocalizations::forLocale({"ja", "", ""}).cancel, "Cancel");
}
