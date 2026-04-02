#include <campello_widgets/campello_widgets.hpp>
#include <campello_widgets/macos/run_app.hpp>

#include <string>
#include <vector>
#include <algorithm>

namespace cw = systems::leal::campello_widgets;

// ---------------------------------------------------------------------------
// Key name helpers
// ---------------------------------------------------------------------------

static std::string keyCodeName(cw::KeyCode code)
{
    using K = cw::KeyCode;
    switch (code)
    {
        case K::a: return "A";  case K::b: return "B";  case K::c: return "C";
        case K::d: return "D";  case K::e: return "E";  case K::f: return "F";
        case K::g: return "G";  case K::h: return "H";  case K::i: return "I";
        case K::j: return "J";  case K::k: return "K";  case K::l: return "L";
        case K::m: return "M";  case K::n: return "N";  case K::o: return "O";
        case K::p: return "P";  case K::q: return "Q";  case K::r: return "R";
        case K::s: return "S";  case K::t: return "T";  case K::u: return "U";
        case K::v: return "V";  case K::w: return "W";  case K::x: return "X";
        case K::y: return "Y";  case K::z: return "Z";

        case K::digit_0: return "0";  case K::digit_1: return "1";
        case K::digit_2: return "2";  case K::digit_3: return "3";
        case K::digit_4: return "4";  case K::digit_5: return "5";
        case K::digit_6: return "6";  case K::digit_7: return "7";
        case K::digit_8: return "8";  case K::digit_9: return "9";

        case K::space:          return "Space";
        case K::enter:          return "Return";
        case K::tab:            return "Tab";
        case K::backspace:      return "Backspace";
        case K::escape:         return "Escape";
        case K::delete_forward: return "Delete";

        case K::left:      return "Left";
        case K::right:     return "Right";
        case K::up:        return "Up";
        case K::down:      return "Down";
        case K::home:      return "Home";
        case K::end:       return "End";
        case K::page_up:   return "PageUp";
        case K::page_down: return "PageDown";

        case K::f1:  return "F1";  case K::f2:  return "F2";
        case K::f3:  return "F3";  case K::f4:  return "F4";
        case K::f5:  return "F5";  case K::f6:  return "F6";
        case K::f7:  return "F7";  case K::f8:  return "F8";
        case K::f9:  return "F9";  case K::f10: return "F10";
        case K::f11: return "F11"; case K::f12: return "F12";

        case K::left_shift:  return "Shift";
        case K::right_shift: return "Shift";
        case K::left_ctrl:   return "Ctrl";
        case K::right_ctrl:  return "Ctrl";
        case K::left_alt:    return "Option";
        case K::right_alt:   return "Option";
        case K::left_meta:   return "Cmd";
        case K::right_meta:  return "Cmd";
        case K::caps_lock:   return "CapsLock";

        default: return "?";
    }
}

static std::string kindLabel(cw::KeyEventKind kind)
{
    switch (kind)
    {
        case cw::KeyEventKind::down:   return "down";
        case cw::KeyEventKind::up:     return "up";
        case cw::KeyEventKind::repeat: return "repeat";
    }
    return "";
}

static std::string modifierPrefix(uint32_t mods)
{
    std::string s;
    if (mods & cw::KeyModifiers::meta)  s += "Cmd+";
    if (mods & cw::KeyModifiers::ctrl)  s += "Ctrl+";
    if (mods & cw::KeyModifiers::alt)   s += "Opt+";
    if (mods & cw::KeyModifiers::shift) s += "Shift+";
    return s;
}

// ---------------------------------------------------------------------------
// KeyboardApp
// ---------------------------------------------------------------------------

static constexpr std::size_t kMaxLog = 6;

class KeyboardApp;

class KeyboardState : public cw::State<KeyboardApp>
{
public:
    void initState() override
    {
        node_ = std::make_shared<cw::FocusNode>();
    }

