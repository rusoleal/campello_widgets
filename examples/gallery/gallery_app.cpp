#include "gallery_app.hpp"
#include "assets/mountains_jpeg.h"
#include <campello_widgets/campello_widgets.hpp>
#include <campello_ui/campello_design_system.hpp>
#include <campello_material/material_design_system.hpp>
#include <campello_cupertino/cupertino_design_system.hpp>

#include <cmath>
#include <functional>
#include <string>
#include <vector>
#include <sstream>
#include <iomanip>

namespace cw = systems::leal::campello_widgets;

// Storage for setSampleVideoPath()/sampleVideoPath() — see
// gallery_app.hpp's doc comment on setSampleVideoPath() for why this
// indirection exists (each platform's main.mm/main.cpp resolves the path
// differently; gallery_app.cpp itself stays portable). Defined at file
// scope, ahead of every class that reads it, since VideoSectionState
// (a good way below) needs it visible at its point of use.
static std::string& sampleVideoPathStorage()
{
    static std::string path;
    return path;
}

static const std::string& sampleVideoPath()
{
    return sampleVideoPathStorage();
}

// ---------------------------------------------------------------------------
// Palette
// ---------------------------------------------------------------------------
static const cw::Color kBlue   = cw::Color::fromRGB(0.08f, 0.47f, 0.95f);
static const cw::Color kGreen  = cw::Color::fromRGB(0.10f, 0.70f, 0.40f);
static const cw::Color kOrange = cw::Color::fromRGB(0.95f, 0.40f, 0.10f);
static const cw::Color kPurple = cw::Color::fromRGB(0.60f, 0.20f, 0.80f);
static const cw::Color kRed    = cw::Color::fromRGB(0.85f, 0.20f, 0.15f);
static const cw::Color kTeal   = cw::Color::fromRGB(0.05f, 0.65f, 0.75f);
static const cw::Color kAmber  = cw::Color::fromRGB(0.95f, 0.65f, 0.05f);

// ---------------------------------------------------------------------------
// Design system switcher — lets the gallery live-switch between
// campello_ui/campello_material/campello_cupertino (plus light/dark),
// proving the DesignSystem abstraction end-to-end inside the flagship
// example. Owned by GalleryShellState; see buildSidebar()'s footer.
// ---------------------------------------------------------------------------
enum class GalleryDesignSystemKind
{
    campello_ui,
    material,
    cupertino,
    cupertino_glass,
};

static std::shared_ptr<const cw::DesignSystem> makeGalleryDesignSystem(GalleryDesignSystemKind kind, bool dark)
{
    switch (kind) {
        case GalleryDesignSystemKind::material:
            return std::make_shared<cw::MaterialDesignSystem>(
                dark ? cw::MaterialDesignSystem::dark() : cw::MaterialDesignSystem::light());
        case GalleryDesignSystemKind::cupertino:
            return std::make_shared<cw::CupertinoDesignSystem>(
                dark ? cw::CupertinoDesignSystem::dark() : cw::CupertinoDesignSystem::light());
        case GalleryDesignSystemKind::cupertino_glass:
            return std::make_shared<cw::CupertinoDesignSystem>(
                cw::CupertinoDesignSystem::liquidGlass(dark));
        case GalleryDesignSystemKind::campello_ui:
        default:
            return std::make_shared<cw::CampelloDesignSystem>(
                dark ? cw::CampelloDesignSystem::dark() : cw::CampelloDesignSystem::light());
    }
}

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------
static cw::TextStyle ts(float size, cw::Color color = cw::Color::fromRGB(0.10f, 0.10f, 0.10f))
{
    cw::TextStyle s{};
    s.font_size = size;
    s.color     = color;
    // Safe for every caller of this helper: RenderText only applies
    // tight_vertical_bounds when the text lays out to a single line, so
    // this is a no-op for the multi-line body copy elsewhere in the
    // gallery and only affects single-line labels (buttons, dialog
    // titles, ...) — see TextStyle::tight_vertical_bounds's doc.
    s.tight_vertical_bounds = true;
    return s;
}

static cw::WidgetRef ptr(cw::WidgetRef child)
{
    auto r    = std::make_shared<cw::MouseRegion>();
    r->cursor = cw::SystemMouseCursor::pointer;
    r->child  = std::move(child);
    return r;
}

static cw::WidgetRef repaintBoundary(cw::WidgetRef child)
{
    auto rb  = std::make_shared<cw::RepaintBoundary>();
    rb->child = std::move(child);
    return rb;
}

static cw::WidgetRef card(cw::WidgetRef content, float pad = 16.0f, cw::Color bg = cw::Color::white())
{
    cw::BoxDecoration deco;
    deco.color         = bg;
    deco.border_radius = 8.0f;
    deco.box_shadow    = { cw::BoxShadow{cw::Color::fromRGBA(0,0,0,0.07f), {0,2}, 6.0f, 0.0f} };
    auto box = std::make_shared<cw::DecoratedBox>(deco);
    box->child = cw::mw<cw::Padding>(cw::EdgeInsets::all(pad), std::move(content));
    return box;
}

static cw::WidgetRef subheading(const std::string& text, cw::Color color = cw::Color::fromRGB(0.5f, 0.5f, 0.55f))
{
    return cw::mw<cw::Padding>(
        cw::EdgeInsets::only(0.0f, 0.0f, 0.0f, 8.0f),
        cw::mw<cw::Text>(text, ts(11.0f, color)));
}

static cw::WidgetRef tapBtn(const std::string& label, cw::Color bg, std::function<void()> fn)
{
    cw::BoxDecoration deco;
    deco.color         = bg;
    deco.border_radius = 6.0f;
    auto box = std::make_shared<cw::DecoratedBox>(deco);
    box->child = cw::mw<cw::Padding>(
        cw::EdgeInsets::symmetric(14.0f, 7.0f),
        cw::mw<cw::Text>(label, ts(13.0f, cw::Color::white())));
    auto g  = std::make_shared<cw::GestureDetector>();
    g->on_tap = std::move(fn);
    g->child  = box;
    return ptr(g);
}

static cw::WidgetRef vspace(float h) { return cw::mw<cw::SizedBox>(std::nullopt, h); }
static cw::WidgetRef hspace(float w) { return cw::mw<cw::SizedBox>(w); }

// ---------------------------------------------------------------------------
// 1. LAYOUT — Wrap, Stack+Positioned, AspectRatio
// ---------------------------------------------------------------------------
class LayoutSection : public cw::StatelessWidget
{
public:
    cw::WidgetRef build(cw::BuildContext& ctx) const override
    {
        const auto* ds     = cw::Theme::of(ctx);
        const auto& colors = ds->tokens().colors;

        // --- Wrap: tag chips ---
        const std::vector<std::pair<std::string, cw::Color>> tags = {
            {"Row / Column", kBlue}, {"Stack", kPurple}, {"Wrap", kGreen},
            {"Expanded", kOrange},   {"Padding", kTeal}, {"Align", kRed},
            {"AspectRatio", kAmber}, {"Flexible", kBlue},{"SizedBox", kGreen},
            {"ConstrainedBox", kPurple}, {"IntrinsicWidth", kTeal},
        };
        std::vector<cw::WidgetRef> chips;
        for (auto& [label, color] : tags) {
            cw::BoxDecoration d;
            d.color = color; d.border_radius = 14.0f;
            auto box = std::make_shared<cw::DecoratedBox>(d);
            box->child = cw::mw<cw::Padding>(cw::EdgeInsets::symmetric(10.0f, 5.0f),
                cw::mw<cw::Text>(label, ts(12.0f, cw::Color::white())));
            chips.push_back(box);
        }
        auto wrap = std::make_shared<cw::Wrap>();
        wrap->spacing = 8.0f; wrap->run_spacing = 8.0f;
        wrap->children = chips;

        // --- Stack + Positioned ---
        auto mkBox = [](cw::Color c, float l, float t, float w, float h) -> cw::WidgetRef {
            cw::BoxDecoration d; d.color = c; d.border_radius = 6.0f;
            d.box_shadow = { cw::BoxShadow{cw::Color::fromRGBA(0,0,0,0.12f), {1,2}, 4.0f, 0.0f} };
            auto box = std::make_shared<cw::DecoratedBox>(d);
            box->child = cw::mw<cw::Padding>(cw::EdgeInsets::all(8.0f), nullptr);
            auto pos = std::make_shared<cw::Positioned>();
            pos->left = l; pos->top = t; pos->width = w; pos->height = h;
            pos->child = box;
            return pos;
        };
        auto stack_bg = std::make_shared<cw::Container>();
        stack_bg->color = colors.surface_variant;
        stack_bg->width = 320.0f; stack_bg->height = 130.0f;
        auto stack_inner = std::make_shared<cw::Stack>();
        stack_inner->children = {
            mkBox(kBlue,    8.0f,  12.0f, 130.0f, 85.0f),
            mkBox(kGreen,  70.0f,  30.0f, 110.0f, 75.0f),
            mkBox(kOrange, 135.0f,  8.0f, 140.0f, 95.0f),
        };
        stack_bg->child = stack_inner;

        // --- AspectRatio 16:9 ---
        auto ar_fill = std::make_shared<cw::Container>();
        ar_fill->color = kTeal;
        ar_fill->child = cw::mw<cw::Center>(
            cw::mw<cw::Text>("AspectRatio 16:9", ts(14.0f, cw::Color::white())));
        auto ar = std::make_shared<cw::AspectRatio>(16.0f / 9.0f, ar_fill);
        auto ar_clip = std::make_shared<cw::ClipRRect>(8.0f, ar);
        auto ar_box = std::make_shared<cw::ConstrainedBox>();
        ar_box->additional_constraints = cw::BoxConstraints{0.0f, 340.0f, 0.0f, 9999.0f};
        ar_box->child = ar_clip;

        auto scroll = std::make_shared<cw::SingleChildScrollView>();
        scroll->physics = std::make_shared<cw::BouncingScrollPhysics>();
        scroll->child   = cw::mw<cw::Padding>(cw::EdgeInsets::all(20.0f),
            cw::mw<cw::Column>(cw::MainAxisAlignment::start, cw::CrossAxisAlignment::start,
                cw::WidgetList{
                    subheading("WRAP — wraps children into multiple runs", colors.on_surface_variant),
                    card(wrap, 16.0f, colors.surface),
                    vspace(20.0f),
                    subheading("STACK + POSITIONED — overlapping layers", colors.on_surface_variant),
                    card(stack_bg, 8.0f, colors.surface),
                    vspace(20.0f),
                    subheading("ASPECT RATIO — maintains 16:9 regardless of width", colors.on_surface_variant),
                    ar_box,
                }));
        auto bg = std::make_shared<cw::Container>();
        bg->color = colors.surface_variant;
        bg->child = scroll;
        return bg;
    }
};

// ---------------------------------------------------------------------------
// 2. CONTROLS — Checkbox, Switch, Slider, RadioGroup, DropdownButton
// ---------------------------------------------------------------------------
class ControlsSection;

class ControlsState : public cw::State<ControlsSection>
{
public:
    void initState() override
    {
        cb_a_ = true; cb_b_ = false; cb_c_ = true;
        sw_a_ = true; sw_b_ = false;
        slider_ = 0.6f;
        radio_  = 1;
        dd_val_ = "Banana";
        exp_tile_expanded_ = false;
        toggle_selected_ = {true, false, false};
        nav_rail_selected_ = 0;
        banner_visible_ = true;
    }

