#include <campello_widgets/widgets/platform_menu_bar_view.hpp>
#include <campello_widgets/widgets/platform_menu_bar.hpp>
#include <campello_widgets/widgets/platform_menu_delegate.hpp>
#include <campello_widgets/widgets/platform_menu.hpp>
#include <campello_widgets/widgets/theme.hpp>
#include <campello_widgets/widgets/element.hpp>
#include <campello_widgets/widgets/overlay.hpp>
#include <campello_widgets/widgets/modal_barrier.hpp>
#include <campello_widgets/widgets/gesture_detector.hpp>
#include <campello_widgets/widgets/decorated_box.hpp>
#include <campello_widgets/widgets/padding.hpp>
#include <campello_widgets/widgets/column.hpp>
#include <campello_widgets/widgets/row.hpp>
#include <campello_widgets/widgets/expanded.hpp>
#include <campello_widgets/widgets/center.hpp>
#include <campello_widgets/widgets/mouse_region.hpp>
#include <campello_widgets/widgets/divider.hpp>
#include <campello_widgets/widgets/positioned.hpp>
#include <campello_widgets/widgets/sized_box.hpp>
#include <campello_widgets/widgets/text.hpp>
#include <campello_widgets/ui/post_frame_callbacks.hpp>
#include <campello_widgets/widgets/opacity.hpp>
#include <campello_widgets/widgets/stack.hpp>
#include <campello_widgets/widgets/single_child_render_object_widget.hpp>
#include <campello_widgets/ui/box_decoration.hpp>
#include <campello_widgets/ui/box_shadow.hpp>
#include <campello_widgets/ui/edge_insets.hpp>
#include <campello_widgets/ui/text_style.hpp>
#include <campello_widgets/ui/stack_fit.hpp>
#include <campello_widgets/ui/axis.hpp>
#include <campello_widgets/ui/render_gesture_detector.hpp>
#include <campello_widgets/ui/render_dropdown_menu_positioner.hpp>
#include <campello_widgets/ui/render_object.hpp>
#include <campello_widgets/ui/draw_backend.hpp>
#include <campello_widgets/ui/text_span.hpp>
#include <campello_widgets/ui/focus_manager.hpp>
#include <campello_widgets/ui/key.hpp>

#include <algorithm>
#include <cmath>
#include <cctype>
#include <set>
#include <unordered_map>

namespace systems::leal::campello_widgets
{

    // -------------------------------------------------------------------------
    // Shortcut parsing
    // -------------------------------------------------------------------------

    namespace
    {
        std::string toLower(std::string s)
        {
            for (auto& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
            return s;
        }
    }

