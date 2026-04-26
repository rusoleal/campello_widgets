#include <campello_widgets/widgets/text_field.hpp>
#include <campello_widgets/widgets/theme.hpp>
#include <campello_widgets/ui/render_text_field.hpp>
#include <campello_widgets/ui/text_editing_controller.hpp>
#include <campello_widgets/ui/text_input_manager.hpp>
#include <campello_widgets/ui/focus_node.hpp>
#include <campello_widgets/ui/focus_manager.hpp>
#include <campello_widgets/widgets/single_child_render_object_widget.hpp>
#include <campello_widgets/widgets/stateful_widget.hpp>
#include <campello_widgets/widgets/stateful_element.hpp>
#include <campello_widgets/widgets/element.hpp>
#include <campello_widgets/widgets/render_object_element.hpp>

#include <algorithm>
#include <array>
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
        float       border_radius = 8.0f;
        float       border_width  = 1.0f;
        float       min_height    = 36.0f;
        bool        focused       = false;
        bool        obscure_text  = false;
        int         max_lines     = 1;
        int         min_lines     = 1;
        bool        expands       = false;

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
            auto& r = static_cast<RenderTextField&>(ro);
            bool was_multiline = r.isMultiline();
            int old_max_lines = r.max_lines;
            int old_min_lines = r.min_lines;
            bool old_expands = r.expands;
            applyTo(r);
            // If multiline state or line configuration changed, we need relayout
            if (was_multiline != r.isMultiline() ||
                old_max_lines != r.max_lines ||
                old_min_lines != r.min_lines ||
                old_expands != r.expands) {
                r.markNeedsLayout();
            } else {
                r.markNeedsPaint();
            }
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
            r.max_lines            = max_lines;
            r.min_lines            = min_lines;
            r.expands              = expands;
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
            // and register/unregister with TextInputManager for IME support
            focus_node_->on_focus_changed = [this](bool has_focus) {
                setState([this, has_focus]() { focused_ = has_focus; });
                
                // Register/unregister with TextInputManager for IME composition
                if (auto* tim = TextInputManager::activeManager())
                {
                    if (has_focus)
                    {
                        tim->registerInputTarget(makeInputTarget());
                    }
                    else
                    {
                        // Cancel any ongoing composition when losing focus
                        if (ctrl_->isComposing())
                        {
                            ctrl_->commitComposing();
                        }
                        tim->unregisterInputTarget();
                        printf("[TextField] Unregistered from TextInputManager\n");
                    }
                }
                // TextInputManager not available - IME won't work
            };

            // Handle keyboard events directly in the State
            focus_node_->on_key = [this](const KeyEvent& event) -> bool {
                return handleKey(event);
            };

            focused_ = focus_node_->hasFocus();
        }

        void dispose() override
        {
            // Unregister from TextInputManager
            if (auto* tim = TextInputManager::activeManager())
            {
                if (ctrl_->isComposing())
                {
                    ctrl_->commitComposing();
                }
                tim->unregisterInputTarget();
            }

            // Clear callbacks BEFORE unregistering from FocusManager.
            // unregisterNode() fires on_focus_changed(false); if the callback
            // is still bound it will call setState() on a State that is in the
            // middle of being disposed, which schedules a build on an element
            // whose render object has already been detached.
            focus_node_->on_focus_changed = nullptr;
            focus_node_->on_key           = nullptr;

            if (auto* fm = FocusManager::activeManager())
            {
                fm->unregisterNode(focus_node_.get());
            }

            if (ctrl_ && ctrl_listener_id_ != 0)
                ctrl_->removeListener(ctrl_listener_id_);
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
                // Unregister old focus node
                if (auto* tim = TextInputManager::activeManager())
                {
                    if (ctrl_->isComposing())
                    {
                        ctrl_->commitComposing();
                    }
                    tim->unregisterInputTarget();
                }

                // Clear callbacks BEFORE unregistering — same reason as in dispose().
                focus_node_->on_focus_changed = nullptr;
                focus_node_->on_key           = nullptr;

                if (auto* fm = FocusManager::activeManager())
                {
                    fm->unregisterNode(focus_node_.get());
                }

                focus_node_ = new_fn;
                focus_node_->on_focus_changed = [this](bool has_focus) {
                    setState([this, has_focus]() { focused_ = has_focus; });
                    
                    // Register/unregister with TextInputManager for IME composition
                    if (auto* tim = TextInputManager::activeManager())
                    {
                        if (has_focus)
                        {
                            tim->registerInputTarget(makeInputTarget());
                        }
                        else
                        {
                            if (ctrl_->isComposing())
                            {
                                ctrl_->commitComposing();
                            }
                            tim->unregisterInputTarget();
                        }
                    }
                };
                focus_node_->on_key = [this](const KeyEvent& event) -> bool {
                    return handleKey(event);
                };
                
                // If new focus node already has focus, register it
                if (focus_node_->hasFocus())
                {
                    if (auto* tim = TextInputManager::activeManager())
                    {
                        tim->registerInputTarget(makeInputTarget());
                    }
                }
            }
        }

        WidgetRef build(BuildContext& ctx) override
        {
            const auto& w = widget();
            const auto* tokens = Theme::tokensOf(ctx);

            auto proxy = std::make_shared<TextFieldProxy>();
            proxy->controller           = ctrl_;
            proxy->style                = w.style;
            proxy->cursor_color         = w.cursor_color.value_or(tokens->colors.primary);
            proxy->selection_color      = w.selection_color.value_or(
                Color::fromRGBA(tokens->colors.primary.r, tokens->colors.primary.g, tokens->colors.primary.b, tokens->colors.primary.a * 0.3f));
            proxy->fill_color           = w.fill_color.value_or(tokens->colors.surface);
            proxy->border_color         = w.border_color.value_or(tokens->colors.outline);
            proxy->focused_border_color = w.focused_border_color.value_or(tokens->colors.primary);
            proxy->placeholder_color    = w.placeholder_color.value_or(tokens->colors.on_surface_variant);
            proxy->placeholder          = w.placeholder;
            proxy->border_radius        = w.border_radius;
            proxy->border_width         = w.border_width;
            proxy->min_height           = w.min_height;
            proxy->focused              = focused_;
            proxy->obscure_text         = w.obscure_text;
            proxy->max_lines            = w.max_lines;
            proxy->min_lines            = w.min_lines;
            proxy->expands              = w.expands;
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

            const bool multiline = widget().isMultiline();

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
                if (multiline && !ctrl)
                {
                    // Multi-line: Insert newline
                    ctrl_->insertText("\n");
                    notifyChanged();
                }
                else if (widget().on_submitted)
                {
                    // Single-line, or multi-line with Ctrl+Enter: Submit
                    widget().on_submitted(ctrl_->text());
                }
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

            case KeyCode::up:
                if (multiline)
                {
                    // TODO: Move cursor up one line (requires RenderTextField cooperation)
                    // For now, move to previous paragraph
                    int pos = ctrl_->selectionEnd();
                    const auto& t = ctrl_->text();
                    // Find start of current line
                    int lineStart = pos;
                    while (lineStart > 0 && t[static_cast<size_t>(lineStart - 1)] != '\n') --lineStart;
                    if (lineStart > 0)
                    {
                        // Move to previous line
                        int prevLineStart = lineStart - 1;
                        while (prevLineStart > 0 && t[static_cast<size_t>(prevLineStart - 1)] != '\n') --prevLineStart;
                        int col = pos - lineStart;
                        int prevLineEnd = lineStart - 1;
                        int newPos = std::min(prevLineStart + col, prevLineEnd);
                        ctrl_->setSelection(shift ? ctrl_->selectionStart() : newPos, newPos);
                    }
                    return true;
                }
                break;

            case KeyCode::down:
                if (multiline)
                {
                    // TODO: Move cursor down one line (requires RenderTextField cooperation)
                    // For now, move to next paragraph
                    int pos = ctrl_->selectionEnd();
                    const auto& t = ctrl_->text();
                    // Find start of current line
                    int lineStart = pos;
                    while (lineStart > 0 && t[static_cast<size_t>(lineStart - 1)] != '\n') --lineStart;
                    int col = pos - lineStart;
                    // Find end of current line
                    int lineEnd = pos;
                    int sz = static_cast<int>(t.size());
                    while (lineEnd < sz && t[static_cast<size_t>(lineEnd)] != '\n') ++lineEnd;
                    if (lineEnd < sz)
                    {
                        // Move to next line
                        int nextLineStart = lineEnd + 1;
                        int nextLineEnd = nextLineStart;
                        while (nextLineEnd < sz && t[static_cast<size_t>(nextLineEnd)] != '\n') ++nextLineEnd;
                        int newPos = std::min(nextLineStart + col, nextLineEnd);
                        ctrl_->setSelection(shift ? ctrl_->selectionStart() : newPos, newPos);
                    }
                    return true;
                }
                break;

            case KeyCode::home:
                if (multiline)
                {
                    // Move to start of current line
                    int pos = ctrl_->selectionEnd();
                    const auto& t = ctrl_->text();
                    int lineStart = pos;
                    while (lineStart > 0 && t[static_cast<size_t>(lineStart - 1)] != '\n') --lineStart;
                    ctrl_->setSelection(shift ? ctrl_->selectionStart() : lineStart, lineStart);
                }
                else
                {
                    ctrl_->setSelection(shift ? ctrl_->selectionStart() : 0, 0);
                }
                return true;

            case KeyCode::end:
            {
                if (multiline)
                {
                    // Move to end of current line
                    int pos = ctrl_->selectionEnd();
                    const auto& t = ctrl_->text();
                    int sz = static_cast<int>(t.size());
                    int lineEnd = pos;
                    while (lineEnd < sz && t[static_cast<size_t>(lineEnd)] != '\n') ++lineEnd;
                    ctrl_->setSelection(shift ? ctrl_->selectionStart() : lineEnd, lineEnd);
                }
                else
                {
                    int end = static_cast<int>(ctrl_->text().size());
                    ctrl_->setSelection(shift ? ctrl_->selectionStart() : end, end);
                }
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

        /**
         * @brief Builds an InputTarget with geometry callbacks for the IME.
         *
         * Traverses from this State's element down to the RenderTextField to
         * provide character-rect and point-to-character queries.
         */
        TextInputManager::InputTarget makeInputTarget()
        {
            TextInputManager::InputTarget target;
            target.controller = ctrl_.get();

            target.get_character_rect = [this](int byte_offset) -> std::array<float, 4> {
                auto* elem = this->element();
                if (!elem) return {0, 0, 0, 0};
                auto* render_elem = elem->findDescendantRenderObjectElement();
                if (!render_elem) return {0, 0, 0, 0};
                auto* render_field = dynamic_cast<RenderTextField*>(render_elem->renderObject());
                if (!render_field) return {0, 0, 0, 0};

                auto rect = render_field->getRectForCharacterRange(byte_offset, byte_offset);
                Offset global = render_field->globalOffset();
                return {
                    rect.x + global.x,
                    rect.y + global.y,
                    std::max(rect.width, 1.0f),
                    rect.height
                };
            };

            target.get_position_for_point = [this](float x, float y) -> int {
                auto* elem = this->element();
                if (!elem) return 0;
                auto* render_elem = elem->findDescendantRenderObjectElement();
                if (!render_elem) return 0;
                auto* render_field = dynamic_cast<RenderTextField*>(render_elem->renderObject());
                if (!render_field) return 0;

                Offset global = render_field->globalOffset();
                float local_x = x - global.x - render_field->padding_h;
                float local_y = y - global.y - render_field->padding_v;
                return render_field->getPositionForPoint(local_x, local_y);
            };

            return target;
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