    // Controls is the section that most fully demonstrates the
    // DesignSystem abstraction: every interactive control below goes
    // through Theme::of(ctx)->buildXxx() instead of raw widget
    // construction, so this section's look changes live with the
    // sidebar's design-system switcher. The other 9 sections (see
    // TODO.md Phase 16 M9) still keep their illustrative kBlue/kGreen/...
    // accent palette for demo variety, but their chrome — backgrounds,
    // cards, captions, body text — now also follows Theme::of(ctx).
    cw::WidgetRef build(cw::BuildContext& ctx) override
    {
        const auto* ds     = cw::Theme::of(ctx);
        const auto& colors = ds->tokens().colors;

        auto mkCb = [ds, &colors](const std::string& lbl, bool val, std::function<void(bool)> fn) {
            cw::CheckboxConfig cfg;
            cfg.value      = val;
            cfg.on_changed = std::move(fn);
            auto cb = ds->buildCheckbox(cfg);
            return cw::mw<cw::Row>(cw::MainAxisAlignment::start, cw::CrossAxisAlignment::center,
                cw::WidgetList{ cb, hspace(8.0f), cw::mw<cw::Text>(lbl, ts(14.0f, colors.on_surface)) });
        };

        auto mkSw = [ds, &colors](const std::string& lbl, bool val, std::function<void(bool)> fn) {
            cw::SwitchConfig cfg;
            cfg.value      = val;
            cfg.on_changed = std::move(fn);
            auto sw = ds->buildSwitch(cfg);
            return cw::mw<cw::Row>(cw::MainAxisAlignment::start, cw::CrossAxisAlignment::center,
                cw::WidgetList{ sw, hspace(10.0f), cw::mw<cw::Text>(lbl, ts(14.0f, colors.on_surface)) });
        };

        // Slider
        cw::SliderConfig slider_cfg;
        slider_cfg.value = slider_; slider_cfg.min = 0.0f; slider_cfg.max = 1.0f;
        slider_cfg.on_changed = [this](float v) { setState([this, v] { slider_ = v; }); };
        auto sl = ds->buildSlider(slider_cfg);
        std::ostringstream oss; oss << std::fixed << std::setprecision(2) << slider_;
        auto slider_row = cw::mw<cw::Row>(cw::MainAxisAlignment::start, cw::CrossAxisAlignment::center,
            cw::WidgetList{
                cw::mw<cw::Expanded>(sl),
                hspace(12.0f),
                cw::mw<cw::Text>(oss.str(), ts(13.0f, colors.primary)),
            });

        // RadioGroup — DesignSystem::buildRadio() returns a standalone
        // toggle with no group wiring, so this keeps using RadioGroup
        // directly, but pulls its accent colors from the active theme.
        auto mkRadio = [&colors](int val, const std::string& lbl) -> cw::WidgetRef {
            auto radio = std::make_shared<cw::Radio>(val);
            radio->active_color   = colors.primary;
            radio->inactive_color = colors.outline;
            return cw::mw<cw::Row>(cw::MainAxisAlignment::start, cw::CrossAxisAlignment::center,
                cw::WidgetList{
                    radio,
                    hspace(6.0f),
                    cw::mw<cw::Text>(lbl, ts(14.0f, colors.on_surface)),
                });
        };
        auto radio_col = cw::mw<cw::Column>(cw::MainAxisAlignment::start, cw::CrossAxisAlignment::start,
            cw::WidgetList{
                mkRadio(0, "Option Alpha"),
                vspace(6.0f),
                mkRadio(1, "Option Beta"),
                vspace(6.0f),
                mkRadio(2, "Option Gamma"),
            });
        auto rg = std::make_shared<cw::RadioGroup>();
        rg->group_value = radio_;
        rg->on_changed  = [this](int v) { setState([this, v] { radio_ = v; }); };
        rg->child       = radio_col;

        // DropdownButton
        cw::DropdownConfig dd_cfg;
        dd_cfg.items = {
            {"Apple", "Apple"}, {"Banana", "Banana"}, {"Cherry", "Cherry"},
            {"Dragonfruit", "Dragonfruit"}, {"Elderberry", "Elderberry"},
        };
        dd_cfg.selected_value = dd_val_;
        dd_cfg.hint = "Select fruit…";
        dd_cfg.on_changed = [this](std::string v) { setState([this, v] { dd_val_ = v; }); };
        auto dd = ds->buildDropdownButton(dd_cfg);
        auto dd_width = std::make_shared<cw::ConstrainedBox>();
        dd_width->additional_constraints = cw::BoxConstraints{0.0f, 240.0f, 0.0f, 9999.0f};
        dd_width->child = dd;

        // PopupMenuButton
        cw::PopupMenuConfig pmb_cfg;
        pmb_cfg.items = {
            {"Share",  [this]{ setState([this]{ pmb_choice_ = "Share";  }); }},
            {"Rename", [this]{ setState([this]{ pmb_choice_ = "Rename"; }); }},
            {"Delete", [this]{ setState([this]{ pmb_choice_ = "Delete"; }); }},
        };
        pmb_cfg.child = cw::mw<cw::Padding>(cw::EdgeInsets::symmetric(12.0f, 8.0f),
            cw::mw<cw::Text>("Actions ⋯", ts(14.0f, colors.on_surface)));
        auto pmb = ds->buildPopupMenuButton(pmb_cfg);
        auto pmb_row = cw::mw<cw::Row>(
            cw::MainAxisAlignment::start, cw::CrossAxisAlignment::center,
            cw::WidgetList{
                pmb,
                hspace(12.0f),
                cw::mw<cw::Text>(std::string("Last chosen: ") + pmb_choice_,
                                  ts(13.0f, colors.on_surface_variant)),
            });

        // Dialog (uses Overlay) — the button that opens it.
        cw::ButtonConfig dialog_open_cfg;
        dialog_open_cfg.label      = cw::mw<cw::Text>("Show Dialog", ts(14.0f, colors.on_primary));
        dialog_open_cfg.priority   = cw::ButtonPriority::primary;
        dialog_open_cfg.on_pressed = [ds, colors] {
            cw::DialogConfig cfg;
            cfg.title   = cw::mw<cw::Text>("Delete item?", ts(17.0f, colors.on_surface).bold());
            cfg.content = cw::mw<cw::Text>(
                "This action cannot be undone.", ts(13.0f, colors.on_surface_variant));
            // Boxed so the on_pressed closures below (which outlive this
            // lambda, living inside the overlay's own button widgets) can
            // capture it by value without dangling.
            auto entry_box = std::make_shared<std::shared_ptr<cw::OverlayEntry>>();
            cw::ButtonConfig cancel_cfg;
            cancel_cfg.label     = cw::mw<cw::Text>("Cancel", ts(15.0f, colors.primary));
            cancel_cfg.priority  = cw::ButtonPriority::tertiary;
            cancel_cfg.on_pressed = [entry_box] { if (*entry_box) cw::hideDialog(*entry_box); };
            cw::ButtonConfig delete_cfg;
            delete_cfg.label     = cw::mw<cw::Text>("Delete", ts(15.0f, colors.error).bold());
            delete_cfg.priority  = cw::ButtonPriority::tertiary;
            delete_cfg.on_pressed = [entry_box] { if (*entry_box) cw::hideDialog(*entry_box); };
            cfg.actions = { ds->buildButton(cancel_cfg), ds->buildButton(delete_cfg) };
            *entry_box = cw::showDialog(ds->buildDialog(cfg));
        };
        auto dialog_btn = ds->buildButton(dialog_open_cfg);

        // ActionSheet (uses Overlay) — no showActionSheet() core helper
        // (unlike Dialog's showDialog()), so this presents it by hand:
        // a dismiss barrier plus a bottom-aligned sheet, matching real
        // iOS action sheet placement.
        cw::ButtonConfig sheet_open_cfg;
        sheet_open_cfg.label      = cw::mw<cw::Text>("Show Action Sheet", ts(14.0f, colors.on_primary));
        sheet_open_cfg.priority   = cw::ButtonPriority::primary;
        sheet_open_cfg.on_pressed = [ds, colors] {
            auto entry_box = std::make_shared<std::shared_ptr<cw::OverlayEntry>>();
            auto dismiss = [entry_box] { if (*entry_box) cw::Overlay::remove(*entry_box); };

            cw::ActionSheetConfig cfg;
            cfg.title = cw::mw<cw::Text>("Photo Options", ts(13.0f, colors.on_surface_variant));
            cfg.actions = {
                {"Take Photo", [dismiss] { dismiss(); }, false},
                {"Choose from Library", [dismiss] { dismiss(); }, false},
                {"Delete Photo", [dismiss] { dismiss(); }, true},
            };
            cfg.on_cancel = dismiss;
            auto sheet = ds->buildActionSheet(cfg);

            auto barrier = cw::ModalBarrier::create(
                cw::Color::fromRGBA(0.0f, 0.0f, 0.0f, 0.4f), true, dismiss);
            auto padded_sheet = cw::mw<cw::Padding>(cw::EdgeInsets::all(8.0f), sheet);
            auto aligned = std::make_shared<cw::Align>(cw::Alignment::bottomCenter(), padded_sheet);
            std::vector<cw::WidgetRef> stack_children = {barrier, aligned};
            auto stack = cw::Stack::create(stack_children);
            stack->fit = cw::StackFit::expand;

            *entry_box = std::make_shared<cw::OverlayEntry>(stack);
            cw::Overlay::insert(*entry_box);
        };
        auto sheet_btn = ds->buildButton(sheet_open_cfg);

        // Tooltip (long-press to show, uses Overlay)
        cw::TooltipConfig tooltip_cfg;
        tooltip_cfg.message = "This is a tooltip";
        tooltip_cfg.child = cw::mw<cw::Padding>(cw::EdgeInsets::symmetric(14.0f, 8.0f),
            cw::mw<cw::Text>("Long-press me", ts(14.0f, colors.on_surface_variant)));
        auto tooltip_target = ds->buildTooltip(tooltip_cfg);

        // ExpansionTile
        cw::ExpansionTileConfig et_cfg;
        et_cfg.title    = cw::mw<cw::Text>("Advanced options", ts(14.0f, colors.on_surface));
        et_cfg.subtitle = cw::mw<cw::Text>("Tap to expand", ts(12.0f, colors.on_surface_variant));
        et_cfg.expanded = exp_tile_expanded_;
        et_cfg.on_expansion_changed = [this](bool v) { setState([this, v] { exp_tile_expanded_ = v; }); };
        et_cfg.children_content = cw::mw<cw::Padding>(cw::EdgeInsets::symmetric(0.0f, 8.0f),
            cw::mw<cw::Text>("Hidden content revealed when expanded.", ts(13.0f, colors.on_surface_variant)));
        auto et = ds->buildExpansionTile(et_cfg);

        // ToggleButtons
        cw::ToggleButtonsConfig tb_cfg;
        const char* tb_labels[] = {"Bold", "Italic", "Underline"};
        for (int i = 0; i < 3; ++i) {
            cw::ToggleButtonsConfig::Item item;
            item.label    = cw::mw<cw::Text>(tb_labels[i], ts(13.0f, colors.on_surface));
            item.selected = toggle_selected_[i];
            tb_cfg.items.push_back(item);
        }
        tb_cfg.on_pressed = [this](int i) { setState([this, i] { toggle_selected_[i] = !toggle_selected_[i]; }); };
        auto tb = ds->buildToggleButtons(tb_cfg);

        // Banner
        cw::WidgetRef banner_widget;
        if (banner_visible_) {
            cw::BannerConfig b_cfg;
            b_cfg.leading = cw::mw<cw::Text>("!", ts(16.0f, colors.error));
            b_cfg.content = cw::mw<cw::Text>("Your storage is almost full.", ts(14.0f, colors.on_surface));
            cw::ButtonConfig dismiss_cfg;
            dismiss_cfg.label     = cw::mw<cw::Text>("Dismiss", ts(13.0f, colors.primary));
            dismiss_cfg.priority  = cw::ButtonPriority::tertiary;
            dismiss_cfg.on_pressed = [this] { setState([this] { banner_visible_ = false; }); };
            b_cfg.actions = { ds->buildButton(dismiss_cfg) };
            banner_widget = ds->buildBanner(b_cfg);
        }

        // NavigationRail
        cw::NavigationRailConfig nr_cfg;
        nr_cfg.items = {
            {cw::mw<cw::Text>("H", ts(14.0f, colors.on_surface)), "Home"},
            {cw::mw<cw::Text>("S", ts(14.0f, colors.on_surface)), "Search"},
            {cw::mw<cw::Text>("P", ts(14.0f, colors.on_surface)), "Profile"},
        };
        nr_cfg.selected_index = nav_rail_selected_;
        nr_cfg.extended       = true;
        nr_cfg.on_tap = [this](int i) { setState([this, i] { nav_rail_selected_ = i; }); };
        auto nr = ds->buildNavigationRail(nr_cfg);
        auto nr_sized = std::make_shared<cw::ConstrainedBox>();
        nr_sized->additional_constraints = cw::BoxConstraints{0.0f, 200.0f, 0.0f, 9999.0f};
        nr_sized->child = nr;

        // DataTable
        cw::DataTableConfig dt_cfg;
        dt_cfg.columns = {"Name", "Role", "Status"};
        dt_cfg.rows = {
            {cw::mw<cw::Text>("Ada", ts(13.0f, colors.on_surface)),
             cw::mw<cw::Text>("Engineer", ts(13.0f, colors.on_surface_variant)),
             cw::mw<cw::Text>("Active", ts(13.0f, colors.primary))},
            {cw::mw<cw::Text>("Grace", ts(13.0f, colors.on_surface)),
             cw::mw<cw::Text>("Designer", ts(13.0f, colors.on_surface_variant)),
             cw::mw<cw::Text>("Away", ts(13.0f, colors.on_surface_variant))},
            {cw::mw<cw::Text>("Alan", ts(13.0f, colors.on_surface)),
             cw::mw<cw::Text>("Researcher", ts(13.0f, colors.on_surface_variant)),
             cw::mw<cw::Text>("Active", ts(13.0f, colors.primary))},
        };
        auto dt = ds->buildDataTable(dt_cfg);

        // Card (elevated) + PrimaryActionButton — the two components
        // currently wired for CupertinoDesignSystem's Liquid Glass
        // material (see cupertino_design_system.hpp's CupertinoMaterial
        // doc comment). Switch the sidebar switcher to "Glass" to see it:
        // a real DesignSystem-driven ClipRRect(BackdropFilter(liquidGlass))
        // card and a circular glass FAB, not just the raw ImageFilter
        // primitive demo in the Clipping & FX tab. Every other design
        // system kind renders these exactly as before (flat/solid) —
        // this row is a no-op visual change for UI/MD3/classic iOS.
        cw::CardConfig card_cfg;
        card_cfg.child = cw::mw<cw::Padding>(cw::EdgeInsets::all(4.0f),
            cw::mw<cw::Text>("ds->buildCard() — elevated priority",
                ts(13.0f, colors.on_surface)));
        auto ds_card = ds->buildCard(card_cfg);

        cw::PrimaryActionConfig fab_cfg;
        fab_cfg.on_pressed = [] {};
        auto fab = ds->buildPrimaryActionButton(fab_cfg);

        // Explicit height rather than left to ambient constraints: this
        // Row sits as a bare Column child inside a SingleChildScrollView
        // (unbounded incoming height), unlike every other Row-with-
        // Expanded in this file (e.g. slider_row), which is wrapped in
        // card() first and so already bounded before insertion. This
        // alone wasn't the whole story, though — see TODO.md's Liquid
        // Glass entry for the real bug it uncovered: buildPrimaryActionButton()
        // had backwards Align/SizedBox nesting in all three concrete
        // DesignSystems, which made the FAB expand to claim most of the
        // row instead of staying pinned to 56x56, independent of this
        // SizedBox. Fixed at the source (all three campello_*
        // implementations); this explicit height stays as cheap,
        // independent insurance against the row depending on ambient
        // constraints resolving favorably.
        auto card_fab_row = cw::mw<cw::SizedBox>(std::nullopt, 100.0f,
            cw::mw<cw::Row>(
                cw::MainAxisAlignment::start, cw::CrossAxisAlignment::center,
                cw::WidgetList{ cw::mw<cw::Expanded>(ds_card), hspace(16.0f), fab }));

        auto scroll = std::make_shared<cw::SingleChildScrollView>();
        scroll->physics = std::make_shared<cw::BouncingScrollPhysics>();
        scroll->child   = cw::mw<cw::Padding>(cw::EdgeInsets::all(20.0f),
            cw::mw<cw::Column>(cw::MainAxisAlignment::start, cw::CrossAxisAlignment::start,
                cw::WidgetList{
                    subheading("CHECKBOX", colors.on_surface_variant),
                    card(cw::mw<cw::Column>(cw::MainAxisAlignment::start, cw::CrossAxisAlignment::start,
                        cw::WidgetList{
                            mkCb("Enable feature A", cb_a_, [this](bool v){setState([this,v]{cb_a_=v;});}),
                            vspace(8.0f),
                            mkCb("Enable feature B", cb_b_, [this](bool v){setState([this,v]{cb_b_=v;});}),
                            vspace(8.0f),
                            mkCb("Enable feature C", cb_c_, [this](bool v){setState([this,v]{cb_c_=v;});}),
                        }), 16.0f, colors.surface),
                    vspace(20.0f),
                    subheading("SWITCH", colors.on_surface_variant),
                    card(cw::mw<cw::Column>(cw::MainAxisAlignment::start, cw::CrossAxisAlignment::start,
                        cw::WidgetList{
                            mkSw("Notifications", sw_a_, [this](bool v){setState([this,v]{sw_a_=v;});}),
                            vspace(10.0f),
                            mkSw("Dark mode", sw_b_, [this](bool v){setState([this,v]{sw_b_=v;});}),
                        }), 16.0f, colors.surface),
                    vspace(20.0f),
                    subheading("SLIDER", colors.on_surface_variant),
                    card(slider_row, 16.0f, colors.surface),
                    vspace(20.0f),
                    subheading("RADIO GROUP", colors.on_surface_variant),
                    card(rg, 16.0f, colors.surface),
                    vspace(20.0f),
                    subheading("DROPDOWN BUTTON (uses Overlay)", colors.on_surface_variant),
                    dd_width,
                    vspace(20.0f),
                    subheading("POPUP MENU BUTTON (Liquid Glass on \"Glass\")", colors.on_surface_variant),
                    pmb_row,
                    vspace(20.0f),
                    subheading("DIALOG (Liquid Glass on \"Glass\", uses Overlay)", colors.on_surface_variant),
                    dialog_btn,
                    vspace(20.0f),
                    subheading("ACTION SHEET (Liquid Glass on \"Glass\", uses Overlay)", colors.on_surface_variant),
                    sheet_btn,
                    vspace(20.0f),
                    subheading("TOOLTIP (Liquid Glass on \"Glass\", long-press, uses Overlay)", colors.on_surface_variant),
                    tooltip_target,
                    vspace(20.0f),
                    subheading("EXPANSION TILE", colors.on_surface_variant),
                    et,
                    vspace(20.0f),
                    subheading("TOGGLE BUTTONS", colors.on_surface_variant),
                    tb,
                    vspace(20.0f),
                    subheading("BANNER", colors.on_surface_variant),
                    banner_visible_ ? banner_widget
                        : cw::mw<cw::Text>("(dismissed)", ts(13.0f, colors.on_surface_variant)),
                    vspace(20.0f),
                    subheading("NAVIGATION RAIL", colors.on_surface_variant),
                    nr_sized,
                    vspace(20.0f),
                    subheading("DATA TABLE", colors.on_surface_variant),
                    dt,
                    vspace(20.0f),
                    subheading("CARD + PRIMARY ACTION BUTTON (Liquid Glass on \"Glass\")", colors.on_surface_variant),
                    card_fab_row,
                    vspace(20.0f),
                }));

        auto bg = std::make_shared<cw::Container>();
        bg->color = colors.surface_variant;
        bg->child = scroll;
        return bg;
    }

private:
    bool  cb_a_ = true, cb_b_ = false, cb_c_ = true;
    bool  sw_a_ = true, sw_b_ = false;
    float slider_ = 0.6f;
    int   radio_  = 1;
    std::string dd_val_ = "Banana";
    std::string pmb_choice_ = "(none)";
    bool  exp_tile_expanded_ = false;
    std::vector<bool> toggle_selected_ = {true, false, false};
    int   nav_rail_selected_ = 0;
    bool  banner_visible_ = true;
};