    namespace detail
    {
        bool parseMenuShortcut(const std::string& shortcut, KeyCode& out_code, uint32_t& out_modifiers)
        {
            out_code      = KeyCode::unknown;
            out_modifiers = KeyModifiers::none;
            if (shortcut.empty()) return false;

            std::vector<std::string> parts;
            size_t start = 0;
            while (true) {
                size_t plus = shortcut.find('+', start);
                if (plus == std::string::npos) {
                    parts.push_back(shortcut.substr(start));
                    break;
                }
                parts.push_back(shortcut.substr(start, plus - start));
                start = plus + 1;
            }
            if (parts.empty() || parts.back().empty()) return false;

            for (size_t i = 0; i + 1 < parts.size(); ++i) {
                const std::string mod = toLower(parts[i]);
                // Cmd has no Linux equivalent — treated as Ctrl, since menu
                // shortcuts are typically authored once and shared across
                // platforms (macOS never reaches this parser; it resolves
                // "Cmd" natively via NSMenuItem's keyEquivalent instead).
                if (mod == "cmd" || mod == "ctrl") out_modifiers |= KeyModifiers::ctrl;
                else if (mod == "shift")           out_modifiers |= KeyModifiers::shift;
                else if (mod == "alt")             out_modifiers |= KeyModifiers::alt;
                else if (mod == "meta" || mod == "super" || mod == "win") out_modifiers |= KeyModifiers::meta;
                else return false;
            }

            const std::string key = toLower(parts.back());
            if (key.size() == 1) {
                const char c = key[0];
                if (c >= 'a' && c <= 'z') {
                    out_code = static_cast<KeyCode>(static_cast<uint32_t>(KeyCode::a) + static_cast<uint32_t>(c - 'a'));
                    return true;
                }
                if (c >= '0' && c <= '9') {
                    out_code = static_cast<KeyCode>(static_cast<uint32_t>(KeyCode::digit_0) + static_cast<uint32_t>(c - '0'));
                    return true;
                }
                return false;
            }

            static const std::unordered_map<std::string, KeyCode> special = {
                {"space", KeyCode::space}, {"enter", KeyCode::enter}, {"return", KeyCode::enter},
                {"tab", KeyCode::tab}, {"backspace", KeyCode::backspace}, {"delete", KeyCode::backspace},
                {"escape", KeyCode::escape}, {"esc", KeyCode::escape},
                {"forwarddelete", KeyCode::delete_forward},
                {"left", KeyCode::left}, {"right", KeyCode::right}, {"up", KeyCode::up}, {"down", KeyCode::down},
                {"home", KeyCode::home}, {"end", KeyCode::end},
                {"pageup", KeyCode::page_up}, {"pagedown", KeyCode::page_down},
                {"f1", KeyCode::f1}, {"f2", KeyCode::f2}, {"f3", KeyCode::f3}, {"f4", KeyCode::f4},
                {"f5", KeyCode::f5}, {"f6", KeyCode::f6}, {"f7", KeyCode::f7}, {"f8", KeyCode::f8},
                {"f9", KeyCode::f9}, {"f10", KeyCode::f10}, {"f11", KeyCode::f11}, {"f12", KeyCode::f12},
            };
            auto it = special.find(key);
            if (it == special.end()) return false;
            out_code = it->second;
            return true;
        }
    } // namespace detail

    namespace
    {
        // Positions the open dropdown panel relative to its anchoring
        // top-level menu label, flipping upward if it doesn't fit below.
        // Internal to PlatformMenuBarView — mirrors DropdownButton's own
        // (also internal) positioner widget.
        class MenuDropdownPositionerWidget : public SingleChildRenderObjectWidget
        {
        public:
            Offset anchor_pos;
            Size   anchor_size;
            bool   beside       = false;  // a submenu, beside its row
            float  beside_inset = 0.0f;

            std::shared_ptr<RenderObject> createRenderObject() const override
            {
                auto r = std::make_shared<RenderDropdownMenuPositioner>();
                r->anchor_pos   = anchor_pos;
                r->anchor_size  = anchor_size;
                r->beside       = beside;
                r->beside_inset = beside_inset;
                return r;
            }

            void updateRenderObject(RenderObject& ro) const override
            {
                auto& r = static_cast<RenderDropdownMenuPositioner&>(ro);
                r.anchor_pos   = anchor_pos;
                r.anchor_size  = anchor_size;
                r.beside       = beside;
                r.beside_inset = beside_inset;
                r.markNeedsLayout();
            }
        };