    cw::WidgetRef build(cw::BuildContext&) override
    {
        // Styles
        cw::TextStyle labelStyle{};
        labelStyle.font_family = "Helvetica Neue";
        labelStyle.font_size   = 13.0f;
        labelStyle.color       = cw::Color::fromRGB(0.55f, 0.55f, 0.60f);

        cw::TextStyle keyStyle{};
        keyStyle.font_family = "Helvetica Neue";
        keyStyle.font_size   = 72.0f;
        keyStyle.color       = cw::Color::fromRGB(0.08f, 0.47f, 0.95f);

        cw::TextStyle downStyle = keyStyle;
        downStyle.color = cw::Color::fromRGB(0.08f, 0.47f, 0.95f);

        cw::TextStyle upStyle = keyStyle;
        upStyle.color = cw::Color::fromRGB(0.70f, 0.70f, 0.75f);

        cw::TextStyle repeatStyle = keyStyle;
        repeatStyle.color = cw::Color::fromRGB(0.08f, 0.47f, 0.95f);
        repeatStyle.font_size = 64.0f;

        cw::TextStyle typedStyle{};
        typedStyle.font_family = "Helvetica Neue";
        typedStyle.font_size   = 22.0f;
        typedStyle.color       = cw::Color::fromRGB(0.15f, 0.15f, 0.20f);

        cw::TextStyle logStyle{};
        logStyle.font_family = "Helvetica Neue";
        logStyle.font_size   = 12.0f;
        logStyle.color       = cw::Color::fromRGB(0.60f, 0.60f, 0.65f);

        // Last key display
        const std::string keyName = last_key_.empty()
            ? "press a key"
            : modifierPrefix(last_mods_) + last_key_;

        cw::TextStyle& activeKeyStyle =
            last_kind_ == cw::KeyEventKind::up     ? upStyle :
            last_kind_ == cw::KeyEventKind::repeat  ? repeatStyle : downStyle;

        auto key_display = cw::mw<cw::Text>(keyName, activeKeyStyle);

        // Kind badge
        cw::TextStyle badgeStyle{};
        badgeStyle.font_family = "Helvetica Neue";
        badgeStyle.font_size   = 13.0f;
        badgeStyle.color       = cw::Color::fromRGB(0.55f, 0.55f, 0.60f);

        auto kind_text = cw::mw<cw::Text>(
            last_key_.empty() ? "" : kindLabel(last_kind_), badgeStyle);

        // Typed text line
        const std::string typed_display = typed_.empty() ? "typed text appears here" : typed_;
        cw::TextStyle activeTypedStyle = typedStyle;
        if (typed_.empty())
            activeTypedStyle.color = cw::Color::fromRGB(0.75f, 0.75f, 0.78f);

        auto typed_row = cw::mw<cw::Text>(typed_display, activeTypedStyle);

        // Event log — build children into a vector, then pass to Column
        std::vector<cw::WidgetRef> log_items;
        for (auto it = log_.rbegin(); it != log_.rend(); ++it)
            log_items.push_back(cw::mw<cw::Text>(*it, logStyle));

        auto log_col = std::make_shared<cw::Column>();
        log_col->main_axis_alignment  = cw::MainAxisAlignment::start;
        log_col->cross_axis_alignment = cw::CrossAxisAlignment::start;
        log_col->main_axis_size       = cw::MainAxisSize::min;
        log_col->children             = std::move(log_items);

        // Layout — inner columns must use MainAxisSize::min so they size to
        // their content rather than filling all available vertical space.
        auto center_col = std::make_shared<cw::Column>();
        center_col->main_axis_alignment  = cw::MainAxisAlignment::center;
        center_col->cross_axis_alignment = cw::CrossAxisAlignment::center;
        center_col->main_axis_size       = cw::MainAxisSize::min;
        center_col->children = {
            cw::mw<cw::Text>("last key event", labelStyle),
            cw::mw<cw::SizedBox>(std::nullopt, 8.0f),
            key_display,
            cw::mw<cw::SizedBox>(std::nullopt, 4.0f),
            kind_text,
            cw::mw<cw::SizedBox>(std::nullopt, 40.0f),
            cw::mw<cw::Text>("typed text", labelStyle),
            cw::mw<cw::SizedBox>(std::nullopt, 8.0f),
            typed_row,
        };

        auto log_section = std::make_shared<cw::Column>();
        log_section->main_axis_alignment  = cw::MainAxisAlignment::start;
        log_section->cross_axis_alignment = cw::CrossAxisAlignment::start;
        log_section->main_axis_size       = cw::MainAxisSize::min;
        log_section->children = {
            cw::mw<cw::Text>("event log", labelStyle),
            cw::mw<cw::SizedBox>(std::nullopt, 6.0f),
            log_col,
        };

        auto root = cw::mw<cw::Column>(
            cw::MainAxisAlignment::start,
            cw::CrossAxisAlignment::stretch,
            cw::WidgetList{
                cw::mw<cw::Expanded>(cw::mw<cw::Center>(center_col)),
                cw::mw<cw::Padding>(
                    cw::EdgeInsets::only(32.0f, 0.0f, 32.0f, 32.0f),
                    log_section),
            });

        auto bg = std::make_shared<cw::Container>();
        bg->color = cw::Color::fromRGB(0.97f, 0.97f, 1.0f);
        bg->child = root;

        // Wrap in KeyboardListener
        auto listener = std::make_shared<cw::KeyboardListener>();
        listener->focus_node   = node_;
        listener->auto_focus   = true;
        listener->on_key_event = [this](const cw::KeyEvent& e) { handleKey(e); };
        listener->child        = bg;
        return listener;
    }

private:
    void handleKey(const cw::KeyEvent& e)
    {
        setState([this, e] {
            last_key_  = keyCodeName(e.key_code);
            last_kind_ = e.kind;
            last_mods_ = e.modifiers;

            // Append to event log
            const std::string entry =
                modifierPrefix(e.modifiers) + keyCodeName(e.key_code)
                + "  [" + kindLabel(e.kind) + "]";
            log_.push_back(entry);
            if (log_.size() > kMaxLog)
                log_.erase(log_.begin());

            // Build typed text from down/repeat events with a printable character
            if (e.kind != cw::KeyEventKind::up && e.character != 0)
            {
                if (e.key_code == cw::KeyCode::backspace)
                {
                    if (!typed_.empty())
                        typed_.pop_back();
                }
                else if (e.character >= 0x20 && e.character < 0x7F)
                {
                    typed_ += static_cast<char>(e.character);
                }
            }
        });
    }

    std::shared_ptr<cw::FocusNode> node_;

    std::string           last_key_;
    cw::KeyEventKind      last_kind_  = cw::KeyEventKind::down;
    uint32_t              last_mods_  = cw::KeyModifiers::none;
    std::string           typed_;
    std::vector<std::string> log_;
};

class KeyboardApp : public cw::StatefulWidget
{
public:
    std::unique_ptr<cw::StateBase> createState() const override
    {
        return std::make_unique<KeyboardState>();
    }
};

// ---------------------------------------------------------------------------
// Entry point
// ---------------------------------------------------------------------------

int main()
{
    cw::DebugFlags::showDebugBanner = false;

    return cw::runApp(
        std::make_shared<KeyboardApp>(),
        "campello_widgets — KeyboardListener",
        480.0f,
        560.0f);
}