class ControlsSection : public cw::StatefulWidget {
public:
    std::unique_ptr<cw::StateBase> createState() const override
    { return std::make_unique<ControlsState>(); }
};

// ---------------------------------------------------------------------------
// 3. TEXT & INPUT — TextStyle variants, RichText, TextField
// ---------------------------------------------------------------------------
class TextSection;

class TextSectionState : public cw::State<TextSection>
{
public:
    void initState() override { ctrl_ = std::make_shared<cw::TextEditingController>(); }

    cw::WidgetRef build(cw::BuildContext& ctx) override
    {
        const auto* ds     = cw::Theme::of(ctx);
        const auto& colors = ds->tokens().colors;

        // Size showcase
        std::vector<cw::WidgetRef> sizes;
        for (float sz : {10.0f, 12.0f, 14.0f, 18.0f, 24.0f, 32.0f, 48.0f}) {
            std::ostringstream o; o << sz << "px — The quick brown fox";
            sizes.push_back(cw::mw<cw::Text>(o.str(), ts(sz, colors.on_surface)));
        }

        // Weight / style showcase
        auto bold_style = ts(16.0f, colors.on_surface); bold_style.font_weight = cw::FontWeight::bold;
        auto italic_style = ts(16.0f, colors.on_surface); italic_style.italic = true;
        auto combo_style  = ts(16.0f, colors.on_surface);
        combo_style.font_weight = cw::FontWeight::bold; combo_style.italic = true;
        auto weights = cw::mw<cw::Column>(cw::MainAxisAlignment::start, cw::CrossAxisAlignment::start,
            cw::WidgetList{
                cw::mw<cw::Text>("Regular weight", ts(16.0f, colors.on_surface)),
                vspace(4.0f),
                cw::mw<cw::Text>("Bold weight", bold_style),
                vspace(4.0f),
                cw::mw<cw::Text>("Italic style", italic_style),
                vspace(4.0f),
                cw::mw<cw::Text>("Bold + Italic", combo_style),
            });

        // RichText / inline spans
        auto bold_span_style = ts(15.0f, kOrange);
        bold_span_style.font_weight = cw::FontWeight::bold;
        std::vector<std::shared_ptr<cw::InlineSpan>> spans = {
            cw::InlineTextSpan::create("Rich text with ", ts(15.0f, colors.on_surface)),
            cw::InlineTextSpan::create("mixed", ts(15.0f, kBlue)),
            cw::InlineTextSpan::create(" colors and ", ts(15.0f, colors.on_surface)),
            cw::InlineTextSpan::create("bold", bold_span_style),
            cw::InlineTextSpan::create(" inline spans.", ts(15.0f, colors.on_surface)),
        };
        auto rich = cw::RichText::create(spans);

        // Single-line TextField
        auto tf = std::make_shared<cw::TextField>();
        tf->controller  = ctrl_;
        tf->placeholder = "Type something…";
        tf->style       = ts(15.0f, colors.on_surface);

        auto scroll = std::make_shared<cw::SingleChildScrollView>();
        scroll->physics = std::make_shared<cw::BouncingScrollPhysics>();
        scroll->child   = cw::mw<cw::Padding>(cw::EdgeInsets::all(20.0f),
            cw::mw<cw::Column>(cw::MainAxisAlignment::start, cw::CrossAxisAlignment::start,
                cw::WidgetList{
                    subheading("TEXT SIZES", colors.on_surface_variant),
                    card(cw::mw<cw::Column>(cw::MainAxisAlignment::start,
                        cw::CrossAxisAlignment::start, sizes), 16.0f, colors.surface),
                    vspace(20.0f),
                    subheading("FONT WEIGHT & STYLE", colors.on_surface_variant),
                    card(weights, 16.0f, colors.surface),
                    vspace(20.0f),
                    subheading("RICH TEXT", colors.on_surface_variant),
                    card(rich, 16.0f, colors.surface),
                    vspace(20.0f),
                    subheading("TEXT FIELD", colors.on_surface_variant),
                    tf,
                    vspace(20.0f),
                }));

        auto bg = std::make_shared<cw::Container>();
        bg->color = colors.surface_variant;
        bg->child = scroll;
        return bg;
    }

private:
    std::shared_ptr<cw::TextEditingController> ctrl_;
};

class TextSection : public cw::StatefulWidget {
public:
    std::unique_ptr<cw::StateBase> createState() const override
    { return std::make_unique<TextSectionState>(); }
};

// ---------------------------------------------------------------------------
// 4. LISTS — ListView (virtualized), GridView (3-col)
// ---------------------------------------------------------------------------
class ListsSection;

class ListsSectionState : public cw::State<ListsSection>
{
public:
    void initState() override { tab_ = 0; selected_ = -1; }

    cw::WidgetRef build(cw::BuildContext& ctx) override
    {
        const auto* ds     = cw::Theme::of(ctx);
        const auto& colors = ds->tokens().colors;

        auto mkTab = [this, &colors](const std::string& label, int idx) -> cw::WidgetRef {
            const bool active = (tab_ == idx);
            cw::BoxDecoration d;
            d.color = active ? colors.primary : colors.surface_variant;
            d.border_radius = 6.0f;
            auto box = std::make_shared<cw::DecoratedBox>(d);
            box->child = cw::mw<cw::Padding>(cw::EdgeInsets::symmetric(16.0f, 8.0f),
                cw::mw<cw::Text>(label, ts(13.0f, active ? colors.on_primary
                    : colors.on_surface_variant)));
            auto g = std::make_shared<cw::GestureDetector>();
            g->on_tap = [this, idx] { setState([this, idx] { tab_ = idx; selected_ = -1; }); };
            g->child  = box;
            return ptr(g);
        };

        auto tab_bar = cw::mw<cw::Padding>(cw::EdgeInsets::all(10.0f),
            cw::mw<cw::Row>(cw::MainAxisAlignment::start, cw::CrossAxisAlignment::center,
                cw::WidgetList{ mkTab("ListView", 0), hspace(8.0f), mkTab("GridView", 1) }));

        cw::WidgetRef list_content;
        if (tab_ == 0)
        {
            const int kCount = 200;
            auto lv = std::make_shared<cw::ListView>();
            lv->item_count  = kCount;
            lv->item_extent = 52.0f;
            lv->physics     = std::make_shared<cw::BouncingScrollPhysics>();
            lv->builder = [this, colors](cw::BuildContext&, int i) -> cw::WidgetRef {
                const bool sel = (i == selected_);
                const cw::Color avatar_palette[] = { kBlue, kGreen, kOrange, kPurple, kTeal };
                cw::Color av_color = avatar_palette[i % 5];

                auto avatar = std::make_shared<cw::Container>();
                avatar->width = 36.0f; avatar->height = 36.0f; avatar->color = av_color;
                avatar->child = cw::mw<cw::Center>(
                    cw::mw<cw::Text>(std::to_string(i + 1), ts(13.0f, cw::Color::white())));
                auto av_clip = std::make_shared<cw::ClipOval>(avatar);

                auto row = cw::mw<cw::Row>(cw::MainAxisAlignment::start, cw::CrossAxisAlignment::center,
                    cw::WidgetList{
                        av_clip, hspace(12.0f),
                        cw::mw<cw::Column>(cw::MainAxisAlignment::center, cw::CrossAxisAlignment::start,
                            cw::WidgetList{
                                cw::mw<cw::Text>("Item " + std::to_string(i + 1),
                                    ts(14.0f, sel ? colors.primary : colors.on_surface)),
                                vspace(2.0f),
                                cw::mw<cw::Text>("Scroll me — row " + std::to_string(i + 1),
                                    ts(11.0f, colors.on_surface_variant)),
                            }),
                    });

                auto cell = std::make_shared<cw::Container>();
                cell->padding = cw::EdgeInsets::symmetric(16.0f, 0.0f);
                cell->color   = sel ? colors.primary_container
                                    : (i % 2 ? colors.surface_variant : colors.surface);
                cell->child = row;

                auto g = std::make_shared<cw::GestureDetector>();
                g->on_tap = [this, i] { setState([this, i] { selected_ = (selected_ == i ? -1 : i); }); };
                g->child  = cell;
                return ptr(g);
            };
            list_content = lv;
        }
        else
        {
            const int kCount = 120;
            auto gv = std::make_shared<cw::GridView>();
            gv->item_count       = kCount;
            gv->item_extent      = 100.0f;
            gv->cross_axis_count = 4;
            gv->physics          = std::make_shared<cw::BouncingScrollPhysics>();
            gv->builder = [this, colors](cw::BuildContext&, int i) -> cw::WidgetRef {
                const cw::Color grid_palette[] = { kBlue, kGreen, kOrange, kPurple, kTeal, kRed, kAmber };
                cw::Color c = grid_palette[i % 7];
                const bool sel = (i == selected_);
                auto fill = std::make_shared<cw::Container>();
                fill->color = sel ? colors.primary_container : c;
                fill->child = cw::mw<cw::Center>(
                    cw::mw<cw::Text>(std::to_string(i + 1),
                        ts(16.0f, sel ? colors.on_primary_container : cw::Color::white())));
                auto clip = std::make_shared<cw::ClipRRect>(8.0f, fill);
                auto pad  = std::make_shared<cw::Padding>();
                pad->padding = cw::EdgeInsets::all(4.0f); pad->child = clip;
                auto g = std::make_shared<cw::GestureDetector>();
                g->on_tap = [this, i] { setState([this, i] { selected_ = (selected_ == i ? -1 : i); }); };
                g->child  = pad;
                return ptr(g);
            };
            list_content = gv;
        }

        auto root = cw::mw<cw::Column>(cw::MainAxisAlignment::start, cw::CrossAxisAlignment::stretch,
            cw::WidgetList{
                tab_bar,
                cw::mw<cw::Expanded>(list_content),
            });

        auto bg = std::make_shared<cw::Container>();
        bg->color = colors.surface_variant;
        bg->child = root;
        return bg;
    }

private:
    int tab_      = 0;
    int selected_ = -1;
};

class ListsSection : public cw::StatefulWidget {
public:
    std::unique_ptr<cw::StateBase> createState() const override
    { return std::make_unique<ListsSectionState>(); }
};

// ---------------------------------------------------------------------------
// 5. ANIMATIONS — AnimatedSwitcher, AnimatedAlign, explicit transitions
// ---------------------------------------------------------------------------
class AnimationsSection;

class AnimationsSectionState : public cw::State<AnimationsSection>
{
public:
    void initState() override
    {
        show_a_    = true;
        align_idx_ = 0;
        ctrl_      = std::make_shared<cw::AnimationController>(700.0);

        // AnimatedSwitcher determines "did the child change" by pointer
        // identity, not content — build() must hand it the same WidgetRef
        // every time show_a_ hasn't actually changed, or every unrelated
        // setState() in this section (Play, Next corner, ...) spuriously
        // retriggers its cross-fade. Build both variants once and pick
        // between the stable instances instead of constructing fresh each
        // build().
        auto make_sw_box = [](const cw::Color& color, const char* label) {
            auto box = std::make_shared<cw::Container>();
            box->width = 160.0f; box->height = 60.0f; box->color = color;
            box->child = cw::mw<cw::Center>(cw::mw<cw::Text>(label, ts(15.0f, cw::Color::white())));
            return box;
        };
        widget_a_box_ = make_sw_box(kBlue, "Widget A");
        widget_b_box_ = make_sw_box(kGreen, "Widget B");
    }

    cw::WidgetRef build(cw::BuildContext& ctx) override
    {
        const auto* ds     = cw::Theme::of(ctx);
        const auto& colors = ds->tokens().colors;

        // AnimatedSwitcher
        auto switcher = std::make_shared<cw::AnimatedSwitcher>();
        switcher->child       = show_a_ ? widget_a_box_ : widget_b_box_;
        switcher->duration_ms = 350.0;

        // AnimatedAlign
        const cw::Alignment corners[] = {
            cw::Alignment::topLeft(), cw::Alignment::topRight(),
            cw::Alignment::bottomRight(), cw::Alignment::bottomLeft(),
        };
        auto dot = std::make_shared<cw::Container>();
        dot->width = 40.0f; dot->height = 40.0f; dot->color = kPurple;
        dot->child = cw::mw<cw::ClipOval>(dot);
        auto dot_clipped = std::make_shared<cw::ClipOval>(
            [](){
                auto c = std::make_shared<cw::Container>();
                c->width = 40.0f; c->height = 40.0f; c->color = kPurple;
                return std::static_pointer_cast<cw::Widget>(c);
            }());
        auto aa = std::make_shared<cw::AnimatedAlign>();
        aa->alignment   = corners[align_idx_ % 4];
        aa->duration_ms = 500.0;
        aa->curve       = cw::Curves::easeInOut;
        aa->child       = dot_clipped;
        auto aa_box = std::make_shared<cw::Container>();
        aa_box->width = 280.0f; aa_box->height = 100.0f;
        aa_box->color = colors.surface_variant;
        aa_box->child = aa;

        // Explicit transitions: FadeTransition, RotationTransition, ScaleTransition
        auto spinner = std::make_shared<cw::Container>();
        spinner->width = 40.0f; spinner->height = 40.0f; spinner->color = kOrange;
        auto rot = std::make_shared<cw::RotationTransition>(ctrl_);
        rot->curve = cw::Curves::easeInOut;
        rot->turns = { 0.0f, 1.0f };
        rot->child = spinner;

        auto fader_box = std::make_shared<cw::Container>();
        fader_box->width = 80.0f; fader_box->height = 40.0f; fader_box->color = kGreen;
        fader_box->child = cw::mw<cw::Center>(cw::mw<cw::Text>("fade", ts(13.0f, cw::Color::white())));
        auto fade = std::make_shared<cw::FadeTransition>(ctrl_);
        fade->opacity = { 0.0f, 1.0f };
        fade->curve   = cw::Curves::easeInOut;
        fade->child   = fader_box;

        auto scaler_box = std::make_shared<cw::Container>();
        scaler_box->width = 60.0f; scaler_box->height = 60.0f; scaler_box->color = kTeal;
        auto scale = std::make_shared<cw::ScaleTransition>(ctrl_);
        scale->scale = { 0.0f, 1.0f };
        scale->curve = cw::Curves::easeInOut;
        scale->child = scaler_box;

        auto transitions_row = cw::mw<cw::Row>(
            cw::MainAxisAlignment::spaceEvenly, cw::CrossAxisAlignment::center,
            cw::WidgetList{ rot, fade, scale });

        auto play_btn = tapBtn(
            ctrl_->status() == cw::AnimationStatus::forward ||
            ctrl_->status() == cw::AnimationStatus::completed ? "Reverse" : "Play",
            kBlue, [this] {
                if (ctrl_->status() == cw::AnimationStatus::completed ||
                    ctrl_->status() == cw::AnimationStatus::forward)
                    ctrl_->reverse();
                else
                    ctrl_->forward();
                setState([] {});
            });

        // Rebuild on animation tick
        auto anim_builder = std::make_shared<cw::AnimatedBuilder>();
        anim_builder->animation = ctrl_;
        anim_builder->builder = [this, transitions_row, play_btn](cw::BuildContext&) -> cw::WidgetRef {
            return cw::mw<cw::Column>(cw::MainAxisAlignment::start, cw::CrossAxisAlignment::start,
                cw::WidgetList{
                    transitions_row,
                    vspace(12.0f),
                    play_btn,
                });
        };

        auto scroll = std::make_shared<cw::SingleChildScrollView>();
        scroll->physics = std::make_shared<cw::BouncingScrollPhysics>();
        scroll->child   = cw::mw<cw::Padding>(cw::EdgeInsets::all(20.0f),
            cw::mw<cw::Column>(cw::MainAxisAlignment::start, cw::CrossAxisAlignment::start,
                cw::WidgetList{
                    subheading("ANIMATED SWITCHER — tap to swap widgets with cross-fade", colors.on_surface_variant),
                    card(cw::mw<cw::Column>(cw::MainAxisAlignment::start, cw::CrossAxisAlignment::start,
                        cw::WidgetList{
                            switcher,
                            vspace(10.0f),
                            tapBtn("Swap", kPurple, [this] { setState([this] { show_a_ = !show_a_; }); }),
                        }), 16.0f, colors.surface),
                    vspace(20.0f),
                    subheading("ANIMATED ALIGN — tap to cycle corners", colors.on_surface_variant),
                    card(cw::mw<cw::Column>(cw::MainAxisAlignment::start, cw::CrossAxisAlignment::start,
                        cw::WidgetList{
                            aa_box,
                            vspace(10.0f),
                            tapBtn("Next corner", kTeal, [this] {
                                setState([this] { align_idx_ = (align_idx_ + 1) % 4; });
                            }),
                        }), 16.0f, colors.surface),
                    vspace(20.0f),
                    subheading("EXPLICIT TRANSITIONS — Rotate / Fade / Scale", colors.on_surface_variant),
                    card(anim_builder, 16.0f, colors.surface),
                }));

        auto bg = std::make_shared<cw::Container>();
        bg->color = colors.surface_variant;
        bg->child = scroll;
        return bg;
    }

private:
    bool  show_a_    = true;
    int   align_idx_ = 0;
    std::shared_ptr<cw::AnimationController> ctrl_;
    cw::WidgetRef widget_a_box_;
    cw::WidgetRef widget_b_box_;
};

