#include <campello_widgets/campello_widgets.hpp>
#include <campello_widgets/macos/run_app.hpp>

namespace cw = systems::leal::campello_widgets;

// ---------------------------------------------------------------------------
// TextFieldDemoApp - Demonstrates multiline TextField with scrolling
// ---------------------------------------------------------------------------

class TextFieldDemoApp;

class TextFieldDemoState : public cw::State<TextFieldDemoApp>
{
public:
    void initState() override
    {
        // Create controllers for the text fields
        singleController_ = std::make_shared<cw::TextEditingController>();
        multiController_ = std::make_shared<cw::TextEditingController>();
        
        // Pre-fill multiline text to demonstrate scrolling
        multiController_->setText(
            "This is a multiline TextField demo.\n"
            "You can type multiple lines of text here.\n"
            "Use Enter to create new lines.\n"
            "The text will scroll when it exceeds the visible area.\n"
            "\n"
            "Try scrolling with your mouse wheel or trackpad!\n"
            "\n"
            "Line 7: Lorem ipsum dolor sit amet.\n"
            "Line 8: Consectetur adipiscing elit.\n"
            "Line 9: Sed do eiusmod tempor incididunt.\n"
            "Line 10: Ut labore et dolore magna aliqua.\n"
            "Line 11: Ut enim ad minim veniam.\n"
            "Line 12: Quis nostrud exercitation ullamco.\n"
            "Line 13: Laboris nisi ut aliquip ex ea commodo.\n"
            "Line 14: Duis aute irure dolor in reprehenderit.\n"
            "Line 15: Voluptate velit esse cillum dolore."
        );
    }

    cw::WidgetRef build(cw::BuildContext&) override
    {
        cw::TextStyle titleStyle{};
        titleStyle.font_family = "Helvetica Neue";
        titleStyle.font_size   = 24.0f;
        titleStyle.font_weight = cw::FontWeight::bold;
        titleStyle.color       = cw::Color::fromRGB(0.2f, 0.2f, 0.2f);

        cw::TextStyle labelStyle{};
        labelStyle.font_family = "Helvetica Neue";
        labelStyle.font_size   = 14.0f;
        labelStyle.color       = cw::Color::fromRGB(0.5f, 0.5f, 0.5f);

        cw::TextStyle textFieldStyle{};
        textFieldStyle.font_family = "Helvetica Neue";
        textFieldStyle.font_size   = 16.0f;
        textFieldStyle.color       = cw::Color::fromRGB(0.2f, 0.2f, 0.2f);

        // Single-line TextField
        auto singleLabel = cw::mw<cw::Text>("Single-line TextField:", labelStyle);
        
        auto singleField = std::make_shared<cw::TextField>();
        singleField->controller = singleController_;
        singleField->placeholder = "Type something...";
        singleField->style = textFieldStyle;
        singleField->on_changed = [this](const std::string& text) {
            characterCount_ = static_cast<int>(text.size());
            setState([]{});
        };
        singleField->on_submitted = [](const std::string& text) {
            printf("Single-line submitted: %s\n", text.c_str());
        };

        // Multiline TextField with scrolling - wrapped in ConstrainedBox to limit height
        auto multiLabel = cw::mw<cw::Text>("Multiline TextField (max 8 lines, scrollable):", labelStyle);
        
        auto multiField = std::make_shared<cw::TextField>();
        multiField->controller = multiController_;
        multiField->placeholder = "Enter multiple lines of text...";
        multiField->style = textFieldStyle;
        multiField->max_lines = 8;    // Show up to 8 lines, then scroll
        multiField->min_lines = 4;    // At least 4 lines tall
        multiField->on_changed = [this](const std::string& text) {
            lineCount_ = static_cast<int>(std::count(text.begin(), text.end(), '\n')) + 1;
            setState([]{});
        };
        multiField->on_submitted = [](const std::string& text) {
            printf("Multiline submitted (Ctrl+Enter): %s\n", text.c_str());
        };

        // Wrap the multiline field in a ConstrainedBox to prevent it from expanding infinitely
        // Using 200px max height which should fit ~8 lines
        auto constrainedMultiField = std::make_shared<cw::ConstrainedBox>();
        constrainedMultiField->additional_constraints = cw::BoxConstraints(0.0f, 10000.0f, 150.0f, 200.0f);
        constrainedMultiField->child = multiField;

        // Character and line count display
        cw::TextStyle countStyle{};
        countStyle.font_family = "Helvetica Neue";
        countStyle.font_size   = 12.0f;
        countStyle.color       = cw::Color::fromRGB(0.6f, 0.6f, 0.6f);
        
        auto countText = cw::mw<cw::Text>(
            "Characters: " + std::to_string(characterCount_) + 
            " | Lines in multiline: " + std::to_string(lineCount_), 
            countStyle
        );

        // Instructions
        cw::TextStyle hintStyle{};
        hintStyle.font_family = "Helvetica Neue";
        hintStyle.font_size   = 12.0f;
        hintStyle.color       = cw::Color::fromRGB(0.7f, 0.7f, 0.7f);
        hintStyle.italic = true;
        
        auto hintText = cw::mw<cw::Text>(
            "Tip: Use mouse wheel or trackpad to scroll the multiline field. "
            "Press Ctrl+Enter to submit multiline text.",
            hintStyle
        );

        // Layout
        auto content = cw::mw<cw::Column>(
            cw::MainAxisAlignment::start,
            cw::CrossAxisAlignment::stretch,
            cw::WidgetList{
                cw::mw<cw::Text>("TextField Demo", titleStyle),
                cw::mw<cw::SizedBox>(std::nullopt, 24.0f),
                singleLabel,
                cw::mw<cw::SizedBox>(std::nullopt, 8.0f),
                singleField,
                cw::mw<cw::SizedBox>(std::nullopt, 32.0f),
                multiLabel,
                cw::mw<cw::SizedBox>(std::nullopt, 8.0f),
                constrainedMultiField,
                cw::mw<cw::SizedBox>(std::nullopt, 16.0f),
                countText,
                cw::mw<cw::SizedBox>(std::nullopt, 8.0f),
                hintText,
            }
        );

        auto padded = cw::mw<cw::Padding>(
            cw::EdgeInsets::all(32.0f),
            content
        );

        auto bg = std::make_shared<cw::Container>();
        bg->color = cw::Color::fromRGB(0.97f, 0.97f, 0.98f);
        bg->child = padded;
        return bg;
    }

private:
    std::shared_ptr<cw::TextEditingController> singleController_;
    std::shared_ptr<cw::TextEditingController> multiController_;
    int characterCount_ = 0;
    int lineCount_ = 15;  // Starting line count from prefilled text
};

class TextFieldDemoApp : public cw::StatefulWidget
{
public:
    std::unique_ptr<cw::StateBase> createState() const override
    {
        return std::make_unique<TextFieldDemoState>();
    }
};

// ---------------------------------------------------------------------------
// Entry point
// ---------------------------------------------------------------------------

int main()
{
    cw::DebugFlags::showDebugBanner        = false;
    cw::DebugFlags::showPerformanceOverlay = true;

    return cw::runApp(
        std::make_shared<TextFieldDemoApp>(),
        "campello_widgets — TextField Demo",
        600.0f,
        700.0f);
}