        std::string providedItemLabel(PlatformProvidedMenuItemType type)
        {
            switch (type) {
                case PlatformProvidedMenuItemType::about:               return "About";
                case PlatformProvidedMenuItemType::preferences:         return "Preferences...";
                case PlatformProvidedMenuItemType::hide:                return "Hide";
                case PlatformProvidedMenuItemType::hide_others:         return "Hide Others";
                case PlatformProvidedMenuItemType::show_all:            return "Show All";
                case PlatformProvidedMenuItemType::quit:                return "Quit";
                case PlatformProvidedMenuItemType::new_file:            return "New";
                case PlatformProvidedMenuItemType::open:                return "Open...";
                case PlatformProvidedMenuItemType::close:               return "Close";
                case PlatformProvidedMenuItemType::save:                return "Save";
                case PlatformProvidedMenuItemType::save_as:             return "Save As...";
                case PlatformProvidedMenuItemType::print:               return "Print...";
                case PlatformProvidedMenuItemType::undo:                return "Undo";
                case PlatformProvidedMenuItemType::redo:                return "Redo";
                case PlatformProvidedMenuItemType::cut:                 return "Cut";
                case PlatformProvidedMenuItemType::copy:                return "Copy";
                case PlatformProvidedMenuItemType::paste:               return "Paste";
                case PlatformProvidedMenuItemType::paste_and_match_style: return "Paste and Match Style";
                case PlatformProvidedMenuItemType::delete_item:         return "Delete";
                case PlatformProvidedMenuItemType::select_all:          return "Select All";
                case PlatformProvidedMenuItemType::toggle_fullscreen:   return "Toggle Full Screen";
                case PlatformProvidedMenuItemType::minimize:            return "Minimize";
                case PlatformProvidedMenuItemType::zoom:                return "Zoom";
                case PlatformProvidedMenuItemType::bring_all_to_front:  return "Bring All to Front";
            }
            return "";
        }

        // Fixed row height for the top-level bar — deliberately not derived
        // from the label Text's own natural height, so the label is
        // vertically centered inside a predictable, native-menu-bar-sized
        // strip regardless of font metrics (ascent/descent vary per glyph
        // set — e.g. a label with no descenders would otherwise sit
        // slightly high in a height that hugs the text exactly).
        constexpr float kMenuBarHeight = 28.0f;
        constexpr float kTopItemHPad   = 10.0f;  // each side of a title

        // Menu panels, as compact as native menus: row padding, the panel's
        // top and bottom padding, and the gap between the widest label and
        // the trailing column (shortcuts, submenu arrows).
        constexpr float kRowHPad     = 12.0f;
        constexpr float kRowVPad     = 3.0f;
        constexpr float kPanelVPad   = 4.0f;
        constexpr float kMenuItemGap = 20.0f;
    } // namespace

    // -------------------------------------------------------------------------
    // PlatformMenuBarViewState
    // -------------------------------------------------------------------------

    class PlatformMenuBarViewState : public State<PlatformMenuBarView>
    {
    public:
        void initState() override
        {
            renders_in_window_ = PlatformMenuDelegate::instance()->needsInWindowMenuBar();
            if (renders_in_window_) {
                // Chain to whatever handler (if any) was already installed
                // rather than clobbering it — an app may legitimately have
                // its own global shortcuts (e.g. dev-only toggles) registered
                // before this widget mounts.
                previous_key_handler_ = FocusManager::globalKeyHandler();
                FocusManager::setGlobalKeyHandler([this](const KeyEvent& event) {
                    if (handleShortcut(event)) return true;
                    return previous_key_handler_ ? previous_key_handler_(event) : false;
                });
            }
        }

        void dispose() override
        {
            removeOverlayEntry();
            if (renders_in_window_) {
                FocusManager::setGlobalKeyHandler(previous_key_handler_);
            }
        }

        WidgetRef build(BuildContext& ctx) override
        {
            if (!renders_in_window_) return SizedBox::shrink();

            const auto* menus = PlatformMenuBar::menusOf(ctx);
            current_menus_ = menus ? *menus : std::vector<PlatformMenuRef>{};
            if (current_menus_.empty()) return SizedBox::shrink();

            const auto* tokens = Theme::tokensOf(ctx);
            bg_color_            = tokens->colors.surface;
            on_surface_          = tokens->colors.on_surface;
            on_surface_variant_  = tokens->colors.on_surface_variant;
            highlight_color_     = tokens->colors.surface_variant;
            separator_color_     = tokens->colors.outline_variant;

            while (anchor_keys_.size() < current_menus_.size())
                anchor_keys_.push_back(std::make_shared<GlobalKey>());

            if (open_top_index_ >= static_cast<int>(current_menus_.size())) {
                open_top_index_ = -1;
                open_path_.clear();
                hovered_ = nullptr;
                removeOverlayEntry();
            }

            // A Row of titles, each as wide as its own text (as native menu
            // bars): each item is a tight SizedBox, so its Center fills only
            // that item.
            std::vector<WidgetRef> titles;
            for (int i = 0; i < static_cast<int>(current_menus_.size()); ++i)
                titles.push_back(buildTopLabel(i));
            auto row = std::make_shared<Row>(std::move(titles));

            auto sized_row = SizedBox::from_height(kMenuBarHeight, row);

            BoxDecoration bar_deco;
            bar_deco.color = bg_color_;
            auto bar = std::make_shared<DecoratedBox>();
            bar->decoration = bar_deco;
            bar->child       = sized_row;

            auto col = std::make_shared<Column>();
            col->main_axis_size = MainAxisSize::min;
            col->children        = {bar, std::make_shared<Divider>()};
            return col;
        }