class AnimationsSection : public cw::StatefulWidget {
public:
    std::unique_ptr<cw::StateBase> createState() const override
    { return std::make_unique<AnimationsSectionState>(); }
};

// ---------------------------------------------------------------------------
// 6. GESTURES — GestureDetector + Draggable / DragTarget
// ---------------------------------------------------------------------------
class GesturesSection;

class GesturesSectionState : public cw::State<GesturesSection>
{
public:
    void initState() override
    {
        gesture_   = "interact with the zone";
        detail_    = "";
        has_interacted_ = false;
        dropped_   = 0;
    }

    cw::WidgetRef build(cw::BuildContext& ctx) override
    {
        const auto* ds     = cw::Theme::of(ctx);
        const auto& colors = ds->tokens().colors;

        const cw::Color zone_color = has_interacted_ ? zone_color_ : colors.surface_variant;
        const cw::Color text_color = has_interacted_ ? text_color_ : colors.on_surface_variant;

        // GestureDetector zone
        auto zone_center = cw::mw<cw::Column>();
        {
            auto col = std::static_pointer_cast<cw::Column>(zone_center);
            col->main_axis_alignment  = cw::MainAxisAlignment::center;
            col->cross_axis_alignment = cw::CrossAxisAlignment::center;
            col->main_axis_size       = cw::MainAxisSize::min;
            col->children = {
                cw::mw<cw::Text>(gesture_, ts(28.0f, text_color)),
                vspace(6.0f),
                cw::mw<cw::Text>(detail_, ts(12.0f, cw::Color::fromRGB(0.85f,0.85f,0.90f))),
            };
        }
        auto zone_bg = std::make_shared<cw::Container>();
        zone_bg->color  = zone_color;
        zone_bg->child  = cw::mw<cw::Center>(zone_center);

        auto detector = std::make_shared<cw::GestureDetector>();
        detector->child = zone_bg;
        detector->on_tap = [this] {
            setState([this] {
                gesture_="tap"; detail_=""; zone_color_=kBlue; text_color_=cw::Color::white();
                has_interacted_ = true;
            });
        };
        detector->on_double_tap = [this] {
            setState([this] {
                gesture_="double tap"; detail_=""; zone_color_=kGreen; text_color_=cw::Color::white();
                has_interacted_ = true;
            });
        };
        detector->on_long_press = [this] {
            setState([this] {
                gesture_="long press"; detail_=""; zone_color_=kOrange; text_color_=cw::Color::white();
                has_interacted_ = true;
            });
        };
        detector->on_pan_update = [this](cw::Offset d) {
            std::ostringstream o;
            o << std::fixed << std::setprecision(1) << "Δ " << d.x << ", " << d.y;
            setState([this, s = o.str()] {
                gesture_="pan"; detail_=s; zone_color_=kPurple; text_color_=cw::Color::white();
                has_interacted_ = true;
            });
        };
        detector->on_pan_end = [this] {
            setState([this] {
                gesture_="pan end"; detail_=""; zone_color_=cw::Color::fromRGB(0.60f,0.40f,0.85f);
                text_color_=cw::Color::white();
                has_interacted_ = true;
            });
        };
        detector->on_scroll = [this](cw::Offset d) {
            std::ostringstream o;
            o << std::fixed << std::setprecision(1) << "Δ " << d.x << ", " << d.y;
            setState([this, s = o.str()] {
                gesture_="scroll"; detail_=s; zone_color_=kTeal; text_color_=cw::Color::white();
                has_interacted_ = true;
            });
        };

        // Drag-and-drop
        auto draggable_content = std::make_shared<cw::Container>();
        draggable_content->width = 80.0f; draggable_content->height = 80.0f;
        draggable_content->color = kOrange;
        draggable_content->child = cw::mw<cw::Center>(
            cw::mw<cw::Text>("drag\nme", ts(12.0f, cw::Color::white())));
        auto draggable_clip = std::make_shared<cw::ClipRRect>(12.0f, draggable_content);

        auto feedback_box = std::make_shared<cw::Container>();
        feedback_box->width = 80.0f; feedback_box->height = 80.0f;
        feedback_box->color = cw::Color::fromRGBA(0.95f, 0.40f, 0.10f, 0.7f);
        feedback_box->child = cw::mw<cw::Center>(
            cw::mw<cw::Text>("drop!", ts(12.0f, cw::Color::white())));
        auto feedback_clip = std::make_shared<cw::ClipRRect>(12.0f, feedback_box);

        auto drag = std::make_shared<cw::Draggable<int>>();
        drag->data     = 1;
        drag->child    = draggable_clip;
        drag->feedback = feedback_clip;

        int dropped_count = dropped_;
        auto target_widget = std::make_shared<cw::DragTarget<int>>();
        target_widget->builder = [dropped_count, colors](cw::BuildContext&, bool hovering) -> cw::WidgetRef {
            auto box = std::make_shared<cw::Container>();
            box->width = 160.0f; box->height = 80.0f;
            box->color = hovering ? kGreen : colors.surface_variant;
            const std::string label = dropped_count > 0
                ? "dropped " + std::to_string(dropped_count) + "x"
                : (hovering ? "release!" : "drop here");
            box->child = cw::mw<cw::Center>(
                cw::mw<cw::Text>(label, ts(13.0f,
                    hovering || dropped_count > 0 ? cw::Color::white()
                        : colors.on_surface_variant)));
            return std::make_shared<cw::ClipRRect>(12.0f, box);
        };
        target_widget->on_accept = [this](const int&) {
            setState([this] { dropped_++; });
        };

        auto dnd_row = cw::mw<cw::Row>(
            cw::MainAxisAlignment::start, cw::CrossAxisAlignment::center,
            cw::WidgetList{ drag, hspace(24.0f),
                cw::mw<cw::Text>("→", ts(20.0f, colors.on_surface_variant)),
                hspace(24.0f), target_widget });

        auto content = cw::mw<cw::Padding>(cw::EdgeInsets::all(20.0f),
            cw::mw<cw::Column>(cw::MainAxisAlignment::start, cw::CrossAxisAlignment::stretch,
                cw::WidgetList{
                    subheading("GESTURE DETECTOR — tap · double-tap · long-press · pan · scroll", colors.on_surface_variant),
                    cw::mw<cw::Expanded>(card(detector, 0.0f, colors.surface)),
                    vspace(20.0f),
                    subheading("DRAGGABLE + DRAG TARGET", colors.on_surface_variant),
                    card(dnd_row, 16.0f, colors.surface),
                }));

        auto bg = std::make_shared<cw::Container>();
        bg->color = colors.surface_variant;
        bg->child = content;
        return bg;
    }

private:
    std::string gesture_, detail_;
    cw::Color   zone_color_ = cw::Color::white(), text_color_ = cw::Color::black();
    bool        has_interacted_ = false;
    int         dropped_ = 0;
};

class GesturesSection : public cw::StatefulWidget {
public:
    std::unique_ptr<cw::StateBase> createState() const override
    { return std::make_unique<GesturesSectionState>(); }
};

// ---------------------------------------------------------------------------
// LambdaCustomPainter — wraps a plain draw function as a CustomPainter, so a
// handful of small one-off Canvas demos (see the STROKE CAPS & JOINS samples
// below) don't each need their own subclass. Static content only, so
// shouldRepaint() always returns false — nothing here ever animates.
// ---------------------------------------------------------------------------
class LambdaCustomPainter : public cw::CustomPainter
{
public:
    explicit LambdaCustomPainter(std::function<void(cw::Canvas&, cw::Size)> draw)
        : draw_(std::move(draw)) {}

    void paint(cw::Canvas& canvas, cw::Size size) override { draw_(canvas, size); }
    bool shouldRepaint(const cw::CustomPainter&) const override { return false; }

private:
    std::function<void(cw::Canvas&, cw::Size)> draw_;
};

// A labeled Canvas demo cell: fixed-size RawCustomPaint over `draw`, with a
// caption underneath — mirrors fitSample()'s box+label composition below.
static cw::WidgetRef strokeSample(
    const std::string& label, float w, float h,
    std::function<void(cw::Canvas&, cw::Size)> draw, cw::Color label_color)
{
    auto painter = std::make_shared<LambdaCustomPainter>(std::move(draw));
    auto canvas_box = std::make_shared<cw::SizedBox>(w, h, cw::RawCustomPaint::create(painter));

    return cw::mw<cw::Column>(cw::MainAxisAlignment::start, cw::CrossAxisAlignment::center,
        cw::WidgetList{
            canvas_box,
            vspace(6.0f),
            cw::mw<cw::Text>(label, ts(11.0f, label_color)),
        });
}

static cw::Paint strokeDemoPaint(
    cw::Color color, float width, cw::StrokeCap cap, cw::StrokeJoin join, float miter_limit = 4.0f)
{
    cw::Paint p;
    p.style             = cw::PaintStyle::stroke;
    p.color             = color;
    p.stroke_width       = width;
    p.stroke_cap         = cap;
    p.stroke_join        = join;
    p.stroke_miter_limit = miter_limit;
    return p;
}

// A thick horizontal stroke with thin white tick marks at its true (logical)
// endpoints, so the cap style's effect beyond/at those endpoints is visible:
// butt stops exactly on the tick, round/square extend past it.
static std::function<void(cw::Canvas&, cw::Size)> strokeCapDemo(cw::StrokeCap cap, cw::Color color)
{
    return [cap, color](cw::Canvas& canvas, cw::Size size) {
        const cw::Offset p1{28.0f, size.height * 0.5f};
        const cw::Offset p2{size.width - 28.0f, size.height * 0.5f};
        canvas.drawLine(p1, p2, strokeDemoPaint(color, 26.0f, cap, cw::StrokeJoin::miter));

        const cw::Paint tick = strokeDemoPaint(cw::Color::white(), 2.0f, cw::StrokeCap::butt, cw::StrokeJoin::miter);
        canvas.drawLine({p1.x, p1.y - 20.0f}, {p1.x, p1.y + 20.0f}, tick);
        canvas.drawLine({p2.x, p2.y - 20.0f}, {p2.x, p2.y + 20.0f}, tick);
    };
}

// A two-segment corner (a "V" opening upward from a hub near the bottom),
// stroked with the given join style. `interior_deg` is the corner's own
// interior angle at the hub — smaller means sharper, which is what pushes
// a miter join's spike length past `miter_limit` (see buildStrokeGeometry()'s
// miter-limit fallback in src/gpu/stroke_geometry.cpp).
static std::function<void(cw::Canvas&, cw::Size)> strokeJoinDemo(
    cw::StrokeJoin join, cw::Color color, float interior_deg, float miter_limit = 4.0f)
{
    return [join, color, interior_deg, miter_limit](cw::Canvas& canvas, cw::Size size) {
        const cw::Offset hub{size.width * 0.5f, size.height * 0.78f};
        const float half_rad = (interior_deg * 0.5f) * (3.14159265f / 180.0f);
        const float arm      = size.height * 0.62f;

        cw::Path path;
        path.moveTo(hub.x - arm * std::sin(half_rad), hub.y - arm * std::cos(half_rad));
        path.lineTo(hub);
        path.lineTo(hub.x + arm * std::sin(half_rad), hub.y - arm * std::cos(half_rad));

        canvas.drawPath(path, strokeDemoPaint(color, 20.0f, cw::StrokeCap::butt, join, miter_limit));
    };
}

static void strokeRotatedRectDemo(cw::Canvas& canvas, cw::Size size)
{
    canvas.save();
    canvas.translate(size.width * 0.5f, size.height * 0.5f);
    canvas.rotate(30.0f * (3.14159265f / 180.0f));
    const float half = std::min(size.width, size.height) * 0.30f;
    canvas.drawRect(
        cw::Rect::fromLTWH(-half, -half, half * 2.0f, half * 2.0f),
        strokeDemoPaint(kBlue, 14.0f, cw::StrokeCap::butt, cw::StrokeJoin::round));
    canvas.restore();
}

