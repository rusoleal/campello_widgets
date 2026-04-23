#include <campello_widgets/campello_widgets.hpp>
#include <campello_widgets/linux/run_app.hpp>

namespace cw = systems::leal::campello_widgets;

// Simple app with a TextField to demonstrate IME composition on Linux.
class HelloLinuxApp : public cw::StatelessWidget
{
public:
    cw::WidgetRef build(cw::BuildContext&) const override
    {
        auto title_style = cw::TextStyle{};
        title_style.font_size = 24.0f;
        title_style.color = cw::Color::fromRGB(0.1f, 0.1f, 0.1f);

        auto hint_style = cw::TextStyle{};
        hint_style.font_size = 14.0f;
        hint_style.color = cw::Color::fromRGB(0.5f, 0.5f, 0.5f);

        auto edit_controller = std::make_shared<cw::TextEditingController>();

        auto text_field = std::make_shared<cw::TextField>();
        text_field->controller = edit_controller;
        text_field->style.font_size = 16.0f;
        text_field->placeholder = "Type here (IME supported)…";

        auto col = cw::mw<cw::Column>(
            cw::MainAxisAlignment::center,
            cw::CrossAxisAlignment::stretch,
            cw::WidgetList{
                cw::mw<cw::Center>(cw::mw<cw::Text>("Hello Linux!", title_style)),
                cw::mw<cw::Padding>(
                    cw::EdgeInsets::symmetric(40.0f, 16.0f),
                    cw::mw<cw::Text>("campello_widgets + X11 + IBus IME", hint_style)),
                cw::mw<cw::Padding>(
                    cw::EdgeInsets::all(24.0f),
                    text_field),
            }
        );

        auto bg = std::make_shared<cw::Container>();
        bg->color = cw::Color::white();
        bg->child = col;
        return bg;
    }
};

int main()
{
    return cw::runApp(
        "campello_widgets — Linux Hello",
        800, 600,
        std::make_shared<HelloLinuxApp>());
}