    private:
        // ------------------------------------------------------------------
        // Top-level bar
        // ------------------------------------------------------------------

        // A title's width: its text, measured, plus padding on each side.
        float topItemWidth(int index)
        {
            const auto& menu = current_menus_[static_cast<size_t>(index)];
            const std::string label = menu ? menu->label : std::string();
            TextStyle ts;
            ts.color = on_surface_;
            auto* backend = RenderObject::activeBackend();
            // Before the first layout (the first build, on the web) there is
            // no backend to measure with: estimate as RenderText does, and
            // measure again once a frame has run.
            const float w = backend ? backend->measureText(TextSpan{label, ts}).width
                                    : ts.font_size * 0.6f * static_cast<float>(label.size());
            if (!backend && !remeasure_scheduled_) {
                remeasure_scheduled_ = true;
                std::weak_ptr<bool> alive = alive_;
                PostFrameCallbacks::schedule([this, alive]() {
                    if (alive.lock()) setState([]() {});
                });
            }
            return std::ceil(w) + 2.0f * kTopItemHPad;
        }

        WidgetRef buildTopLabel(int index)
        {
            const auto& menu = current_menus_[static_cast<size_t>(index)];

            TextStyle ts;
            ts.color = on_surface_;
            auto text = std::make_shared<Text>(menu ? menu->label : std::string(), ts);

            BoxDecoration deco;
            deco.color = (open_top_index_ == index) ? highlight_color_ : Color::transparent();
            auto deco_box = std::make_shared<DecoratedBox>();
            deco_box->decoration = deco;
            deco_box->child      = std::make_shared<Center>(text);

            auto gesture = std::make_shared<GestureDetector>();
            gesture->key    = anchor_keys_[static_cast<size_t>(index)];
            gesture->on_tap = [this, index]() { toggleTopMenu(index); };
            gesture->child  = deco_box;
            return std::make_shared<SizedBox>(topItemWidth(index), kMenuBarHeight, gesture);
        }

        void toggleTopMenu(int index)
        {
            const bool was_open_here = (open_top_index_ == index);
            setState([this, index, was_open_here]() {
                open_top_index_ = was_open_here ? -1 : index;
                open_path_.clear();
                hovered_ = nullptr;
            });
            refreshOverlay();
        }

        // With a menu open, the pointer moved onto another title: that menu
        // opens instead (as native menu bars).
        void switchTopMenu(int index)
        {
            if (open_top_index_ < 0 || open_top_index_ == index) return;
            setState([this, index]() {
                open_top_index_ = index;
                open_path_.clear();
                hovered_ = nullptr;
            });
            refreshOverlay();
        }

        void closeAll()
        {
            setState([this]() {
                open_top_index_ = -1;
                open_path_.clear();
                hovered_ = nullptr;
            });
            removeOverlayEntry();
        }

        // ------------------------------------------------------------------
        // Dropdown overlay
        // ------------------------------------------------------------------