// ---------------------------------------------------------------------------
// 7. CLIPPING & FX — ClipRRect, ClipOval, DecoratedBox, Opacity, BackdropFilter
// ---------------------------------------------------------------------------
class ClippingSection : public cw::StatelessWidget
{
public:
    cw::WidgetRef build(cw::BuildContext& ctx) const override
    {
        const auto* ds     = cw::Theme::of(ctx);
        const auto& colors = ds->tokens().colors;

        // ClipRRect
        auto rrect_box = std::make_shared<cw::Container>();
        rrect_box->width = 120.0f; rrect_box->height = 80.0f; rrect_box->color = kBlue;
        rrect_box->child = cw::mw<cw::Center>(cw::mw<cw::Text>("ClipRRect\nr=20", ts(12.0f, cw::Color::white())));
        auto rrect = std::make_shared<cw::ClipRRect>(20.0f, rrect_box);

        // ClipOval
        auto oval_box = std::make_shared<cw::Container>();
        oval_box->width = 100.0f; oval_box->height = 80.0f; oval_box->color = kGreen;
        oval_box->child = cw::mw<cw::Center>(cw::mw<cw::Text>("ClipOval", ts(12.0f, cw::Color::white())));
        auto oval = std::make_shared<cw::ClipOval>(oval_box);

        auto clips_row = cw::mw<cw::Row>(cw::MainAxisAlignment::start, cw::CrossAxisAlignment::center,
            cw::WidgetList{ rrect, hspace(20.0f), oval });

        // DecoratedBox with border + shadow
        cw::BoxDecoration fancy;
        fancy.color         = colors.surface;
        fancy.border_radius = 12.0f;
        fancy.border        = cw::BoxBorder::all(kBlue, 2.0f);
        fancy.box_shadow    = {
            cw::BoxShadow{cw::Color::fromRGBA(0,0,0,0.15f), {0,4}, 12.0f, 0.0f},
            cw::BoxShadow{cw::Color::fromRGBA(0.08f,0.47f,0.95f,0.2f), {0,0}, 8.0f, 2.0f},
        };
        auto fancy_box = std::make_shared<cw::DecoratedBox>(fancy);
        fancy_box->child = cw::mw<cw::Padding>(cw::EdgeInsets::all(20.0f),
            cw::mw<cw::Text>("DecoratedBox\nborder + dual shadow", ts(13.0f, kBlue)));

        // Gradient fills — Linear / Radial / Sweep BoxGradient
        auto linear_grad_box = std::make_shared<cw::Container>();
        linear_grad_box->width  = 140.0f;
        linear_grad_box->height = 90.0f;
        linear_grad_box->decoration = cw::BoxDecoration{
            .gradient = cw::LinearBoxGradient{
                .begin  = cw::Alignment::topLeft(),
                .end    = cw::Alignment::bottomRight(),
                .colors = {kBlue, kPurple},
            },
            .border_radius = 12.0f,
        };
        linear_grad_box->child = cw::mw<cw::Center>(
            cw::mw<cw::Text>("Linear", ts(13.0f, cw::Color::white())));

        auto radial_grad_box = std::make_shared<cw::Container>();
        radial_grad_box->width  = 140.0f;
        radial_grad_box->height = 90.0f;
        radial_grad_box->decoration = cw::BoxDecoration{
            .gradient = cw::RadialBoxGradient{
                .colors = {cw::Color::white(), kOrange},
            },
            .border_radius = 12.0f,
        };
        radial_grad_box->child = cw::mw<cw::Center>(
            cw::mw<cw::Text>("Radial", ts(13.0f, cw::Color::fromRGB(0.3f, 0.15f, 0.0f))));

        auto sweep_grad_box = std::make_shared<cw::Container>();
        sweep_grad_box->width  = 140.0f;
        sweep_grad_box->height = 90.0f;
        sweep_grad_box->decoration = cw::BoxDecoration{
            .gradient = cw::SweepBoxGradient{
                .colors = {kGreen, kTeal, kBlue, kPurple, kGreen},
            },
            .border_radius = 12.0f,
        };
        sweep_grad_box->child = cw::mw<cw::Center>(
            cw::mw<cw::Text>("Sweep", ts(13.0f, cw::Color::white())));

        auto gradient_fills_row = cw::mw<cw::Row>(cw::MainAxisAlignment::start, cw::CrossAxisAlignment::center,
            cw::WidgetList{ linear_grad_box, hspace(16.0f), radial_grad_box, hspace(16.0f), sweep_grad_box });

        // Gradient borders — BoxBorder::gradientBorder()
        auto linear_border_box = std::make_shared<cw::Container>();
        linear_border_box->width  = 140.0f;
        linear_border_box->height = 90.0f;
        linear_border_box->decoration = cw::BoxDecoration{
            .color         = colors.surface,
            .border_radius = 16.0f,
            .border        = cw::BoxBorder::gradientBorder(
                cw::LinearBoxGradient{
                    .begin  = cw::Alignment::centerLeft(),
                    .end    = cw::Alignment::centerRight(),
                    .colors = {kRed, kAmber},
                }, 4.0f),
        };
        linear_border_box->child = cw::mw<cw::Center>(
            cw::mw<cw::Text>("Linear border", ts(12.0f, colors.on_surface)));

        auto sweep_border_box = std::make_shared<cw::Container>();
        sweep_border_box->width  = 140.0f;
        sweep_border_box->height = 90.0f;
        sweep_border_box->decoration = cw::BoxDecoration{
            .color         = colors.surface,
            .border_radius = 45.0f,
            .border        = cw::BoxBorder::gradientBorder(
                cw::SweepBoxGradient{
                    .colors = {kBlue, kGreen, kAmber, kRed, kBlue},
                }, 5.0f),
        };
        sweep_border_box->child = cw::mw<cw::Center>(
            cw::mw<cw::Text>("Sweep border", ts(12.0f, colors.on_surface)));

        // Repeated tile mode — short gradient span tiled across the border.
        auto repeated_border_box = std::make_shared<cw::Container>();
        repeated_border_box->width  = 140.0f;
        repeated_border_box->height = 90.0f;
        repeated_border_box->decoration = cw::BoxDecoration{
            .color         = colors.surface,
            .border_radius = 16.0f,
            .border        = cw::BoxBorder::gradientBorder(
                cw::LinearBoxGradient{
                    .begin     = cw::Alignment::topLeft(),
                    .end       = cw::Alignment{-0.6f, -0.6f},
                    .colors    = {kPurple, cw::Color::white()},
                    .tile_mode = cw::TileMode::repeated,
                }, 4.0f),
        };
        repeated_border_box->child = cw::mw<cw::Center>(
            cw::mw<cw::Text>("Repeated border", ts(12.0f, colors.on_surface)));

        auto gradient_borders_row = cw::mw<cw::Row>(cw::MainAxisAlignment::start, cw::CrossAxisAlignment::center,
            cw::WidgetList{ linear_border_box, hspace(16.0f), sweep_border_box, hspace(16.0f), repeated_border_box });

        // Stroke caps & joins — Paint::stroke_cap / stroke_join / stroke_miter_limit,
        // rendered via RawCustomPaint + raw Canvas calls (drawLine/drawPath/drawRect
        // with PaintStyle::stroke), exercising the GPU-side cap/join expansion in
        // src/gpu/stroke_geometry.hpp across all three backends.
        auto stroke_caps_row = cw::mw<cw::Row>(cw::MainAxisAlignment::start, cw::CrossAxisAlignment::start,
            cw::WidgetList{
                strokeSample("Butt cap",   150.0f, 90.0f, strokeCapDemo(cw::StrokeCap::butt,   kBlue),   colors.on_surface_variant),
                hspace(16.0f),
                strokeSample("Round cap",  150.0f, 90.0f, strokeCapDemo(cw::StrokeCap::round,  kGreen),  colors.on_surface_variant),
                hspace(16.0f),
                strokeSample("Square cap", 150.0f, 90.0f, strokeCapDemo(cw::StrokeCap::square, kOrange), colors.on_surface_variant),
            });

        auto stroke_joins_row = cw::mw<cw::Row>(cw::MainAxisAlignment::start, cw::CrossAxisAlignment::start,
            cw::WidgetList{
                strokeSample("Miter join", 150.0f, 110.0f, strokeJoinDemo(cw::StrokeJoin::miter, kPurple, 70.0f), colors.on_surface_variant),
                hspace(16.0f),
                strokeSample("Round join", 150.0f, 110.0f, strokeJoinDemo(cw::StrokeJoin::round, kTeal, 70.0f), colors.on_surface_variant),
                hspace(16.0f),
                strokeSample("Bevel join", 150.0f, 110.0f, strokeJoinDemo(cw::StrokeJoin::bevel, kRed, 70.0f), colors.on_surface_variant),
            });

        auto stroke_advanced_row = cw::mw<cw::Row>(cw::MainAxisAlignment::start, cw::CrossAxisAlignment::start,
            cw::WidgetList{
                strokeSample("Miter, limit 4\n(falls back to bevel)", 170.0f, 110.0f,
                    strokeJoinDemo(cw::StrokeJoin::miter, kAmber, 16.0f, 4.0f), colors.on_surface_variant),
                hspace(16.0f),
                strokeSample("Miter, limit 20\n(spike preserved)", 170.0f, 110.0f,
                    strokeJoinDemo(cw::StrokeJoin::miter, kAmber, 16.0f, 20.0f), colors.on_surface_variant),
                hspace(16.0f),
                strokeSample("Rotated stroked rect", 150.0f, 110.0f, strokeRotatedRectDemo, colors.on_surface_variant),
            });

        // Opacity row — 5 levels
        std::vector<cw::WidgetRef> opacity_boxes;
        for (int i = 1; i <= 5; ++i) {
            const float op = i * 0.2f;
            auto b = std::make_shared<cw::Container>();
            b->width = 48.0f; b->height = 48.0f; b->color = kPurple;
            auto op_widget = std::make_shared<cw::Opacity>(op, b);
            std::ostringstream o; o << std::fixed << std::setprecision(1) << op;
            opacity_boxes.push_back(cw::mw<cw::Column>(
                cw::MainAxisAlignment::start, cw::CrossAxisAlignment::center,
                cw::WidgetList{
                    op_widget,
                    vspace(4.0f),
                    cw::mw<cw::Text>(o.str(), ts(10.0f, colors.on_surface_variant)),
                }));
            if (i < 5) opacity_boxes.push_back(hspace(8.0f));
        }
        auto opacity_row = cw::mw<cw::Row>(cw::MainAxisAlignment::start, cw::CrossAxisAlignment::end,
            opacity_boxes);

        // BackdropFilter — frosted glass over striped background
        std::vector<cw::WidgetRef> stripe_cols;
        const cw::Color stripes[] = { kBlue, kGreen, kOrange, kPurple, kTeal, kRed, kAmber };
        for (int i = 0; i < 7; ++i) {
            auto stripe = std::make_shared<cw::Expanded>(std::make_shared<cw::ColoredBox>(stripes[i]));
            stripe_cols.push_back(stripe);
        }
        auto bg_stripes = cw::mw<cw::Row>(cw::MainAxisAlignment::start, cw::CrossAxisAlignment::stretch,
            stripe_cols);

        auto frost_content = std::make_shared<cw::Container>();
        frost_content->color = cw::Color::fromRGBA(1.0f, 1.0f, 1.0f, 0.25f);
        frost_content->child = cw::mw<cw::Center>(cw::mw<cw::Padding>(
            cw::EdgeInsets::all(16.0f),
            cw::mw<cw::Text>("BackdropFilter\nfrosted glass\nblur σ=10", ts(13.0f, cw::Color::white()))));

        auto bf = std::make_shared<cw::BackdropFilter>();
        bf->filter = cw::ImageFilter::blur(10.0f);
        bf->child  = frost_content;

        auto frost_pos = std::make_shared<cw::Positioned>();
        frost_pos->left = 40.0f; frost_pos->top = 20.0f; frost_pos->right = 40.0f; frost_pos->bottom = 20.0f;
        frost_pos->child = bf;

        auto blur_container = std::make_shared<cw::Container>();
        blur_container->height = 120.0f;
        auto blur_stack = std::make_shared<cw::Stack>();
        blur_stack->fit      = cw::StackFit::expand;
        blur_stack->children = { bg_stripes, frost_pos };
        blur_container->child = blur_stack;

        // --- Liquid Glass — same striped backdrop, refracted/tinted/
        // specular-rimmed instead of plain-frosted. See ImageFilter::
        // liquidGlass()'s doc comment and TODO.md for the technique.
        std::vector<cw::WidgetRef> glass_stripe_cols;
        for (int i = 0; i < 7; ++i) {
            auto stripe = std::make_shared<cw::Expanded>(std::make_shared<cw::ColoredBox>(stripes[i]));
            glass_stripe_cols.push_back(stripe);
        }
        auto glass_bg_stripes = cw::mw<cw::Row>(cw::MainAxisAlignment::start, cw::CrossAxisAlignment::stretch,
            glass_stripe_cols);

        auto glass_content = cw::mw<cw::Center>(
            cw::mw<cw::Text>("Liquid Glass", ts(14.0f, cw::Color::white())));

        auto glass_bf = std::make_shared<cw::BackdropFilter>();
        glass_bf->filter = cw::ImageFilter::liquidGlass(24.0f);
        glass_bf->child  = glass_content;

        auto glass_pos = std::make_shared<cw::Positioned>();
        glass_pos->left = 40.0f; glass_pos->top = 20.0f; glass_pos->right = 40.0f; glass_pos->bottom = 20.0f;
        glass_pos->child = glass_bf;

        auto glass_container = std::make_shared<cw::Container>();
        glass_container->height = 120.0f;
        auto glass_stack = std::make_shared<cw::Stack>();
        glass_stack->fit      = cw::StackFit::expand;
        glass_stack->children = { glass_bg_stripes, glass_pos };
        glass_container->child = glass_stack;

        auto scroll = std::make_shared<cw::SingleChildScrollView>();
        scroll->physics = std::make_shared<cw::BouncingScrollPhysics>();
        scroll->child   = cw::mw<cw::Padding>(cw::EdgeInsets::all(20.0f),
            cw::mw<cw::Column>(cw::MainAxisAlignment::start, cw::CrossAxisAlignment::start,
                cw::WidgetList{
                    subheading("CLIP RRECT + CLIP OVAL", colors.on_surface_variant),
                    card(clips_row, 16.0f, colors.surface),
                    vspace(20.0f),
                    subheading("DECORATED BOX — border + shadow", colors.on_surface_variant),
                    card(fancy_box, 0.0f, colors.surface),
                    vspace(20.0f),
                    subheading("GRADIENT FILLS — Linear / Radial / Sweep BoxGradient", colors.on_surface_variant),
                    card(gradient_fills_row, 16.0f, colors.surface),
                    vspace(20.0f),
                    subheading("GRADIENT BORDERS — BoxBorder::gradientBorder()", colors.on_surface_variant),
                    card(gradient_borders_row, 16.0f, colors.surface),
                    vspace(20.0f),
                    subheading("STROKE CAPS — Paint::stroke_cap (white ticks mark the true endpoints)", colors.on_surface_variant),
                    card(stroke_caps_row, 16.0f, colors.surface),
                    vspace(20.0f),
                    subheading("STROKE JOINS — Paint::stroke_join", colors.on_surface_variant),
                    card(stroke_joins_row, 16.0f, colors.surface),
                    vspace(20.0f),
                    subheading("STROKE MITER LIMIT + ROTATION — Paint::stroke_miter_limit, drawRect() stroke under rotation", colors.on_surface_variant),
                    card(stroke_advanced_row, 16.0f, colors.surface),
                    vspace(20.0f),
                    subheading("OPACITY — five levels 0.2 → 1.0", colors.on_surface_variant),
                    card(opacity_row, 16.0f, colors.surface),
                    vspace(20.0f),
                    subheading("BACKDROP FILTER — frosted glass", colors.on_surface_variant),
                    blur_container,
                    vspace(20.0f),
                    subheading("LIQUID GLASS — refraction + saturation + specular rim", colors.on_surface_variant),
                    glass_container,
                    vspace(20.0f),
                }));

        auto root = std::make_shared<cw::Container>();
        root->color = colors.surface_variant;
        root->child = scroll;
        return root;
    }
};

