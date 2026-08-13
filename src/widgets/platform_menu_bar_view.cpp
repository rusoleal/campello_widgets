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
#include <campello_widgets/widgets/list_view.hpp>
#include <campello_widgets/widgets/divider.hpp>
#include <campello_widgets/widgets/positioned.hpp>
#include <campello_widgets/widgets/sized_box.hpp>
#include <campello_widgets/widgets/text.hpp>
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

            std::shared_ptr<RenderObject> createRenderObject() const override
            {
                auto r = std::make_shared<RenderDropdownMenuPositioner>();
                r->anchor_pos  = anchor_pos;
                r->anchor_size = anchor_size;
                return r;
            }

            void updateRenderObject(RenderObject& ro) const override
            {
                auto& r = static_cast<RenderDropdownMenuPositioner&>(ro);
                r.anchor_pos  = anchor_pos;
                r.anchor_size = anchor_size;
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
        constexpr float kMenuBarHeight = 32.0f;

        // Gap reserved after the widest label in the open menu, before any
        // trailing shortcut/submenu-arrow content — see
        // PlatformMenuBarViewState::measured_label_width_.
        constexpr float kMenuItemGap = 24.0f;
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

            while (anchor_keys_.size() < current_menus_.size())
                anchor_keys_.push_back(std::make_shared<GlobalKey>());

            if (open_top_index_ >= static_cast<int>(current_menus_.size())) {
                open_top_index_ = -1;
                expanded_submenus_.clear();
                removeOverlayEntry();
            }

            // A horizontal, fixed-extent ListView rather than a Row: each
            // item gets a genuinely tight (item_extent × bar height) box to
            // lay out into, so Center below fills only *that* box instead
            // of the whole bar (the bug with a Row, where a loose
            // cross-axis-center child fills all the way to the row's own
            // resolved width). It also scrolls instead of overflowing if
            // there isn't enough space for every menu.
            auto list = std::make_shared<ListView>();
            list->scroll_axis = Axis::horizontal;
            list->item_count  = static_cast<int>(current_menus_.size());
            list->item_extent = measureTopBarItemExtent();
            list->builder     = [this](BuildContext&, int i) { return buildTopLabel(i); };

            auto sized_list = SizedBox::from_height(kMenuBarHeight, list);

            BoxDecoration bar_deco;
            bar_deco.color = bg_color_;
            auto bar = std::make_shared<DecoratedBox>();
            bar->decoration = bar_deco;
            bar->child       = sized_list;

            auto col = std::make_shared<Column>();
            col->main_axis_size = MainAxisSize::min;
            col->children        = {bar, std::make_shared<Divider>()};
            return col;
        }

    private:
        // ------------------------------------------------------------------
        // Top-level bar
        // ------------------------------------------------------------------

        // Reserves enough per-item width for the longest menu name, plus
        // horizontal padding — ListView requires one uniform item_extent
        // (see build()), so this is computed from real text metrics rather
        // than guessed, the same way measureColumnWidths() sizes the
        // dropdown panel below.
        float measureTopBarItemExtent()
        {
            float w = 40.0f;
            auto* backend = RenderObject::activeBackend();
            if (!backend) return w;

            w = 0.0f;
            TextStyle ts;
            ts.color = on_surface_;
            for (const auto& menu : current_menus_) {
                if (!menu) continue;
                w = std::max(w, backend->measureText(TextSpan{menu->label, ts}).width);
            }
            return w + 24.0f; // 12px padding each side, matching buildTopLabel
        }

        WidgetRef buildTopLabel(int index)
        {
            const auto& menu = current_menus_[static_cast<size_t>(index)];

            TextStyle ts;
            ts.color = on_surface_;
            auto text = std::make_shared<Text>(menu->label, ts);

            auto h_padded = std::make_shared<Padding>();
            h_padded->padding = EdgeInsets::symmetric(0.0f, 12.0f);
            h_padded->child   = text;

            // Safe here (unlike wrapping the whole bar row in Center):
            // ListView hands each item a genuinely tight item_extent-wide
            // box, so "fill to max available" fills only this one item's
            // slot, not the bar's full resolved width.
            auto centered = std::make_shared<Center>(h_padded);

            BoxDecoration deco;
            deco.color = (open_top_index_ == index) ? highlight_color_ : Color::transparent();
            auto deco_box = std::make_shared<DecoratedBox>();
            deco_box->decoration = deco;
            deco_box->child      = centered;

            auto gesture = std::make_shared<GestureDetector>();
            gesture->key    = anchor_keys_[static_cast<size_t>(index)];
            gesture->on_tap = [this, index]() { toggleTopMenu(index); };
            gesture->child  = deco_box;
            return gesture;
        }

        void toggleTopMenu(int index)
        {
            const bool was_open_here = (open_top_index_ == index);
            setState([this, index, was_open_here]() {
                open_top_index_ = was_open_here ? -1 : index;
                expanded_submenus_.clear();
            });
            refreshOverlay();
        }

        void closeAll()
        {
            setState([this]() {
                open_top_index_ = -1;
                expanded_submenus_.clear();
            });
            removeOverlayEntry();
        }

        // ------------------------------------------------------------------
        // Dropdown overlay
        // ------------------------------------------------------------------

        void refreshOverlay()
        {
            if (open_top_index_ < 0) {
                removeOverlayEntry();
                return;
            }

            Offset anchor_pos;
            Size   anchor_size = Size::zero();
            if (auto& key = anchor_keys_[static_cast<size_t>(open_top_index_)]) {
                if (auto* element = key->currentElement()) {
                    if (auto* roe = element->findDescendantRenderObjectElement()) {
                        auto box = std::dynamic_pointer_cast<RenderGestureDetector>(roe->sharedRenderObject());
                        if (box) {
                            anchor_pos  = box->globalOffset();
                            anchor_size = box->size();
                        }
                    }
                }
            }

            // Floors rather than 0: if there's no active draw backend yet
            // (shouldn't happen once the app is actually on screen, but
            // cheap to guard) a zero-width panel would render nothing
            // instead of just skipping alignment.
            measured_label_width_    = 40.0f;
            measured_shortcut_width_ = 0.0f;
            if (auto* backend = RenderObject::activeBackend()) {
                measured_label_width_ = 0.0f;
                measureColumnWidths(backend, current_menus_[static_cast<size_t>(open_top_index_)]->items);
            }

            auto panel = buildDropdownPanel(current_menus_[static_cast<size_t>(open_top_index_)]->items, 0);

            auto positioner = std::make_shared<MenuDropdownPositionerWidget>();
            positioner->anchor_pos  = anchor_pos;
            positioner->anchor_size = anchor_size;
            positioner->child       = panel;

            auto dismiss = [this]() { closeAll(); };
            auto barrier = ModalBarrier::create(Color::transparent(), true, dismiss);

            std::vector<WidgetRef> stack_children = {barrier, Positioned::fill(positioner)};
            auto stack = Stack::create(stack_children);
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

        // Measures every label/submenu-title and every shortcut/arrow in
        // `items` (recursively, so a still-collapsed submenu's content
        // doesn't shift the panel width when later expanded), growing
        // measured_label_width_/measured_shortcut_width_ to fit the widest
        // of each. buildDropdownPanel sizes the whole panel from these two
        // numbers, and each row is a [label][Expanded spacer][trailing] Row
        // that fills that established width — trailing content lands
        // flush against the panel's right edge on every row, using real
        // font metrics via the active draw backend rather than a guess.
        void measureColumnWidths(IDrawBackend* backend, const std::vector<PlatformMenuItemRef>& items)
        {
            TextStyle ts;
            ts.color = on_surface_;
            TextStyle sts;
            sts.color = on_surface_variant_;
            for (const auto& item : items) {
                if (!item) continue;
                if (auto* label = dynamic_cast<PlatformMenuItemLabel*>(item.get())) {
                    const std::string prefix = label->checked ? "✓ " : "";
                    measured_label_width_ = std::max(measured_label_width_,
                        backend->measureText(TextSpan{prefix + label->label, ts}).width);
                    if (!label->shortcut.empty())
                        measured_shortcut_width_ = std::max(measured_shortcut_width_,
                            backend->measureText(TextSpan{label->shortcut, sts}).width);
                } else if (auto* submenu = dynamic_cast<PlatformMenuItemSubmenu*>(item.get())) {
                    measured_label_width_ = std::max(measured_label_width_,
                        backend->measureText(TextSpan{submenu->label, ts}).width);
                    measured_shortcut_width_ = std::max(measured_shortcut_width_,
                        backend->measureText(TextSpan{"▾", sts}).width);
                    measureColumnWidths(backend, submenu->items);
                }
            }
        }

        WidgetRef buildDropdownPanel(const std::vector<PlatformMenuItemRef>& items, int depth)
        {
            std::vector<WidgetRef> rows;
            for (const auto& item : items) {
                if (!item) continue;

                if (dynamic_cast<PlatformMenuItemSeparator*>(item.get())) {
                    rows.push_back(std::make_shared<Divider>());
                } else if (auto* label = dynamic_cast<PlatformMenuItemLabel*>(item.get())) {
                    rows.push_back(buildLabelRow(label, depth));
                } else if (auto* submenu = dynamic_cast<PlatformMenuItemSubmenu*>(item.get())) {
                    rows.push_back(buildSubmenuRow(submenu, depth));
                    if (expanded_submenus_.count(submenu) > 0)
                        rows.push_back(buildDropdownPanel(submenu->items, depth + 1));
                } else if (auto* provided = dynamic_cast<PlatformProvidedMenuItem*>(item.get())) {
                    rows.push_back(buildProvidedRow(provided, depth));
                }
            }

            // stretch: safe here because every row is explicitly sized to
            // panel_width below (a definite, finite value from real text
            // metrics) rather than to whatever RenderDropdownMenuPositioner
            // offers — the panel would otherwise span the full overlay/
            // viewport width (RenderDropdownMenuPositioner hands its child
            // a max-width constraint equal to that), the same trap
            // DropdownButton's own positioner usage documents. stretch is
            // what makes every row share panel_width, so each row's
            // [label][Expanded][trailing] can right-justify its trailing
            // content against a common edge.
            auto col = std::make_shared<Column>();
            col->main_axis_size       = MainAxisSize::min;
            col->cross_axis_alignment = CrossAxisAlignment::stretch;
            col->children             = std::move(rows);

            if (depth > 0) return col;

            constexpr float kPanelHPad = 12.0f;
            const float panel_width = kPanelHPad + measured_label_width_ + kMenuItemGap
                                     + measured_shortcut_width_ + kPanelHPad;
            auto sized_col = SizedBox::from_width(panel_width, col);

            BoxDecoration deco;
            deco.color         = bg_color_;
            deco.border_radius = 6.0f;
            deco.box_shadow    = {BoxShadow{Color::fromRGBA(0.0f, 0.0f, 0.0f, 0.2f), Offset{0.0f, 4.0f}, 12.0f}};

            auto box = std::make_shared<DecoratedBox>();
            box->decoration = deco;
            box->child       = sized_col;
            return box;
        }

        WidgetRef buildLabelRow(PlatformMenuItemLabel* item, int depth)
        {
            TextStyle ts;
            ts.color = on_surface_;
            const std::string prefix = item->checked ? "✓ " : "";
            auto label_text = std::make_shared<Text>(prefix + item->label, ts);

            TextStyle sts;
            sts.color = on_surface_variant_;
            auto shortcut_text = std::make_shared<Text>(item->shortcut, sts);

            // [label][Expanded spacer][shortcut]: the row is stretched to
            // panel_width by the parent Column (see buildDropdownPanel), so
            // the spacer absorbs whatever's left over and pushes the
            // shortcut flush against the row's — and so every row's —
            // right edge, regardless of how long this row's own label is.
            // (An empty shortcut_text just collapses to zero width.)
            auto row = std::make_shared<Row>();
            row->children = {
                label_text,
                std::make_shared<Expanded>(std::make_shared<SizedBox>()),
                shortcut_text,
            };

            auto padded = std::make_shared<Padding>();
            padded->padding = EdgeInsets::only(12.0f + static_cast<float>(depth) * 16.0f, 8.0f, 12.0f, 8.0f);
            padded->child   = row;

            if (item->enabled) {
                auto callback = item->on_selected;
                auto g    = std::make_shared<GestureDetector>();
                g->on_tap = [this, callback]() {
                    closeAll();
                    if (callback) callback();
                };
                g->child = padded;
                return g;
            }

            auto faded     = std::make_shared<Opacity>();
            faded->opacity = 0.4f;
            faded->child   = padded;
            return faded;
        }

        WidgetRef buildSubmenuRow(PlatformMenuItemSubmenu* item, int depth)
        {
            const bool expanded = expanded_submenus_.count(item) > 0;

            TextStyle ts;
            ts.color = on_surface_;
            auto label_text = std::make_shared<Text>(item->label, ts);

            TextStyle arrow_ts;
            arrow_ts.color = on_surface_variant_;
            auto arrow = std::make_shared<Text>(expanded ? "▾" : "▸", arrow_ts);

            auto row = std::make_shared<Row>();
            row->children = {
                label_text,
                std::make_shared<Expanded>(std::make_shared<SizedBox>()),
                arrow,
            };

            auto padded = std::make_shared<Padding>();
            padded->padding = EdgeInsets::only(12.0f + static_cast<float>(depth) * 16.0f, 8.0f, 12.0f, 8.0f);
            padded->child   = row;

            if (item->enabled) {
                auto g    = std::make_shared<GestureDetector>();
                g->on_tap = [this, item]() {
                    if (expanded_submenus_.count(item) > 0) expanded_submenus_.erase(item);
                    else expanded_submenus_.insert(item);
                    refreshOverlay();
                };
                g->child = padded;
                return g;
            }

            auto faded     = std::make_shared<Opacity>();
            faded->opacity = 0.4f;
            faded->child   = padded;
            return faded;
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
            auto label_text = std::make_shared<Text>(providedItemLabel(item->type), ts);

            auto padded = std::make_shared<Padding>();
            padded->padding = EdgeInsets::only(12.0f + static_cast<float>(depth) * 16.0f, 8.0f, 12.0f, 8.0f);
            padded->child   = label_text;

            auto faded     = std::make_shared<Opacity>();
            faded->opacity = 0.4f;
            faded->child   = padded;
            return faded;
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
        int  open_top_index_    = -1;
        std::function<bool(const KeyEvent&)> previous_key_handler_;

        std::vector<PlatformMenuRef>       current_menus_;
        std::vector<std::shared_ptr<GlobalKey>> anchor_keys_;
        std::set<PlatformMenuItemSubmenu*> expanded_submenus_;
        std::shared_ptr<OverlayEntry>      menu_entry_;
        float                               measured_label_width_    = 0.0f;
        float                               measured_shortcut_width_ = 0.0f;

        Color bg_color_           = Color::white();
        Color on_surface_         = Color::black();
        Color on_surface_variant_ = Color::black();
        Color highlight_color_    = Color::transparent();
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