        // Where a keyed GestureDetector is on screen (false before its first
        // layout; only gesture detectors track their global offset).
        static bool rectOf(const std::shared_ptr<GlobalKey>& key, Offset& pos, Size& size)
        {
            if (!key) return false;
            auto* element = key->currentElement();
            if (!element) return false;
            auto* roe = element->findDescendantRenderObjectElement();
            if (!roe) return false;
            auto box = std::dynamic_pointer_cast<RenderGestureDetector>(roe->sharedRenderObject());
            if (!box) return false;
            pos  = box->globalOffset();
            size = box->size();
            return size.width > 0.0f && size.height > 0.0f;
        }

        // The overlay: a barrier that closes the menus on an outside press;
        // the bar's titles again, above it (moving onto one switches menus,
        // a tap on the open one closes it); the open menu below its title;
        // each open submenu beside its row.
        void refreshOverlay()
        {
            if (open_top_index_ < 0) {
                removeOverlayEntry();
                return;
            }

            std::vector<WidgetRef> layers;
            layers.push_back(ModalBarrier::create(Color::transparent(), true, [this]() { closeAll(); }));

            for (int i = 0; i < static_cast<int>(current_menus_.size()); ++i) {
                Offset pos;
                Size   size = Size::zero();
                if (!rectOf(anchor_keys_[static_cast<size_t>(i)], pos, size)) continue;
                auto tap = std::make_shared<GestureDetector>();
                tap->on_tap = [this, i]() { toggleTopMenu(i); };
                tap->child  = std::make_shared<SizedBox>(size.width, size.height);
                auto hover = std::make_shared<MouseRegion>();
                hover->on_enter = [this, i]() { switchTopMenu(i); };
                hover->child    = tap;
                layers.push_back(std::make_shared<Positioned>(pos.x, pos.y, std::nullopt, std::nullopt,
                                                              size.width, size.height, hover));
            }

            Offset anchor_pos;
            Size   anchor_size = Size::zero();
            rectOf(anchor_keys_[static_cast<size_t>(open_top_index_)], anchor_pos, anchor_size);
            auto top = std::make_shared<MenuDropdownPositionerWidget>();
            top->anchor_pos  = anchor_pos;
            top->anchor_size = anchor_size;
            top->child       = buildDropdownPanel(current_menus_[static_cast<size_t>(open_top_index_)]->items, 0);
            layers.push_back(Positioned::fill(top));

            for (size_t d = 0; d < open_path_.size(); ++d) {
                Offset pos;
                Size   size = Size::zero();
                auto key = submenu_keys_.find(open_path_[d]);
                if (key == submenu_keys_.end() || !rectOf(key->second, pos, size)) {
                    open_path_.resize(d);
                    break;
                }
                auto beside = std::make_shared<MenuDropdownPositionerWidget>();
                beside->anchor_pos   = pos;
                beside->anchor_size  = size;
                beside->beside       = true;
                beside->beside_inset = kPanelVPad;
                beside->child        = buildDropdownPanel(open_path_[d]->items, static_cast<int>(d) + 1);
                layers.push_back(Positioned::fill(beside));
            }

            auto stack = Stack::create(layers);
            stack->fit = StackFit::expand;

            if (menu_entry_) {
                menu_entry_->child = stack;
                if (auto* es = menu_entry_->entryState()) es->markNeedsBuild();
            } else {
                menu_entry_ = std::make_shared<OverlayEntry>(stack);
                Overlay::insert(menu_entry_);
            }
        }

        void removeOverlayEntry()
        {
            if (menu_entry_) {
                Overlay::remove(menu_entry_);
                menu_entry_.reset();
            }
        }