// ---------------------------------------------------------------------------
// 8. KEYBOARD — KeyboardListener + FocusNode
// ---------------------------------------------------------------------------
static std::string keyName(cw::KeyCode code)
{
    using K = cw::KeyCode;
    switch (code) {
        case K::a: return "A"; case K::b: return "B"; case K::c: return "C";
        case K::d: return "D"; case K::e: return "E"; case K::f: return "F";
        case K::g: return "G"; case K::h: return "H"; case K::i: return "I";
        case K::j: return "J"; case K::k: return "K"; case K::l: return "L";
        case K::m: return "M"; case K::n: return "N"; case K::o: return "O";
        case K::p: return "P"; case K::q: return "Q"; case K::r: return "R";
        case K::s: return "S"; case K::t: return "T"; case K::u: return "U";
        case K::v: return "V"; case K::w: return "W"; case K::x: return "X";
        case K::y: return "Y"; case K::z: return "Z";
        case K::digit_0: return "0"; case K::digit_1: return "1";
        case K::digit_2: return "2"; case K::digit_3: return "3";
        case K::digit_4: return "4"; case K::digit_5: return "5";
        case K::digit_6: return "6"; case K::digit_7: return "7";
        case K::digit_8: return "8"; case K::digit_9: return "9";
        case K::space:          return "Space";
        case K::enter:          return "Return";
        case K::tab:            return "Tab";
        case K::backspace:      return "Backspace";
        case K::escape:         return "Escape";
        case K::delete_forward: return "Delete";
        case K::left:  return "←"; case K::right: return "→";
        case K::up:    return "↑"; case K::down:  return "↓";
        case K::home:      return "Home"; case K::end:       return "End";
        case K::page_up:   return "PgUp"; case K::page_down: return "PgDn";
        case K::f1:  return "F1";  case K::f2:  return "F2";
        case K::f3:  return "F3";  case K::f4:  return "F4";
        case K::f5:  return "F5";  case K::f6:  return "F6";
        case K::f7:  return "F7";  case K::f8:  return "F8";
        case K::f9:  return "F9";  case K::f10: return "F10";
        case K::f11: return "F11"; case K::f12: return "F12";
        case K::left_shift:  case K::right_shift: return "Shift";
        case K::left_ctrl:   case K::right_ctrl:  return "Ctrl";
        case K::left_alt:    case K::right_alt:   return "Option/Alt";
        case K::left_meta:   case K::right_meta:  return "Cmd/Win";
        case K::caps_lock: return "CapsLock";
        default: return "?";
    }
}

static std::string kindStr(cw::KeyEventKind k)
{
    switch (k) {
        case cw::KeyEventKind::down:   return "down";
        case cw::KeyEventKind::up:     return "up";
        case cw::KeyEventKind::repeat: return "repeat";
    }
    return "";
}

class KeyboardSection;

class KeyboardSectionState : public cw::State<KeyboardSection>
{
public:
    void initState() override
    { node_ = std::make_shared<cw::FocusNode>(); }

    cw::WidgetRef build(cw::BuildContext& ctx) override
    {
        const auto* ds     = cw::Theme::of(ctx);
        const auto& colors = ds->tokens().colors;

        const std::string key_display = last_key_.empty() ? "press a key" : last_key_;
        const cw::Color key_color = last_kind_ == cw::KeyEventKind::up
            ? colors.on_surface_variant
            : colors.primary;

        const std::string typed_display = typed_.empty() ? "typed text appears here" : typed_;
        const cw::Color typed_color = typed_.empty()
            ? colors.on_surface_variant
            : colors.on_surface;

        std::vector<cw::WidgetRef> log_items;
        for (auto it = log_.rbegin(); it != log_.rend(); ++it) {
            log_items.push_back(cw::mw<cw::Text>(*it,
                ts(11.0f, colors.on_surface_variant)));
        }
        auto log_col = std::make_shared<cw::Column>();
        log_col->main_axis_alignment  = cw::MainAxisAlignment::start;
        log_col->cross_axis_alignment = cw::CrossAxisAlignment::start;
        log_col->main_axis_size       = cw::MainAxisSize::min;
        log_col->children             = log_items;

        auto center_content = cw::mw<cw::Column>(
            cw::MainAxisAlignment::center, cw::CrossAxisAlignment::center,
            cw::WidgetList{
                cw::mw<cw::Text>("last key event", ts(12.0f, colors.on_surface_variant)),
                vspace(8.0f),
                cw::mw<cw::Text>(key_display, ts(64.0f, key_color)),
                vspace(4.0f),
                cw::mw<cw::Text>(last_key_.empty() ? "" : kindStr(last_kind_),
                    ts(12.0f, colors.on_surface_variant)),
                vspace(32.0f),
                cw::mw<cw::Text>("typed", ts(12.0f, colors.on_surface_variant)),
                vspace(8.0f),
                cw::mw<cw::Text>(typed_display, ts(20.0f, typed_color)),
            });

        auto root = cw::mw<cw::Column>(
            cw::MainAxisAlignment::start, cw::CrossAxisAlignment::stretch,
            cw::WidgetList{
                cw::mw<cw::Expanded>(cw::mw<cw::Center>(center_content)),
                cw::mw<cw::Padding>(cw::EdgeInsets::all(20.0f), log_col),
            });

        auto bg = std::make_shared<cw::Container>();
        bg->color = colors.surface_variant;
        bg->child = root;

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
            last_key_  = keyName(e.key_code);
            last_kind_ = e.kind;

            const std::string entry = keyName(e.key_code) + "  [" + kindStr(e.kind) + "]";
            log_.push_back(entry);
            if (log_.size() > 5) log_.erase(log_.begin());

            if (e.kind != cw::KeyEventKind::up && e.character != 0) {
                if (e.key_code == cw::KeyCode::backspace) {
                    if (!typed_.empty()) typed_.pop_back();
                } else if (e.character >= 0x20 && e.character < 0x7F) {
                    typed_ += static_cast<char>(e.character);
                }
            }
        });
    }

    std::shared_ptr<cw::FocusNode> node_;
    std::string             last_key_;
    cw::KeyEventKind        last_kind_  = cw::KeyEventKind::down;
    std::string             typed_;
    std::vector<std::string> log_;
};

class KeyboardSection : public cw::StatefulWidget {
public:
    std::unique_ptr<cw::StateBase> createState() const override
    { return std::make_unique<KeyboardSectionState>(); }
};

// ---------------------------------------------------------------------------
// 9. IMAGES — ImageWidget, BoxFit, decorations, transforms, blur
// ---------------------------------------------------------------------------

// Mount Robson, CC0 1.0 Universal Public Domain Dedication — see
// assets/mountains_jpeg.h for provenance. Every call decodes against the
// same cache key (MemoryImage::cacheKey() hashes the bytes), so repeated
// calls below don't each re-decode the JPEG.
static cw::WidgetRef mountainsImage(
    cw::BoxFit fit, std::optional<float> w = std::nullopt, std::optional<float> h = std::nullopt)
{
    return cw::ImageWidget::memory(
        std::vector<uint8_t>(
            cw::gallery_assets::kMountainsJpeg,
            cw::gallery_assets::kMountainsJpeg + cw::gallery_assets::kMountainsJpegSize),
        fit, w, h);
}

// Square container — lets every BoxFit mode be checked against the same
// width and height, which is the case that actually distinguishes them
// (fitWidth vs. fitHeight are indistinguishable on a non-square box).
static constexpr float kFitSampleBoxSize = 120.0f;

static cw::WidgetRef fitSample(const std::string& label, cw::BoxFit fit,
    cw::Color box_bg, cw::Color label_color)
{
    auto box = std::make_shared<cw::Container>();
    box->width  = kFitSampleBoxSize;
    box->height = kFitSampleBoxSize;
    box->color  = box_bg;
    box->child  = mountainsImage(fit, kFitSampleBoxSize, kFitSampleBoxSize);
    auto clipped = std::make_shared<cw::ClipRRect>(8.0f, box);

    return cw::mw<cw::Column>(cw::MainAxisAlignment::start, cw::CrossAxisAlignment::center,
        cw::WidgetList{
            clipped,
            vspace(6.0f),
            cw::mw<cw::Text>(label, ts(11.0f, label_color)),
        });
}

// ---------------------------------------------------------------------------
// RotatingTransformRow — isolated in its own small StatefulWidget so its
// per-frame animation only rebuilds these four images, not the rest of the
// Images tab (which was the cause of the ~40ms/frame UI time: everything —
// all 7 BoxFit samples, all 3 decoration samples, the blur backdrop — was
// rebuilding on every animation tick before this was split out).
//
// X/Y rotation is now genuine 3D perspective rotation, not the earlier
// scale-based flip approximation: the Metal backend was rewritten (see
// TODO.md's "Real per-vertex quad rendering" entry) to build every quad
// from real, independently-transformed corners and emit a true clip-space
// w the GPU hardware perspective-divides, instead of collapsing corners to
// an axis-aligned dstRect with a hardcoded w=1. `perspectiveRotation()`
// below mirrors Flutter's well-known `Matrix4.identity()..setEntry(3,2,
// small)..rotateX(angle)` trick — this renderer's Matrix4 uses the same
// row-major, column-vector convention, so the same recipe applies
// directly: `data[14]` is row 3, column 2 (`data[r*4+c]`), and injecting a
// small value there before composing the rotation makes the GPU's
// perspective divide produce real foreshortening once the rotated point's
// Z becomes non-zero.
// ---------------------------------------------------------------------------
class RotatingTransformRow;

static cw::Matrix4 perspectiveRotationX(float angle)
{
    cw::Matrix4 m = cw::Matrix4::identity();
    m.data[14] = -0.0025f;
    return m * cw::Matrix4::rotateX(angle);
}

static cw::Matrix4 perspectiveRotationY(float angle)
{
    cw::Matrix4 m = cw::Matrix4::identity();
    m.data[14] = -0.0025f;
    return m * cw::Matrix4::rotateY(angle);
}

static cw::WidgetRef labeledTransform(const char* label, cw::WidgetRef image, cw::Matrix4 transform,
    cw::Color label_color)
{
    auto t = std::make_shared<cw::Transform>();
    t->transform = transform;
    t->child = std::make_shared<cw::ClipRRect>(8.0f, std::move(image));

    return cw::mw<cw::Column>(cw::MainAxisAlignment::start, cw::CrossAxisAlignment::center,
        cw::WidgetList{
            t, vspace(6.0f),
            cw::mw<cw::Text>(label, ts(11.0f, label_color)),
        });
}

class RotatingTransformRowState : public cw::State<RotatingTransformRow>
{
public:
    void initState() override
    {
        anim_ctrl_ = std::make_unique<cw::AnimationController>(6000.0, 0.0, 6.283185307);
        listener_id_ = anim_ctrl_->addListener([this]() {
            if (anim_ctrl_->status() == cw::AnimationStatus::completed)
                anim_ctrl_->forward(0.0);
            setState([] {});
        });
        anim_ctrl_->forward();

        // The image content never changes between ticks — only the
        // Transform's rotation angle does. Building a fresh ImageWidget
        // every tick (60x/sec) forced a ~200KB byte-vector copy plus a
        // full StatefulElement rebuild cascade every single frame, purely
        // as a side effect of reconstructing a widget whose content was
        // always identical anyway. Constructing these once here and
        // reusing the *same* WidgetRef in build() lets Element::updateChild()'s
        // identical-pointer fast path (see element.cpp) skip that whole
        // subtree's reconciliation on every tick where only the ancestor
        // Transform actually changed.
        image_x_     = mountainsImage(cw::BoxFit::cover, 110.0f, 80.0f);
        image_y_     = mountainsImage(cw::BoxFit::cover, 110.0f, 80.0f);
        image_z_     = mountainsImage(cw::BoxFit::cover, 110.0f, 80.0f);
        image_scale_ = mountainsImage(cw::BoxFit::cover, 110.0f, 80.0f);
    }

    void dispose() override
    {
        if (anim_ctrl_) anim_ctrl_->removeListener(listener_id_);
    }

    cw::WidgetRef build(cw::BuildContext& ctx) override
    {
        const auto* ds     = cw::Theme::of(ctx);
        const auto& colors = ds->tokens().colors;
        const float angle = static_cast<float>(anim_ctrl_->value());

        return cw::mw<cw::Row>(cw::MainAxisAlignment::start, cw::CrossAxisAlignment::center,
            cw::WidgetList{
                labeledTransform("rotate — X axis", image_x_,
                    perspectiveRotationX(angle), colors.on_surface_variant),
                hspace(24.0f),
                labeledTransform("rotate — Y axis", image_y_,
                    perspectiveRotationY(angle + 1.2f), colors.on_surface_variant),
                hspace(24.0f),
                labeledTransform("rotate — Z axis", image_z_,
                    cw::RenderTransform::rotation(angle), colors.on_surface_variant),
                hspace(24.0f),
                labeledTransform("scale", image_scale_,
                    cw::RenderTransform::scaling(0.65f + 0.15f * std::sin(angle)), colors.on_surface_variant),
            });
    }

private:
    cw::WidgetRef image_x_;
    cw::WidgetRef image_y_;
    cw::WidgetRef image_z_;
    cw::WidgetRef image_scale_;
    std::unique_ptr<cw::AnimationController> anim_ctrl_;
    uint64_t                                 listener_id_ = 0;
};

class RotatingTransformRow : public cw::StatefulWidget {
public:
    std::unique_ptr<cw::StateBase> createState() const override
    { return std::make_unique<RotatingTransformRowState>(); }
};

