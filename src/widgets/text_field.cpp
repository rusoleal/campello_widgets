#include <campello_widgets/widgets/text_field.hpp>
#include <campello_widgets/ui/render_text_field.hpp>
#include <campello_widgets/ui/text_editing_controller.hpp>
#include <campello_widgets/ui/focus_node.hpp>
#include <campello_widgets/ui/focus_manager.hpp>
#include <campello_widgets/widgets/single_child_render_object_widget.hpp>
#include <campello_widgets/widgets/stateful_widget.hpp>

#include <algorithm>
#include <string_view>

namespace systems::leal::campello_widgets
{

    // -------------------------------------------------------------------------
    // Internal proxy widget → RenderTextField
    // -------------------------------------------------------------------------

    class TextFieldProxy : public SingleChildRenderObjectWidget
    {
    public:
        std::shared_ptr<TextEditingController> controller;

        TextStyle   style;
        Color       cursor_color;
        Color       selection_color;
        Color       fill_color;
        Color       border_color;
        Color       focused_border_color;
        Color       placeholder_color;
        std::string placeholder;
        float       border_radius = 4.0f;
        float       border_width  = 1.0f;
        float       min_height    = 36.0f;
        bool        focused       = false;
        bool        obscure_text  = false;

        std::function<void(const std::string&)> on_changed;
        std::function<void(const std::string&)> on_submitted;
        std::function<void()>                   on_tap;

        std::shared_ptr<RenderObject> createRenderObject() const override
        {
            auto r = std::make_shared<RenderTextField>();
            applyTo(*r);
            return r;
        }

        void updateRenderObject(RenderObject& ro) const override
        {
            applyTo(static_cast<RenderTextField&>(ro));
            static_cast<RenderTextField&>(ro).markNeedsPaint();
        }

    private:
        void applyTo(RenderTextField& r) const
        {
            r.controller           = controller;
            r.style                = style;
            r.cursor_color         = cursor_color;
            r.selection_color      = selection_color;
            r.fill_color           = fill_color;
            r.border_color         = border_color;
            r.focused_border_color = focused_border_color;
            r.placeholder_color    = placeholder_color;
            r.placeholder          = placeholder;
            r.border_radius        = border_radius;
            r.border_width         = border_width;
            r.min_height           = min_height;
            r.focused              = focused;
            r.obscure_text         = obscure_text;
            r.on_changed           = on_changed;
            r.on_submitted         = on_submitted;
            r.on_tap               = on_tap;
        }
    };

    // -------------------------------------------------------------------------
    // State
    // -------------------------------------------------------------------------

    class TextFieldState : public State<TextField>
    {
    public:
        void initState() override
        {
            const auto& w = widget();

            // Controller
            if (w.controller)
            {
                ctrl_ = w.controller;
            }
            else
            {
                own_ctrl_ = std::make_shared<TextEditingController>();
                ctrl_     = own_ctrl_;
            }

            // FocusNode
            if (w.focus_node)
            {
                focus_node_ = w.focus_node;
            }
            else
            {
                own_focus_node_ = std::make_shared<FocusNode>();
                focus_node_     = own_focus_node_;
            }

            // Listen to controller → rebuild on text/selection change
            ctrl_listener_id_ = ctrl_->addListener([this]() {
                setState([](){});
            });

            // Listen for focus changes → rebuild (border color, cursor visibility)
            focus_node_->on_focus_changed = [this](bool has_focus) {
                setState([this, has_focus]() { focused_ = has_focus; });
            };

            // Handle keyboard events directly in the State
            focus_node_->on_key = [this](const KeyEvent& event) -> bool {
                return handleKey(event);
            };

            focused_ = focus_node_->hasFocus();
        }

        void dispose() override
        {
            if (ctrl_ && ctrl_listener_id_ != 0)
                ctrl_->removeListener(ctrl_listener_id_);
            focus_node_->on_key           = nullptr;
            focus_node_->on_focus_changed = nullptr;
        }

        void didUpdateWidget(const Widget& old_base) override
        {
            (void)old_base;
            const auto& new_w = widget();

            // Controller swap
            std::shared_ptr<TextEditingController> new_ctrl =
                new_w.controller ? new_w.controller : own_ctrl_;

            if (new_ctrl.get() != ctrl_.get())
            {
                if (ctrl_ && ctrl_listener_id_ != 0)
                    ctrl_->removeListener(ctrl_listener_id_);
                ctrl_ = new_ctrl;
                ctrl_listener_id_ = ctrl_->addListener([this]() {
                    setState([](){});
                });
            }

            // FocusNode swap
            std::shared_ptr<FocusNode> new_fn =
                new_w.focus_node ? new_w.focus_node : own_focus_node_;

            if (new_fn.get() != focus_node_.get())
            {
                focus_node_->on_key           = nullptr;
                focus_node_->on_focus_changed = nullptr;
                focus_node_ = new_fn;
                focus_node_->on_focus_changed = [this](bool has_focus) {
                    setState([this, has_focus]() { focused_ = has_focus; });
                };
                focus_node_->on_key = [this](const KeyEvent& event) -> bool {
                    return handleKey(event);
                };
            }
        }