        // The widest label (with its check mark) and widest trailing text
        // (shortcut, or a submenu's arrow) among `items`: one panel's
        // columns. Each row is [label][Expanded spacer][trailing] at the
        // panel's width, so trailing text lines up on the right.
        void measureColumnWidths(const std::vector<PlatformMenuItemRef>& items,
                                 float& label_width, float& trailing_width)
        {
            label_width = 0.0f;
            trailing_width = 0.0f;
            auto* backend = RenderObject::activeBackend();
            TextStyle ts;
            ts.color = on_surface_;
            TextStyle sts;
            sts.color = on_surface_variant_;
            auto measure = [backend](const std::string& text, const TextStyle& style) {
                return backend ? backend->measureText(TextSpan{text, style}).width
                               : style.font_size * 0.6f * static_cast<float>(text.size());
            };
            for (const auto& item : items) {
                if (!item) continue;
                if (auto* label = dynamic_cast<PlatformMenuItemLabel*>(item.get())) {
                    label_width = std::max(label_width, measure(checkPrefix(label) + label->label, ts));
                    if (!label->shortcut.empty())
                        trailing_width = std::max(trailing_width, measure(label->shortcut, sts));
                } else if (auto* submenu = dynamic_cast<PlatformMenuItemSubmenu*>(item.get())) {
                    label_width    = std::max(label_width, measure(submenu->label, ts));
                    trailing_width = std::max(trailing_width, measure("▸", sts));
                } else if (auto* provided = dynamic_cast<PlatformProvidedMenuItem*>(item.get())) {
                    label_width = std::max(label_width, measure(providedItemLabel(provided->type), ts));
                }
            }
            label_width = std::ceil(label_width);
            trailing_width = std::ceil(trailing_width);
        }

        static std::string checkPrefix(const PlatformMenuItemLabel* item)
        {
            return item->checked ? "✓ " : "";
        }

        WidgetRef buildDropdownPanel(const std::vector<PlatformMenuItemRef>& items, int depth)
        {
            std::vector<WidgetRef> rows;
            for (const auto& item : items) {
                if (!item) continue;
                if (dynamic_cast<PlatformMenuItemSeparator*>(item.get())) {
                    rows.push_back(buildSeparator(depth));
                } else if (auto* label = dynamic_cast<PlatformMenuItemLabel*>(item.get())) {
                    rows.push_back(buildLabelRow(label, depth));
                } else if (auto* submenu = dynamic_cast<PlatformMenuItemSubmenu*>(item.get())) {
                    rows.push_back(buildSubmenuRow(submenu, depth));
                } else if (auto* provided = dynamic_cast<PlatformProvidedMenuItem*>(item.get())) {
                    rows.push_back(buildProvidedRow(provided, depth));
                }
            }

            // stretch: every row is as wide as the panel (a definite width
            // from the measured columns, not what the positioner offers —
            // the whole overlay), so trailing texts share the right edge.
            auto col = std::make_shared<Column>();
            col->main_axis_size       = MainAxisSize::min;
            col->cross_axis_alignment = CrossAxisAlignment::stretch;
            col->children             = std::move(rows);

            float label_width = 0.0f, trailing_width = 0.0f;
            measureColumnWidths(items, label_width, trailing_width);
            const float gap = trailing_width > 0.0f ? kMenuItemGap : 0.0f;
            const float panel_width = kRowHPad + label_width + gap + trailing_width + kRowHPad;

            auto padded = std::make_shared<Padding>();
            padded->padding = EdgeInsets::symmetric(kPanelVPad, 0.0f);
            padded->child   = SizedBox::from_width(panel_width, col);

            BoxDecoration deco;
            deco.color         = bg_color_;
            deco.border_radius = 6.0f;
            deco.box_shadow    = {BoxShadow{Color::fromRGBA(0.0f, 0.0f, 0.0f, 0.2f), Offset{0.0f, 4.0f}, 12.0f}};

            auto box = std::make_shared<DecoratedBox>();
            box->decoration = deco;
            box->child       = padded;
            return box;
        }

        // The pointer is on a row of the panel at `depth`: it's highlighted,
        // and submenus open from deeper panels close (as native menus).
        void hoverRow(const void* item, int depth)
        {
            const bool deeper_open = open_path_.size() > static_cast<size_t>(depth);
            if (hovered_ == item && !deeper_open) return;
            hovered_ = item;
            if (deeper_open) open_path_.resize(static_cast<size_t>(depth));
            refreshOverlay();
        }

        void leaveRow(const void* item)
        {
            if (hovered_ != item) return;
            hovered_ = nullptr;
            refreshOverlay();
        }