class ImagesSection : public cw::StatelessWidget
{
public:
    cw::WidgetRef build(cw::BuildContext& ctx) const override
    {
        const auto* ds     = cw::Theme::of(ctx);
        const auto& colors = ds->tokens().colors;

        // BoxFit — same source image, same square box, every fit mode.
        // A horizontal ListView (rather than a Row) so the sample strip
        // scrolls if the window is too narrow to show all seven at once.
        static const std::vector<std::pair<std::string, cw::BoxFit>> kFitModes{
            {"fill", cw::BoxFit::fill},
            {"contain", cw::BoxFit::contain},
            {"cover", cw::BoxFit::cover},
            {"fitWidth", cw::BoxFit::fitWidth},
            {"fitHeight", cw::BoxFit::fitHeight},
            {"none", cw::BoxFit::none},
            {"scaleDown", cw::BoxFit::scaleDown},
        };
        auto fit_list_view = std::make_shared<cw::ListView>();
        fit_list_view->scroll_axis = cw::Axis::horizontal;
        fit_list_view->item_count  = static_cast<int>(kFitModes.size());
        fit_list_view->item_extent = kFitSampleBoxSize + 16.0f;
        fit_list_view->builder = [colors](cw::BuildContext&, int i) -> cw::WidgetRef {
            const auto& [label, fit] = kFitModes[static_cast<size_t>(i)];
            return cw::mw<cw::Padding>(cw::EdgeInsets::only(0.0f, 0.0f, 16.0f, 0.0f),
                fitSample(label, fit, colors.surface_variant, colors.on_surface_variant));
        };
        auto fit_list = cw::SizedBox::from_height(kFitSampleBoxSize + 40.0f, fit_list_view);

        // Decorations — ClipRRect, ClipOval, border + shadow.
        auto rrect_img = std::make_shared<cw::ClipRRect>(
            16.0f, mountainsImage(cw::BoxFit::cover, 140.0f, 100.0f));

        auto oval_img = std::make_shared<cw::ClipOval>(
            mountainsImage(cw::BoxFit::cover, 100.0f, 100.0f));

        cw::BoxDecoration framed_deco;
        framed_deco.border_radius = 10.0f;
        framed_deco.border        = cw::BoxBorder::all(kBlue, 3.0f);
        framed_deco.box_shadow    = {
            cw::BoxShadow{cw::Color::fromRGBA(0.0f, 0.0f, 0.0f, 0.20f), {0.0f, 6.0f}, 14.0f, 0.0f}};
        auto framed_box = std::make_shared<cw::DecoratedBox>(framed_deco);
        framed_box->child = std::make_shared<cw::ClipRRect>(
            10.0f, mountainsImage(cw::BoxFit::cover, 140.0f, 100.0f));

        auto deco_row = cw::mw<cw::Row>(cw::MainAxisAlignment::start, cw::CrossAxisAlignment::center,
            cw::WidgetList{
                rrect_img, hspace(20.0f),
                oval_img, hspace(20.0f),
                framed_box,
            });

        // Transform — isolated in its own StatefulWidget; see
        // RotatingTransformRow's doc comment for why.
        auto transform_row = std::make_shared<RotatingTransformRow>();

        // BackdropFilter — blur the photo itself, frosted-glass panel on top.
        auto frost_content = std::make_shared<cw::Container>();
        frost_content->color = cw::Color::fromRGBA(1.0f, 1.0f, 1.0f, 0.25f);
        frost_content->child = cw::mw<cw::Center>(cw::mw<cw::Padding>(
            cw::EdgeInsets::all(16.0f),
            cw::mw<cw::Text>("BackdropFilter\nblur σ=10 over the photo",
                ts(14.0f, cw::Color::white()))));

        auto bf = std::make_shared<cw::BackdropFilter>();
        bf->filter = cw::ImageFilter::blur(10.0f);
        bf->child  = frost_content;

        auto frost_pos = std::make_shared<cw::Positioned>();
        frost_pos->left = 40.0f; frost_pos->top = 20.0f;
        frost_pos->right = 40.0f; frost_pos->bottom = 20.0f;
        frost_pos->child = bf;

        auto blur_container = std::make_shared<cw::Container>();
        blur_container->height = 180.0f;
        auto blur_stack = std::make_shared<cw::Stack>();
        blur_stack->fit = cw::StackFit::expand;
        blur_stack->children = {
            cw::Positioned::fill(mountainsImage(cw::BoxFit::cover)),
            frost_pos,
        };
        blur_container->child = blur_stack;

        auto scroll = std::make_shared<cw::SingleChildScrollView>();
        scroll->physics = std::make_shared<cw::BouncingScrollPhysics>();
        scroll->child = cw::mw<cw::Padding>(cw::EdgeInsets::all(20.0f),
            cw::mw<cw::Column>(cw::MainAxisAlignment::start, cw::CrossAxisAlignment::start,
                cw::WidgetList{
                    subheading("BOX FIT — fill · contain · cover · fitWidth · fitHeight · none · scaleDown", colors.on_surface_variant),
                    repaintBoundary(card(fit_list, 16.0f, colors.surface)),
                    vspace(20.0f),
                    subheading("DECORATIONS — ClipRRect · ClipOval · border + shadow", colors.on_surface_variant),
                    repaintBoundary(card(deco_row, 16.0f, colors.surface)),
                    vspace(20.0f),
                    subheading("TRANSFORM — animated flip (X/Y) · rotation (Z) · scale", colors.on_surface_variant),
                    repaintBoundary(card(transform_row, 16.0f, colors.surface)),
                    vspace(20.0f),
                    subheading("BACKDROP FILTER — blur the photo itself", colors.on_surface_variant),
                    repaintBoundary(blur_container),
                    vspace(20.0f),
                }));

        auto root = std::make_shared<cw::Container>();
        root->color = colors.surface_variant;
        root->child = scroll;
        return root;
    }
};

// ---------------------------------------------------------------------------
// 10. DRAW — freehand canvas backed by a persistent GPU texture
//
// Strokes are stamped-circle segments accumulated incrementally into the
// surface's own dedicated texture (RenderDrawSurface), rather than
// replaying the whole stroke history every frame — see
// inc/campello_widgets/ui/render_draw_surface.hpp's doc comment. Pressure
// (from a stylus, where available) modulates stroke width; mouse/finger
// input reports a constant pressure of 1.0, so this degrades gracefully to
// a fixed-width pen on macOS.
// ---------------------------------------------------------------------------
class DrawSection;

class DrawSectionState : public cw::State<DrawSection>
{
public:
    void initState() override
    {
        draw_key_ = std::make_shared<cw::GlobalKey>();
    }

    cw::WidgetRef build(cw::BuildContext& ctx) override
    {
        const auto* ds     = cw::Theme::of(ctx);
        const auto& colors = ds->tokens().colors;

        // The canvas itself deliberately stays paper-white regardless of
        // theme/dark mode — like a real drawing app, the drawing surface is
        // its own fixed "paper" rather than following the surrounding UI.
        auto surface = std::make_shared<cw::DrawSurface>();
        surface->stroke_color      = cw::Color::fromRGB(0.15f, 0.15f, 0.18f);
        surface->background_color  = cw::Color::white();
        surface->stroke_width      = 5.0f;
        surface->key               = draw_key_;

        cw::BoxDecoration clear_deco;
        clear_deco.color         = colors.surface_variant;
        clear_deco.border_radius = 6.0f;
        auto clear_box = std::make_shared<cw::DecoratedBox>(clear_deco);
        clear_box->child = cw::mw<cw::Padding>(cw::EdgeInsets::symmetric(16.0f, 8.0f),
            cw::mw<cw::Text>("Clear", ts(13.0f, colors.on_surface_variant)));
        auto clear_tap = std::make_shared<cw::GestureDetector>();
        clear_tap->on_tap = [this] { clearSurface(); };
        clear_tap->child  = clear_box;

        auto toolbar = cw::mw<cw::Row>(cw::MainAxisAlignment::spaceBetween, cw::CrossAxisAlignment::center,
            cw::WidgetList{
                cw::mw<cw::Text>("Draw", ts(20.0f, colors.on_surface)),
                ptr(clear_tap),
            });

        auto canvas_area = card(repaintBoundary(surface), 0.0f, colors.surface);

        auto col = cw::mw<cw::Column>(cw::MainAxisAlignment::start, cw::CrossAxisAlignment::stretch,
            cw::WidgetList{
                toolbar,
                vspace(12.0f),
                cw::mw<cw::Expanded>(canvas_area),
            });

        return cw::mw<cw::Padding>(cw::EdgeInsets::all(24.0f), col);
    }

private:
    void clearSurface()
    {
        if (auto* el = draw_key_->currentElement())
            if (auto* roe = dynamic_cast<cw::RenderObjectElement*>(el))
                if (auto* ro = roe->renderObject())
                    static_cast<cw::RenderDrawSurface*>(ro)->clear();
    }

    std::shared_ptr<cw::GlobalKey> draw_key_;
};

class DrawSection : public cw::StatefulWidget {
public:
    std::unique_ptr<cw::StateBase> createState() const override
    { return std::make_unique<DrawSectionState>(); }
};

// ---------------------------------------------------------------------------
// 11. VIDEO — VideoPlayerController/VideoPlayer, macOS/AVFoundation first
// slice (see TODO.md's Video playback entry). Its own top-level tab rather
// than a Controls subsection — moved out at the user's request once the
// demo proved out.
// ---------------------------------------------------------------------------
class VideoSection;

class VideoSectionState : public cw::State<VideoSection>
{
public:
    void initState() override
    {
        video_ctrl_ = std::make_shared<cw::VideoPlayerController>();
        video_ctrl_->setSource(sampleVideoPath());
        // Position/duration/ready/play-state all flow through this one
        // listener — matches AnimationController's addListener()
        // convention elsewhere in this file. Fires on the main thread
        // (VideoPlayerController's ticker subscription runs there), so
        // setState() here is safe without any cross-thread marshaling.
        video_ctrl_->addListener([this] { setState([] {}); });
    }

    cw::WidgetRef build(cw::BuildContext& ctx) override
    {
        const auto* ds     = cw::Theme::of(ctx);
        const auto& colors = ds->tokens().colors;

        // CPU-decode + CPU-copy-per-frame, not zero-copy; audio isn't
        // exercised by this specific demo clip (a synthetic, silent test
        // pattern — see examples/gallery/assets/sample_video.mp4), though
        // AVPlayer would play it automatically if the source had an audio
        // track.
        cw::ButtonConfig play_cfg;
        const bool playing = video_ctrl_->isPlaying();
        play_cfg.label      = cw::mw<cw::Text>(playing ? "Pause" : "Play",
                                                 ts(14.0f, colors.on_primary));
        play_cfg.priority   = cw::ButtonPriority::primary;
        play_cfg.on_pressed = [this, playing] {
            if (playing) video_ctrl_->pause(); else video_ctrl_->play();
        };
        auto play_btn = ds->buildButton(play_cfg);

        std::ostringstream pos_stream;
        pos_stream << std::fixed << std::setprecision(1)
                    << (video_ctrl_->positionMs() / 1000.0) << "s / "
                    << (video_ctrl_->durationMs() / 1000.0) << "s"
                    << (video_ctrl_->isReady() ? "" : "  (loading…)");
        auto pos_label = cw::mw<cw::Text>(pos_stream.str(), ts(13.0f, colors.on_surface_variant));

        auto controls_row = cw::mw<cw::Row>(
            cw::MainAxisAlignment::start, cw::CrossAxisAlignment::center,
            cw::WidgetList{ play_btn, hspace(12.0f), pos_label });

        // BoxFit::contain — letterboxed, full frame always visible,
        // preserving the video's own aspect ratio. BoxFit::cover (crop to
        // fill) was tried first for a full-bleed look, but that's the
        // wrong tradeoff for a player: normal video-player behavior is to
        // fit the whole frame inside the available area, not crop into it
        // — especially visible on a portrait phone screen showing a
        // landscape (16:9) clip, where cover crops away most of the frame.
        // The surrounding black Container is what actually fills the tab;
        // the video itself letterboxes within it.
        auto surface = std::make_shared<cw::Container>();
        surface->color = cw::Color::fromRGB(0.0f, 0.0f, 0.0f);
        surface->child = cw::VideoPlayer::create(video_ctrl_, cw::BoxFit::contain);

        // The controls bar goes through ds->buildCard() (elevated —
        // Liquid Glass in Glass mode, same as the Controls tab's own Card
        // demo) rather than a plain Container, specifically so this tab
        // doubles as a glass-over-real-content check: the striped test
        // pattern in Clipping & FX is static, but a playing video behind
        // the glass panel is genuinely moving, busy content — a much
        // better way to see the refraction/blur actually working across
        // every theme, not just Glass.
        cw::CardConfig control_card_cfg;
        control_card_cfg.child   = controls_row;
        control_card_cfg.padding = cw::EdgeInsets::symmetric(20.0f, 14.0f);
        auto control_card = ds->buildCard(control_card_cfg);

        auto overlay = cw::mw<cw::Padding>(cw::EdgeInsets::only(20.0f, 0.0f, 20.0f, 20.0f),
            std::make_shared<cw::Align>(cw::Alignment::bottomCenter(), control_card));

        auto stack = cw::Stack::create(cw::WidgetList{ surface, overlay });
        stack->fit = cw::StackFit::expand;
        return stack;
    }

private:
    std::shared_ptr<cw::VideoPlayerController> video_ctrl_;
};

class VideoSection : public cw::StatefulWidget {
public:
    std::unique_ptr<cw::StateBase> createState() const override
    { return std::make_unique<VideoSectionState>(); }
};

// ---------------------------------------------------------------------------
// Gallery Shell — left sidebar nav + section content
// ---------------------------------------------------------------------------
static const std::vector<std::string> kSectionNames = {
    "Layout", "Controls", "Text & Input", "Lists",
    "Animations", "Gestures", "Clipping & FX", "Keyboard", "Images", "Draw",
    "Video",
};

// One glyph per section, shown alone when the sidebar collapses to
// icon-only width. Plain Unicode symbols rather than a real icon font —
// this framework has no Icon widget yet.
static const std::vector<std::string> kSectionIcons = {
    "▦", "⚙", "Aa", "☰",
    "▶", "✋", "✂", "⌨", "\U0001F5BC", "✏",
    "\U0001F3AC",
};

// Below this total window width the sidebar collapses to icon-only.
static constexpr float kSidebarCollapseBreakpoint = 640.0f;
static constexpr float kSidebarExpandedWidth      = 200.0f;
static constexpr float kSidebarCollapsedWidth     = 64.0f;

static cw::WidgetRef buildSection(int idx)
{
    switch (idx) {
        case 0: return std::make_shared<LayoutSection>();
        case 1: return std::make_shared<ControlsSection>();
        case 2: return std::make_shared<TextSection>();
        case 3: return std::make_shared<ListsSection>();
        case 4: return std::make_shared<AnimationsSection>();
        case 5: return std::make_shared<GesturesSection>();
        case 6: return std::make_shared<ClippingSection>();
        case 7: return std::make_shared<KeyboardSection>();
        case 8: return std::make_shared<ImagesSection>();
        case 9: return std::make_shared<DrawSection>();
        case 10: return std::make_shared<VideoSection>();
        default: return std::make_shared<LayoutSection>();
    }
}

// Bridges the PlatformMenuBar's "View" menu (built once, outside the
// widget tree, in buildGalleryApp()) to GalleryShellState's private
// selected_ tab index — the same controller-object pattern this framework
// already uses for TextEditingController / ScrollController / etc.
class GalleryNavController
{
public:
    std::function<void(int)> on_navigate;

    void navigateTo(int index)
    {
        if (on_navigate) on_navigate(index);
    }
};

// Defined ahead of GalleryShellState (rather than forward-declared, as the
// other Section/SectionState pairs in this file do) because
// GalleryShellState::initState()/dispose() read widget().controller, which
// needs GalleryShell complete at that point. createState()'s body is
// defined out-of-line after GalleryShellState instead, to break the
// resulting mutual dependency the other way around.
class GalleryShell : public cw::StatefulWidget {
public:
    std::shared_ptr<GalleryNavController> controller;

    std::unique_ptr<cw::StateBase> createState() const override;
};

class GalleryShellState : public cw::State<GalleryShell>
{
public:
    void initState() override
    {
        selected_ = 0;
        ds_ = makeGalleryDesignSystem(kind_, /*dark=*/false);
        if (auto& controller = widget().controller) {
            controller->on_navigate = [this](int i) {
                setState([this, i] { selected_ = i; });
            };
        }
    }

    void dispose() override
    {
        if (auto& controller = widget().controller)
            controller->on_navigate = nullptr;
    }

    void toggleBrightness()
    {
        setState([this] {
            const bool is_dark = (ds_->tokens().brightness == cw::Brightness::dark);
            ds_ = makeGalleryDesignSystem(kind_, !is_dark);
        });
    }

    void setDesignSystemKind(int index)
    {
        setState([this, index] {
            kind_ = static_cast<GalleryDesignSystemKind>(index);
            const bool is_dark = (ds_->tokens().brightness == cw::Brightness::dark);
            ds_ = makeGalleryDesignSystem(kind_, is_dark);
        });
    }