        WidgetRef build(BuildContext&) override
        {
            const auto& w = widget();

            auto proxy = std::make_shared<TextFieldProxy>();
            proxy->controller           = ctrl_;
            proxy->style                = w.style;
            proxy->cursor_color         = w.cursor_color;
            proxy->selection_color      = w.selection_color;
            proxy->fill_color           = w.fill_color;
            proxy->border_color         = w.border_color;
            proxy->focused_border_color = w.focused_border_color;
            proxy->placeholder_color    = w.placeholder_color;
            proxy->placeholder          = w.placeholder;
            proxy->border_radius        = w.border_radius;
            proxy->border_width         = w.border_width;
            proxy->min_height           = w.min_height;
            proxy->focused              = focused_;
            proxy->obscure_text         = w.obscure_text;
            proxy->on_changed           = w.on_changed;
            proxy->on_submitted         = w.on_submitted;
            proxy->on_tap               = [this]() { focus_node_->requestFocus(); };

            return proxy;
        }

    private:
        // ------------------------------------------------------------------
        // Key handling (called from FocusNode::on_key)
        // ------------------------------------------------------------------

        bool handleKey(const KeyEvent& event)
        {
            if (!ctrl_) return false;
            if (event.kind == KeyEventKind::up) return false;

            const bool shift = (event.modifiers & KeyModifiers::shift) != 0;
            const bool ctrl  = (event.modifiers & KeyModifiers::ctrl)  != 0
                            || (event.modifiers & KeyModifiers::meta)  != 0;

            switch (event.key_code)
            {
            case KeyCode::backspace:
                ctrl_->deleteBackward();
                notifyChanged();
                return true;

            case KeyCode::delete_forward:
                ctrl_->deleteForward();
                notifyChanged();
                return true;

            case KeyCode::enter:
                if (widget().on_submitted)
                    widget().on_submitted(ctrl_->text());
                return true;

            case KeyCode::left:
            {
                int pos = ctrl_->selectionEnd();
                if (ctrl)
                {
                    const auto& t = ctrl_->text();
                    while (pos > 0 && t[static_cast<size_t>(pos - 1)] == ' ') --pos;
                    while (pos > 0 && t[static_cast<size_t>(pos - 1)] != ' ') --pos;
                }
                else
                {
                    pos = std::max(0, pos - 1);
                }
                ctrl_->setSelection(shift ? ctrl_->selectionStart() : pos, pos);
                return true;
            }

            case KeyCode::right:
            {
                int pos = ctrl_->selectionEnd();
                int sz  = static_cast<int>(ctrl_->text().size());
                if (ctrl)
                {
                    const auto& t = ctrl_->text();
                    while (pos < sz && t[static_cast<size_t>(pos)] != ' ') ++pos;
                    while (pos < sz && t[static_cast<size_t>(pos)] == ' ') ++pos;
                }
                else
                {
                    pos = std::min(sz, pos + 1);
                }
                ctrl_->setSelection(shift ? ctrl_->selectionStart() : pos, pos);
                return true;
            }

            case KeyCode::home:
                ctrl_->setSelection(shift ? ctrl_->selectionStart() : 0, 0);
                return true;

            case KeyCode::end:
            {
                int end = static_cast<int>(ctrl_->text().size());
                ctrl_->setSelection(shift ? ctrl_->selectionStart() : end, end);
                return true;
            }

            case KeyCode::a:
                if (ctrl) { ctrl_->selectAll(); return true; }
                break;

            default:
                break;
            }

            // Printable character — encode Unicode codepoint to UTF-8
            if (event.character != 0)
            {
                char buf[5] = {};
                uint32_t cp = event.character;
                if (cp < 0x80)
                {
                    buf[0] = static_cast<char>(cp);
                }
                else if (cp < 0x800)
                {
                    buf[0] = static_cast<char>(0xC0 | (cp >> 6));
                    buf[1] = static_cast<char>(0x80 | (cp & 0x3F));
                }
                else if (cp < 0x10000)
                {
                    buf[0] = static_cast<char>(0xE0 | (cp >> 12));
                    buf[1] = static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
                    buf[2] = static_cast<char>(0x80 | (cp & 0x3F));
                }
                else
                {
                    buf[0] = static_cast<char>(0xF0 | (cp >> 18));
                    buf[1] = static_cast<char>(0x80 | ((cp >> 12) & 0x3F));
                    buf[2] = static_cast<char>(0x80 | ((cp >> 6)  & 0x3F));
                    buf[3] = static_cast<char>(0x80 | (cp & 0x3F));
                }
                ctrl_->insertText(std::string_view(buf));
                notifyChanged();
                return true;
            }

            return false;
        }

        void notifyChanged()
        {
            if (widget().on_changed) widget().on_changed(ctrl_->text());
        }

        // ------------------------------------------------------------------

        std::shared_ptr<TextEditingController> ctrl_;
        std::shared_ptr<TextEditingController> own_ctrl_;   ///< null if controller provided
        uint64_t ctrl_listener_id_ = 0;

        std::shared_ptr<FocusNode> focus_node_;
        std::shared_ptr<FocusNode> own_focus_node_;         ///< null if focus_node provided

        bool focused_ = false;
    };

    // -------------------------------------------------------------------------

    std::unique_ptr<StateBase> TextField::createState() const
    {
        return std::make_unique<TextFieldState>();
    }

} // namespace systems::leal::campello_widgets