        // Opens `submenu` from the panel at `depth` (hovering or tapping its
        // row); a tap on an open one closes it.
        void openSubmenu(PlatformMenuItemSubmenu* submenu, int depth, bool toggle)
        {
            const size_t d = static_cast<size_t>(depth);
            const bool open = open_path_.size() > d && open_path_[d] == submenu;
            if (open && !toggle && open_path_.size() == d + 1 && hovered_ == submenu) return;
            hovered_ = submenu;
            open_path_.resize(d);
            if (!open || !toggle) open_path_.push_back(submenu);
            refreshOverlay();
        }

        // A row's box: padding, the highlight, hover and tap handling.
        WidgetRef rowBox(const void* item, int depth, WidgetRef content, bool enabled, bool highlighted,
                         std::function<void()> on_tap, std::function<void()> on_enter = nullptr,
                         std::shared_ptr<GlobalKey> key = nullptr)
        {
            auto padded = std::make_shared<Padding>();
            padded->padding = EdgeInsets::symmetric(kRowVPad, kRowHPad);
            padded->child   = std::move(content);

            WidgetRef body = padded;
            if (!enabled) {
                auto faded     = std::make_shared<Opacity>();
                faded->opacity = 0.4f;
                faded->child   = body;
                body = faded;
            }

            BoxDecoration deco;
            deco.color = highlighted && enabled ? highlight_color_ : Color::transparent();
            auto box = std::make_shared<DecoratedBox>();
            box->decoration = deco;
            box->child      = body;

            auto tap = std::make_shared<GestureDetector>();
            tap->key   = std::move(key);
            if (enabled) tap->on_tap = std::move(on_tap);
            tap->child = box;

            auto hover = std::make_shared<MouseRegion>();
            hover->on_enter = on_enter ? std::move(on_enter) : std::function<void()>([this, item, depth]() { hoverRow(item, depth); });
            hover->on_exit  = [this, item]() { leaveRow(item); };
            hover->child    = tap;
            return hover;
        }

        WidgetRef buildSeparator(int depth)
        {
            // Hovering the gap still closes deeper submenus.
            auto hover = std::make_shared<MouseRegion>();
            hover->on_enter = [this, depth]() { hoverRow(nullptr, depth); };
            // A hairline with a little space around it, as native menus (a
            // Divider takes the design system's taller list spacing).
            BoxDecoration line;
            line.color = separator_color_;
            auto rule = std::make_shared<DecoratedBox>();
            rule->decoration = line;
            rule->child      = SizedBox::from_height(1.0f);
            hover->child    = std::make_shared<Padding>(EdgeInsets::symmetric(kPanelVPad, 0.0f), rule);
            return hover;
        }

        WidgetRef columns(WidgetRef label, WidgetRef trailing)
        {
            auto row = std::make_shared<Row>();
            row->children = {
                std::move(label),
                std::make_shared<Expanded>(std::make_shared<SizedBox>()),
                std::move(trailing),
            };
            return row;
        }

        WidgetRef buildLabelRow(PlatformMenuItemLabel* item, int depth)
        {
            TextStyle ts;
            ts.color = on_surface_;
            TextStyle sts;
            sts.color = on_surface_variant_;
            auto content = columns(std::make_shared<Text>(checkPrefix(item) + item->label, ts),
                                   std::make_shared<Text>(item->shortcut, sts));
            auto callback = item->on_selected;
            return rowBox(item, depth, content, item->enabled, hovered_ == item, [this, callback]() {
                closeAll();
                if (callback) callback();
            });
        }