    cw::WidgetRef build(cw::BuildContext&) override
    {
        const auto& colors = ds_->tokens().colors;

        // Right content stays identical regardless of sidebar mode.
        auto content = cw::mw<cw::Expanded>(buildSection(selected_));

        // Vertical divider
        auto divider = std::make_shared<cw::Container>();
        divider->width = 1.0f;
        divider->color = colors.outline_variant;

        // The sidebar's own width is fixed (200 or 64), so it can't see the
        // window shrinking on its own — a LayoutBuilder around the whole
        // row is what actually observes the available width and decides
        // which sidebar mode to build.
        auto lb = std::make_shared<cw::LayoutBuilder>(
            [this, divider, content, colors](cw::BuildContext&, cw::BoxConstraints c) -> cw::WidgetRef {
                const bool collapsed = c.max_width < kSidebarCollapseBreakpoint;
                auto root = cw::mw<cw::Row>(
                    cw::MainAxisAlignment::start, cw::CrossAxisAlignment::stretch,
                    cw::WidgetList{ buildSidebar(collapsed), divider, content });
                // PlatformMenuBarView is a no-op on macOS/Windows (the
                // PlatformMenuBar ancestor already drives a native menu bar
                // there) and renders the real in-window menu bar on Linux —
                // see platform_menu_bar_view.hpp.
                auto with_menu_bar = cw::mw<cw::Column>(
                    cw::MainAxisAlignment::start, cw::CrossAxisAlignment::stretch,
                    cw::WidgetList{ cw::PlatformMenuBarView::create(), cw::mw<cw::Expanded>(root) });
                auto bg = std::make_shared<cw::Container>();
                bg->color = colors.surface_variant;
                bg->child = with_menu_bar;
                return bg;
            });
        return cw::mw<cw::Theme>(ds_, lb);
    }

private:
    // Short labels ("UI"/"MD3"/"iOS" rather than full names) so the
    // 3-way switcher fits the 200px expanded sidebar width.
    cw::WidgetRef buildThemeFooter(bool collapsed)
    {
        const auto& colors  = ds_->tokens().colors;
        const bool is_dark = (ds_->tokens().brightness == cw::Brightness::dark);

        cw::IconButtonConfig toggle_cfg;
        toggle_cfg.icon = cw::mw<cw::Text>(is_dark ? "☀" : "◐", ts(14.0f, colors.on_surface));
        toggle_cfg.on_pressed = [this] { toggleBrightness(); };
        auto toggle = ds_->buildIconButton(toggle_cfg);

        cw::WidgetRef row_content;
        if (collapsed) {
            // No room for the 3-way switcher at icon-only width — just the
            // light/dark toggle survives collapsing.
            row_content = cw::mw<cw::Row>(
                cw::MainAxisAlignment::center, cw::CrossAxisAlignment::center,
                cw::WidgetList{ toggle });
        } else {
            // buildSegmentedButton() places these labels as-is without
            // recoloring them, so — same convention as every other
            // caller-supplied label in this file — the color must be
            // chosen here. Each design system fills its selected segment
            // with a different role (Campello: primary, Material:
            // secondary_container, Cupertino: surface as a floating
            // pill), and unselected segments show through to a different
            // background too (Campello/Cupertino: surface_variant track;
            // Material: no track fill at all, shows the sidebar's own
            // surface). A single fixed color (the previous bug: an
            // unconditional black) can't contrast against all of that —
            // pick the correct on-X role per active kind_ instead.
            cw::Color selected_fg, unselected_fg;
            switch (kind_) {
                case GalleryDesignSystemKind::material:
                    selected_fg   = colors.on_secondary_container;
                    unselected_fg = colors.on_surface;
                    break;
                case GalleryDesignSystemKind::cupertino:
                case GalleryDesignSystemKind::cupertino_glass:
                    // buildSegmentedButton() itself isn't glass-wired yet
                    // (see cupertino_design_system.hpp's CupertinoMaterial
                    // doc comment — only buildCard()/buildPrimaryActionButton()
                    // are so far), so cupertino_glass renders this control
                    // identically to plain cupertino.
                    selected_fg   = colors.on_surface;
                    unselected_fg = colors.on_surface_variant;
                    break;
                case GalleryDesignSystemKind::campello_ui:
                default:
                    selected_fg   = colors.on_primary;
                    unselected_fg = colors.on_surface_variant;
                    break;
            }
            auto segLabel = [&](const char* text, int idx) {
                const bool selected = idx == static_cast<int>(kind_);
                return cw::mw<cw::Text>(text, ts(13.0f, selected ? selected_fg : unselected_fg));
            };

            cw::SegmentedConfig seg_cfg;
            seg_cfg.segments = {
                {segLabel("UI", 0),    nullptr},
                {segLabel("MD3", 1),   nullptr},
                {segLabel("iOS", 2),   nullptr},
                {segLabel("Glass", 3), nullptr},
            };
            seg_cfg.selected_index = static_cast<int>(kind_);
            seg_cfg.on_changed     = [this](int i) { setDesignSystemKind(i); };
            auto switcher = ds_->buildSegmentedButton(seg_cfg);

            row_content = cw::mw<cw::Row>(
                cw::MainAxisAlignment::start, cw::CrossAxisAlignment::center,
                cw::WidgetList{ cw::mw<cw::Expanded>(switcher), hspace(8.0f), toggle });
        }

        auto footer_divider = std::make_shared<cw::Container>();
        footer_divider->height = 1.0f;
        footer_divider->color  = colors.outline_variant;

        auto padded = std::make_shared<cw::Padding>();
        padded->padding = collapsed
            ? cw::EdgeInsets::symmetric(8.0f, 10.0f)
            : cw::EdgeInsets::symmetric(12.0f, 12.0f);
        padded->child = row_content;

        // MainAxisSize::min is load-bearing here: this Column sits as a
        // plain (non-Expanded) sibling of the Expanded nav-items column in
        // buildSidebar()'s nav_col. Left at Column's default (max), it
        // greedily claims nearly all available sidebar height in
        // RenderFlex's first (non-flex) layout pass — starving the
        // Expanded nav list, which only sees leftover space in the second
        // pass — and the switcher visibly stretched to fill the whole
        // sidebar with the nav items squeezed to nothing.
        return cw::mw<cw::Column>(
            cw::MainAxisAlignment::start, cw::CrossAxisAlignment::stretch, cw::MainAxisSize::min,
            cw::WidgetList{ footer_divider, padded });
    }

    // One tab: icon always shown; label shown alongside it only when not
    // collapsed. Active tab keeps its left accent bar in both modes.
    cw::WidgetRef buildNavItem(int i, bool collapsed)
    {
        const auto& colors = ds_->tokens().colors;
        const bool active  = (i == selected_);
        const bool hovered = (i == hovered_ || i == keyboard_focused_) && !active;
        const cw::Color fg = active ? colors.primary : colors.on_surface_variant;
        auto icon = cw::mw<cw::Text>(kSectionIcons[i], ts(16.0f, fg));

        cw::WidgetRef content;
        if (collapsed) {
            // Not Center: it expands to fill unbounded main-axis space
            // inside the nav Column instead of shrink-wrapping the icon,
            // which swallowed the whole sidebar height for this one item.
            // A Row with centered content sizes to its own content like
            // the expanded branch below already does correctly.
            content = cw::mw<cw::Row>(
                cw::MainAxisAlignment::center, cw::CrossAxisAlignment::center,
                cw::WidgetList{ icon });
        } else {
            auto label = cw::mw<cw::Text>(kSectionNames[i], ts(14.0f, fg));
            content = cw::mw<cw::Row>(
                cw::MainAxisAlignment::start, cw::CrossAxisAlignment::center,
                cw::WidgetList{ icon, hspace(12.0f), label });
        }

        auto item_container = std::make_shared<cw::Container>();
        item_container->padding = collapsed
            ? cw::EdgeInsets::symmetric(8.0f, 13.0f)
            : cw::EdgeInsets::symmetric(16.0f, 11.0f);
        item_container->color = active
            ? colors.primary_container
            : hovered
                ? colors.surface_variant
                : colors.surface;
        item_container->child = content;

        cw::WidgetRef item_bg = item_container;
        if (active) {
            // Left accent bar as a Row sibling. Use CrossAxisAlignment::center
            // (not stretch) so the Row sizes to content height instead of
            // consuming all remaining column space.
            auto accent = std::make_shared<cw::Container>();
            accent->width = 3.0f; accent->height = 36.0f; accent->color = colors.primary;
            item_container->padding = collapsed
                ? cw::EdgeInsets::only(5.0f, 13.0f, 8.0f, 13.0f)
                : cw::EdgeInsets::only(13.0f, 11.0f, 16.0f, 11.0f);
            auto row = cw::mw<cw::Row>(
                cw::MainAxisAlignment::start, cw::CrossAxisAlignment::center,
                cw::WidgetList{ accent, cw::mw<cw::Expanded>(item_container) });
            item_bg = row;
        }

        auto g   = std::make_shared<cw::GestureDetector>();
        g->on_tap     = [this, i] { setState([this, i] { selected_ = i; }); };
        g->focusable  = true; // D-pad/Tab reachable, matches the content pills
        // Reuses the hover highlight so keyboard/D-pad focus is visible
        // even though this hand-built item has no design-system focus ring.
        g->on_focus_change = [this, i](bool has_focus) {
            setState([this, i, has_focus] {
                if (has_focus) keyboard_focused_ = i;
                else if (keyboard_focused_ == i) keyboard_focused_ = -1;
            });
        };
        g->child      = item_bg;

        auto region = std::make_shared<cw::MouseRegion>();
        region->cursor   = cw::SystemMouseCursor::pointer;
        region->on_enter = [this, i] { setState([this, i] { hovered_ = i; }); };
        region->on_exit  = [this, i] { setState([this, i] { if (hovered_ == i) hovered_ = -1; }); };
        region->child    = g;
        return region;
    }

    cw::WidgetRef buildSidebar(bool collapsed)
    {
        const auto& colors = ds_->tokens().colors;

        std::vector<cw::WidgetRef> nav_items;
        for (int i = 0; i < (int)kSectionNames.size(); ++i)
            nav_items.push_back(buildNavItem(i, collapsed));

        // The "Gallery / campello_widgets" title has nowhere sensible to
        // go at icon-only width (a lone big letter just reads as noise),
        // so collapsed mode drops it entirely in favor of a small top
        // spacer instead.
        std::vector<cw::WidgetRef> nav_col_children;
        if (collapsed) {
            nav_col_children.push_back(vspace(12.0f));
        } else {
            auto sidebar_title = std::make_shared<cw::Container>();
            sidebar_title->padding = cw::EdgeInsets::symmetric(16.0f, 18.0f);
            sidebar_title->color   = colors.surface;
            sidebar_title->child   = cw::mw<cw::Column>(
                cw::MainAxisAlignment::start, cw::CrossAxisAlignment::start, cw::MainAxisSize::min,
                cw::WidgetList{
                    cw::mw<cw::Text>("Gallery", ts(18.0f, colors.on_surface)),
                    vspace(2.0f),
                    cw::mw<cw::Text>("campello_widgets", ts(10.0f, colors.on_surface_variant)),
                });

            auto title_divider = std::make_shared<cw::Container>();
            title_divider->height = 1.0f;
            title_divider->color  = colors.outline_variant;

            nav_col_children = { sidebar_title, title_divider };
        }
        // Nav items wrapped in Expanded so the theme footer below them
        // (added via buildThemeFooter()) gets pushed to the bottom of the
        // sidebar rather than sitting right after the last nav item.
        nav_col_children.push_back(cw::mw<cw::Expanded>(cw::mw<cw::Column>(
            cw::MainAxisAlignment::start, cw::CrossAxisAlignment::stretch, nav_items)));
        nav_col_children.push_back(buildThemeFooter(collapsed));

        auto nav_col = cw::mw<cw::Column>(
            cw::MainAxisAlignment::start, cw::CrossAxisAlignment::stretch, nav_col_children);

        cw::BoxDecoration sidebar_deco;
        sidebar_deco.color = colors.surface;
        sidebar_deco.box_shadow = {
            cw::BoxShadow{cw::Color::fromRGBA(0,0,0,0.08f), {2,0}, 8.0f, 0.0f}
        };
        auto sidebar_box = std::make_shared<cw::DecoratedBox>(sidebar_deco);
        sidebar_box->child = nav_col;

        // No explicit RepaintBoundary needed here: RenderDecoratedBox
        // self-promotes to one automatically whenever decoration.box_shadow
        // is non-empty (see its isRepaintBoundary() override) — a shadow's
        // Gaussian blur is expensive enough that every such box gets this
        // for free, this one included.
        return std::make_shared<cw::SizedBox>(
            collapsed ? kSidebarCollapsedWidth : kSidebarExpandedWidth,
            std::nullopt, sidebar_box);
    }

    int selected_ = 0;
    int hovered_  = -1; ///< Index of the sidebar nav item currently under the pointer, or -1.
    int keyboard_focused_ = -1; ///< Index of the sidebar nav item with keyboard/D-pad focus, or -1.
    std::shared_ptr<const cw::DesignSystem> ds_;
    GalleryDesignSystemKind kind_ = GalleryDesignSystemKind::campello_ui;
};

std::unique_ptr<cw::StateBase> GalleryShell::createState() const
{
    return std::make_unique<GalleryShellState>();
}

// ---------------------------------------------------------------------------
// Public entry point
// ---------------------------------------------------------------------------
namespace systems::leal::campello_widgets
{
    void setSampleVideoPath(std::string path)
    {
        sampleVideoPathStorage() = std::move(path);
    }

    std::shared_ptr<Widget> buildGalleryApp()
    {
        ImageLoader::instance().initialize(4);

        // Dev shortcuts: Cmd+D / Ctrl+D toggles the performance overlay,
        // Cmd+R / Ctrl+R toggles the repaint-rainbow overlay. Registered as
        // a global key handler (checked before focus-based routing) rather
        // than a KeyboardListener, so the shortcut still fires even while
        // e.g. a TextField elsewhere has keyboard focus — FocusManager
        // only ever routes to the single currently-focused node otherwise.
        // Accepting both ctrl and meta (rather than branching per platform)
        // gets "Cmd on macOS, Ctrl on Windows/Linux" for free, since meta
        // is Cmd on macOS and ctrl is native Ctrl everywhere.
        FocusManager::setGlobalKeyHandler([](const KeyEvent& e) {
            if (e.kind != KeyEventKind::down) return false;
            if (!(e.modifiers & (KeyModifiers::ctrl | KeyModifiers::meta))) return false;

            // Toggling a DebugFlags bool doesn't itself mark anything
            // dirty, so a plain FrameScheduler::scheduleFrame() request
            // can still get skipped — buildFrame() bails out early when
            // root_->needsPaint() is false. forceRefresh() marks the root
            // dirty for layout+paint too, guaranteeing the toggle is
            // actually visible on the very next frame.
            if (e.key_code == KeyCode::d) {
                DebugFlags::showPerformanceOverlay = !DebugFlags::showPerformanceOverlay;
                if (auto* r = detail::currentRenderer().load(std::memory_order_acquire))
                    r->forceRefresh();
                return true;
            }
            if (e.key_code == KeyCode::r) {
                DebugFlags::repaintRainbowEnabled = !DebugFlags::repaintRainbowEnabled;
                if (auto* r = detail::currentRenderer().load(std::memory_order_acquire))
                    r->forceRefresh();
                return true;
            }
            return false;
        });

        // "View" menu — jumps to each gallery tab, with Cmd/Ctrl+1..9,0
        // shortcuts (1-indexed section order; the 10th section, Draw, gets
        // the wrap-around "0"). On macOS/Windows this becomes a real native
        // menu; on Linux, PlatformMenuBarView (docked above the sidebar in
        // GalleryShellState::build()) renders it in-window and these
        // shortcuts are wired through FocusManager's global key handler.
        auto nav_controller = std::make_shared<GalleryNavController>();
        std::vector<PlatformMenuItemRef> view_items;
        view_items.reserve(kSectionNames.size());
        for (int i = 0; i < static_cast<int>(kSectionNames.size()); ++i) {
            const std::string shortcut = "Ctrl+" + std::to_string((i + 1) % 10);
            view_items.push_back(PlatformMenuItemLabel::create(
                kSectionNames[static_cast<size_t>(i)], shortcut,
                [nav_controller, i]() { nav_controller->navigateTo(i); }));
        }
        auto view_menu = PlatformMenu::create("View", std::move(view_items));

        auto shell = std::make_shared<GalleryShell>();
        shell->controller = nav_controller;
        auto entry = std::make_shared<OverlayEntry>(shell);
        auto overlay = std::make_shared<Overlay>(
            std::vector<std::shared_ptr<OverlayEntry>>{ entry });

        return PlatformMenuBar::create({ view_menu }, overlay);
    }
}