        WidgetRef buildSubmenuRow(PlatformMenuItemSubmenu* item, int depth)
        {
            auto& key = submenu_keys_[item];
            if (!key) key = std::make_shared<GlobalKey>();
            const bool open = std::find(open_path_.begin(), open_path_.end(), item) != open_path_.end();

            TextStyle ts;
            ts.color = on_surface_;
            TextStyle arrow_ts;
            arrow_ts.color = on_surface_variant_;
            auto content = columns(std::make_shared<Text>(item->label, ts), std::make_shared<Text>("▸", arrow_ts));
            if (!item->enabled)
                return rowBox(item, depth, content, false, false, nullptr, nullptr, key);
            return rowBox(item, depth, content, true, open || hovered_ == item,
                          [this, item, depth]() { openSubmenu(item, depth, true); },
                          [this, item, depth]() { openSubmenu(item, depth, false); },
                          key);
        }

        WidgetRef buildProvidedRow(PlatformProvidedMenuItem* item, int depth)
        {
            // No generic OS responder chain exists outside macOS's AppKit
            // target/action system, so these can't be wired to real actions
            // here (unlike NSMenuItem's automatic cut:/copy:/quit: etc.).
            // Shown dimmed and non-interactive; apps that need these on
            // Linux should use PlatformMenuItemLabel with an explicit
            // on_selected callback instead.
            TextStyle ts;
            ts.color = on_surface_variant_;
            return rowBox(item, depth, std::make_shared<Text>(providedItemLabel(item->type), ts), false, false, nullptr);
        }

        // ------------------------------------------------------------------
        // Keyboard accelerators
        // ------------------------------------------------------------------

        bool handleShortcut(const KeyEvent& event)
        {
            if (event.kind != KeyEventKind::down) return false;
            return tryInvokeShortcut(current_menus_, event);
        }

        bool tryInvokeShortcut(const std::vector<PlatformMenuRef>& menus, const KeyEvent& event)
        {
            for (const auto& menu : menus) {
                if (menu && tryInvokeShortcut(menu->items, event)) return true;
            }
            return false;
        }

        bool tryInvokeShortcut(const std::vector<PlatformMenuItemRef>& items, const KeyEvent& event)
        {
            for (const auto& item : items) {
                if (!item) continue;
                if (auto* label = dynamic_cast<PlatformMenuItemLabel*>(item.get())) {
                    if (!label->enabled || label->shortcut.empty()) continue;
                    KeyCode  code;
                    uint32_t mods;
                    if (detail::parseMenuShortcut(label->shortcut, code, mods) &&
                        code == event.key_code && mods == event.modifiers) {
                        if (label->on_selected) label->on_selected();
                        return true;
                    }
                } else if (auto* submenu = dynamic_cast<PlatformMenuItemSubmenu*>(item.get())) {
                    if (submenu->enabled && tryInvokeShortcut(submenu->items, event)) return true;
                }
            }
            return false;
        }

        bool renders_in_window_ = false;
        bool remeasure_scheduled_ = false;
        std::shared_ptr<bool> alive_ = std::make_shared<bool>(true);  // gone with the state
        int  open_top_index_    = -1;
        std::function<bool(const KeyEvent&)> previous_key_handler_;

        std::vector<PlatformMenuRef>       current_menus_;
        std::vector<std::shared_ptr<GlobalKey>> anchor_keys_;
        std::vector<PlatformMenuItemSubmenu*> open_path_;  // open submenus, outermost first
        std::unordered_map<const PlatformMenuItemSubmenu*, std::shared_ptr<GlobalKey>> submenu_keys_;
        const void*                        hovered_ = nullptr;  // the row under the pointer
        std::shared_ptr<OverlayEntry>      menu_entry_;

        Color bg_color_           = Color::white();
        Color on_surface_         = Color::black();
        Color on_surface_variant_ = Color::black();
        Color highlight_color_    = Color::transparent();
        Color separator_color_    = Color::fromRGBA(0.0f, 0.0f, 0.0f, 0.12f);
    };

    // -------------------------------------------------------------------------

    std::unique_ptr<StateBase> PlatformMenuBarView::createState() const
    {
        return std::make_unique<PlatformMenuBarViewState>();
    }

    std::shared_ptr<PlatformMenuBarView> PlatformMenuBarView::create()
    {
        return std::make_shared<PlatformMenuBarView>();
    }

} // namespace systems::leal::campello_widgets
