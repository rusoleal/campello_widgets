// Themed component fidelity harness.
// Renders every Cupertino/Material DesignSystem builder configuration that
// the iOS reference app captures, producing PNGs for pixel diff.

#include <campello_widgets/campello_widgets.hpp>
#include <campello_cupertino/cupertino_design_system.hpp>
#include <campello_material/material_design_system.hpp>
#include <campello_fluent/fluent_design_system.hpp>
#include "visual_fidelity.hpp"

#include <campello_gpu/device.hpp>
#include <campello_gpu/texture.hpp>
#include <campello_image/image.hpp>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <map>
#include <string>
#include <vector>

namespace cw = systems::leal::campello_widgets;
namespace cwt = systems::leal::campello_widgets::testing;
namespace GPU = systems::leal::campello_gpu;
namespace ci = systems::leal::campello_image;

// 402x874 is the iPhone 16 Pro/17 Pro logical size (confirmed via a real
// simctl screenshot: 1206x2622 physical / 3x). 393x852 is the *base*
// iPhone 16's size — every simulator this harness's iOS counterpart
// actually targets is a Pro model, so that was simply the wrong constant.
static constexpr float kLogicalWidth   = 402.0f;
static constexpr float kLogicalHeight  = 874.0f;
static constexpr float kDevicePixelRatio = 3.0f;
static constexpr float kPhysicalWidth  = kLogicalWidth  * kDevicePixelRatio;
static constexpr float kPhysicalHeight = kLogicalHeight * kDevicePixelRatio;

// Pixel 7 / campello_m3_expressive_test emulator (Android 16, API 36):
// 1080x2400 physical, 420dpi (`adb shell wm size`/`wm density`) -> DPR
// 420/160 = 2.625, logical 411.43x914.29dp. android_fidelity_reference's
// MainActivity hides system bars and centers one real M3 Expressive
// component full-screen on a plain colorScheme.background fill (no shared
// background image / no 360pt-wide red-border convention — those are iOS-
// mockup-specific alignment aids with no Android equivalent).
static constexpr float kAndroidDevicePixelRatio = 2.625f;
static constexpr float kAndroidPhysicalWidth  = 1080.0f;
static constexpr float kAndroidPhysicalHeight = 2400.0f;
static constexpr float kAndroidLogicalWidth   = kAndroidPhysicalWidth  / kAndroidDevicePixelRatio;
static constexpr float kAndroidLogicalHeight  = kAndroidPhysicalHeight / kAndroidDevicePixelRatio;

// Same 136px status bar export_references.sh crops off before comparison
// (see compare_android_cpp.py's STATUS_BAR_PX), converted to logical dp.
// Needed because Android's AlertDialog window centers within the display
// area *below the status bar down to the physical bottom edge* — the nav
// bar is an edge-to-edge overlay, not a layout-reserving inset — not within
// the full physical screen. Confirmed empirically: a real capture's dialog
// card sits ~26dp lower than dead-center of the (already status/nav-bar-
// cropped) comparison frame, matching exactly half this value.
static constexpr float kAndroidStatusBarLogicalHeight = 136.0f / kAndroidDevicePixelRatio;

// iPad Pro 11-inch (M4) simulator: 1668x2420 physical, DPR 2 (`simctl io
// screenshot` measured). navigationRail has no real iPhone equivalent at
// all (see RealCapture.swift's addNavigationRail) — its only real ground
// truth is a UISplitViewController sidebar on iPad, hence its own device
// class and its own render function rather than sharing kLogicalWidth/
// kPhysicalWidth with every other iOS builder.
static constexpr float kIpadDevicePixelRatio  = 2.0f;
static constexpr float kIpadPhysicalWidth     = 1668.0f;
static constexpr float kIpadPhysicalHeight    = 2420.0f;
static constexpr float kIpadLogicalHeight     = kIpadPhysicalHeight / kIpadDevicePixelRatio;

// Real capture's window.safeAreaInsets (top: status bar, bottom: home
// indicator), physical pixels — written dynamically to safe_area.txt per
// capture by RealCapture.swift, needed here since the C++ side has no
// live device to query it from. Differs by OS version on the exact same
// iPad model/hardware: iOS 18.6 (cupertino themes) reports 48/50, iOS 26
// (liquid_glass themes) reports 64/40 — confirmed from real captures'
// own written safe_area.txt values, not guessed.
static constexpr float kIpadTopInsetCupertinoPhysical    = 48.0f;
static constexpr float kIpadBottomInsetCupertinoPhysical = 50.0f;
static constexpr float kIpadTopInsetLiquidGlassPhysical    = 64.0f;
static constexpr float kIpadBottomInsetLiquidGlassPhysical = 40.0f;

// Real UISplitViewController sidebar width when requested at 100pt
// (compact, icon-only) / 260pt (extended, icon+label) primary-column
// width — lines up almost exactly with 2x the request (this iPad's DPR),
// no extra chrome. Measured from a capture with the detail column's
// backdrop image actually loaded, using the gray-sidebar-to-colorful-
// backdrop color transition (an earlier measurement attempt used a
// manual run where that image had silently failed to load, making the
// whole screen look uniformly blank and hiding the true boundary).
// export_references.sh crops the real capture to exactly these widths
// too, so both sides match without further size-reconciliation logic in
// compare_ios_cpp.py.
static constexpr float kIpadRailWidthCompactPhysical  = 199.0f;
static constexpr float kIpadRailWidthExtendedPhysical = 520.0f;

// windows_fidelity_reference's WinUI 3 app window: a fixed moderate size
// (not this machine's full monitor — capturing single small components at
// full desktop resolution would be wasteful and isn't how a real WinUI3 app
// would realistically size a window for this content) at this machine's
// real DPI scaling (192/96 = 2.0, confirmed via GetDpiForWindow — see
// run_app.cpp's identical dpi/96.0f calculation), so text/stroke rendering
// is genuinely comparable between the two sides.
static constexpr float kFluentDevicePixelRatio = 2.0f;
static constexpr float kFluentLogicalWidth     = 480.0f;
static constexpr float kFluentLogicalHeight    = 360.0f;
static constexpr float kFluentPhysicalWidth    = kFluentLogicalWidth  * kFluentDevicePixelRatio;
static constexpr float kFluentPhysicalHeight   = kFluentLogicalHeight * kFluentDevicePixelRatio;

struct Case {
    std::string theme;
    std::string builder;
    std::string state;
};

static std::vector<std::string> builderStates(const std::string& builder)
{
    static const std::map<std::string, std::vector<std::string>> states = {
        {"button", {"primary", "secondary", "tertiary", "danger", "disabled"}},
        {"switch", {"on", "off", "disabled"}},
        {"slider", {"value", "disabled"}},
        {"textField", {"empty", "filled", "disabled"}},
        {"card", {"elevated", "filled", "outlined"}},
        {"listTile", {"one_line", "two_line", "with_icon"}},
        {"divider", {"default", "indented"}},
        {"appBar", {"default", "center_title"}},
        {"navigationBar", {"three_items"}},
        {"dialog", {"one_action", "two_actions", "three_actions"}},
        {"popupMenuButton", {"closed", "open"}},
        {"dropdownButton", {"closed", "open"}},
        {"primaryActionButton", {"icon", "label"}},
        {"tabBar", {"two_tabs"}},
        {"chip", {"unselected", "selected"}},
        {"segmentedButton", {"three_segments"}},
        {"bottomSheet", {"partial"}},
        {"badge", {"dot", "number"}},
        {"iconButton", {"plain", "filled", "selected"}},
        {"stepper", {"default", "disabled"}},
        {"ratingIndicator", {"three_of_five"}},
        {"actionSheet", {"open"}},
        {"searchField", {"empty", "filled"}},
        {"datePicker", {"compact"}},
        {"timePicker", {"compact"}},
        {"expansionTile", {"collapsed", "expanded"}},
        {"toggleButtons", {"multi"}},
        {"banner", {"default"}},
        {"navigationRail", {"compact", "extended"}},
        {"dataTable", {"default"}},
        {"confirmationDialog", {"remove_app"}},
    };
    auto it = states.find(builder);
    if (it == states.end()) return {};
    return it->second;
}

static std::vector<std::string> themes()
{
    return {"cupertino_light", "cupertino_dark", "liquid_glass_light", "liquid_glass_dark",
            "expressive_light", "expressive_dark", "fluent_light", "fluent_dark"};
}

static bool isAndroidTheme(const std::string& theme)
{
    return theme == "expressive_light" || theme == "expressive_dark";
}

static bool isFluentTheme(const std::string& theme)
{
    return theme == "fluent_light" || theme == "fluent_dark";
}

// Only the builders android_fidelity_reference/ComponentCatalog.kt actually
// implements — extend both sides together as coverage grows, same
// incremental-per-builder discipline used for the iOS dialog work.
static std::vector<std::string> androidBuilders()
{
    return {"button", "switch", "card", "slider", "chip", "divider", "listTile", "textField",
            "segmentedButton", "dialog", "tabBar", "dropdownButton", "toggleButtons",
            "popupMenuButton", "badge", "iconButton", "navigationRail", "appBar",
            "primaryActionButton", "navigationBar", "bottomSheet", "stepper",
            "ratingIndicator", "actionSheet", "searchField", "datePicker", "timePicker",
            "expansionTile", "banner", "dataTable"};
}

// Android-specific state override: dropdownButton's shared "open" state
// would need a real anchored DropdownMenu overlay capture — the same
// overlay-anchor positioning complexity that's kept popupMenuButton's
// "open" state deferred all session. buildWidget() also doesn't currently
// vary its output by state for dropdownButton, so "open" would just
// duplicate "closed" on the C++ side too. Only the real ExposedDropdownMenuBox
// closed-state chrome is covered here; falls back to the shared table for
// every other builder.
static std::vector<std::string> androidBuilderStates(const std::string& builder)
{
    if (builder == "dropdownButton") return {"closed"};
    // Real M3 NavigationRail has no native "extended" mode — that
    // horizontal icon+label layout is a NavigationDrawer concept, not
    // NavigationRail's. Only "compact" (the real widget's actual shape)
    // gets a real-capture comparison.
    if (builder == "navigationRail") return {"compact"};
    return builderStates(builder);
}

static std::vector<std::string> builders()
{
    return {
        "button", "switch", "slider", "textField", "card", "listTile", "divider",
        "appBar", "navigationBar", "dialog", "popupMenuButton", "dropdownButton",
        "primaryActionButton", "tabBar", "chip", "segmentedButton", "bottomSheet",
        "badge", "iconButton", "stepper", "ratingIndicator", "actionSheet", "searchField",
        "datePicker", "timePicker", "expansionTile", "toggleButtons", "banner",
        "navigationRail", "dataTable", "confirmationDialog",
    };
}

// builders() minus "confirmationDialog" — that case's buildWidget() branch
// does an unconditional static_cast<const CupertinoDesignSystem&>(ds) (it
// calls Cupertino-specific buildConfirmationDialog()/tokens() directly,
// with no equivalent on the base DesignSystem interface), which is
// undefined behavior for any ds that isn't actually a CupertinoDesignSystem.
// Safe for cupertino_light/dark/liquid_glass_* (the only themes that used
// the full builders() list before Fluent support existed); Fluent must use
// this filtered list instead.
static std::vector<std::string> fluentBuilders()
{
    auto all = builders();
    all.erase(std::remove(all.begin(), all.end(), "confirmationDialog"), all.end());
    return all;
}

static std::string fileName(const Case& c)
{
    return c.builder + "_" + c.state;
}

static std::shared_ptr<cw::DesignSystem> makeDesignSystem(const std::string& theme)
{
    if (theme == "cupertino_light") return std::make_shared<cw::CupertinoDesignSystem>(cw::CupertinoDesignSystem::light());
    if (theme == "cupertino_dark")  return std::make_shared<cw::CupertinoDesignSystem>(cw::CupertinoDesignSystem::dark());
    if (theme == "liquid_glass_light") return std::make_shared<cw::CupertinoDesignSystem>(cw::CupertinoDesignSystem::liquidGlass(false));
    if (theme == "liquid_glass_dark")  return std::make_shared<cw::CupertinoDesignSystem>(cw::CupertinoDesignSystem::liquidGlass(true));
    if (theme == "expressive_light")   return std::make_shared<cw::MaterialDesignSystem>(cw::MaterialDesignSystem::expressiveLight());
    if (theme == "expressive_dark")    return std::make_shared<cw::MaterialDesignSystem>(cw::MaterialDesignSystem::expressiveDark());
    if (theme == "fluent_light")       return std::make_shared<cw::FluentDesignSystem>(cw::FluentDesignSystem::light());
    if (theme == "fluent_dark")        return std::make_shared<cw::FluentDesignSystem>(cw::FluentDesignSystem::dark());
    return nullptr;
}

static cw::WidgetRef text(const std::string& str, const cw::TextStyle& style = cw::TextStyle{})
{
    return std::make_shared<cw::Text>(str, style);
}

// text()'s default TextStyle color is Color::black() — fine against the
// light-ish canvases most themes render on, but there is no
// DefaultTextStyle/InheritedWidget mechanism anywhere in campello_widgets
// that retints a plain Text based on the ambient Theme (confirmed: Theme
// only propagates the DesignSystem pointer, nothing merges text color).
// Builders that draw their own surface behind arbitrary passed-in content
// (Card/DataTable/ListTile/Dialog/ActionSheet/BottomSheet/ExpansionTile/
// Banner) render that content completely unchanged — so under a dark theme,
// plain text() content is black-on-dark and nearly unreadable. Confirmed via
// fluent_dark's report/screenshots (card_filled, dataTable_default). Use
// this instead of text() for any content flowing into those builders.
static cw::WidgetRef surfaceText(const cw::DesignSystem& ds, const std::string& str,
                                  cw::TextStyle style = cw::TextStyle{})
{
    style.color = ds.tokens().colors.on_surface;
    return std::make_shared<cw::Text>(str, style);
}

// Simple "★" placeholder, unrelated to the real-icon loading below — kept
// for builders (e.g. Fluent's with_icon states) that don't need a
// platform-accurate glyph, just a stand-in.
static cw::WidgetRef icon(const std::string& /*name*/)
{
    return text("★");
}

// Real SF Symbol (iOS)/Material Symbol (Android) template PNGs, sourced
// once per session — see tests/visual_fidelity/test_images/icons/{ios,
// android}/. Only the 5 icons the current Cupertino/Material real-capture
// builders (navigationBar/navigationRail/badge/iconButton) actually need
// are sourced so far; any other name falls back to the "★" placeholder
// this whole function used to unconditionally return.
static std::shared_ptr<GPU::Texture> loadIconTexture(const std::string& platform, const std::string& name)
{
    static std::map<std::string, std::shared_ptr<GPU::Texture>> cache;
    const std::string key = platform + "/" + name;
    auto it = cache.find(key);
    if (it != cache.end()) return it->second;

    auto device = cwt::sharedGpuDevice();
    if (!device) return nullptr;

    std::filesystem::path path = std::filesystem::path(cwt::getVisualFidelityDirectory()) /
                                 "test_images" / "icons" / platform / (name + ".png");
    auto img = ci::Image::fromFile(path.string().c_str());
    if (!img) return nullptr;
    if (img->getFormat() != ci::ImageFormat::rgba8) return nullptr;

    uint32_t w = static_cast<uint32_t>(img->getWidth());
    uint32_t h = static_cast<uint32_t>(img->getHeight());
    auto tex = device->createTexture(
        GPU::TextureType::tt2d,
        GPU::PixelFormat::rgba8unorm,
        w, h, 1, 1, 1,
        static_cast<GPU::TextureUsage>(
            static_cast<int>(GPU::TextureUsage::textureBinding) |
            static_cast<int>(GPU::TextureUsage::copyDst)));
    if (!tex) return nullptr;

    // Icon::create()'s tinted-image draw path only ever samples this
    // texture's *alpha* channel (see DrawTintedImageCmd's doc comment) —
    // unlike loadBackgroundTexture(), there's no need to premultiply RGB
    // by alpha here since RGB is never read.
    std::vector<uint8_t> pixels(
        static_cast<const uint8_t*>(img->getData()),
        static_cast<const uint8_t*>(img->getData()) + static_cast<size_t>(w) * h * 4);
    if (!tex->upload(0, w * h * 4, pixels.data())) return nullptr;

    cache[key] = tex;
    return tex;
}

static cw::WidgetRef icon(const cw::DesignSystem& ds, const std::string& name)
{
    using namespace cw;
    const bool is_material = dynamic_cast<const MaterialDesignSystem*>(&ds) != nullptr;
    const std::string platform = is_material ? "android" : "ios";
    auto tex = loadIconTexture(platform, name);
    if (!tex) return text("★"); // not yet sourced for this name — see loadIconTexture()'s comment
    return Icon::create(tex, 24.0f, Color::black());
}

static cw::WidgetRef buildWidget(const cw::DesignSystem& ds, const std::string& builder, const std::string& state)
{
    using namespace cw;

    if (builder == "button") {
        ButtonConfig cfg;
        cfg.label = text("Button");
        cfg.on_pressed = [] {};
        if (state == "primary")   cfg.priority = ButtonPriority::primary;
        if (state == "secondary") cfg.priority = ButtonPriority::secondary;
        if (state == "tertiary")  cfg.priority = ButtonPriority::tertiary;
        if (state == "danger")    cfg.priority = ButtonPriority::danger;
        if (state == "disabled")  { cfg.priority = ButtonPriority::tertiary; cfg.enabled = false; }
        return ds.buildButton(cfg);
    }

    if (builder == "switch") {
        SwitchConfig cfg;
        cfg.on_changed = [](bool) {};
        if (state == "on")       cfg.value = true;
        if (state == "off")      cfg.value = false;
        if (state == "disabled") { cfg.enabled = false; cfg.value = false; }
        return ds.buildSwitch(cfg);
    }

    if (builder == "slider") {
        SliderConfig cfg;
        cfg.value = 0.33f;
        cfg.on_changed = [](float) {};
        if (state == "disabled") cfg.enabled = false;
        return ds.buildSlider(cfg);
    }

    if (builder == "textField") {
        TextFieldConfig cfg;
        cfg.placeholder = "Placeholder";
        cfg.on_changed = [](const std::string&) {};
        if (state == "filled")   cfg.value = "Hello";
        if (state == "disabled") cfg.enabled = false;
        return SizedBox::from_width(240.0f, ds.buildTextField(cfg));
    }

    if (builder == "card") {
        CardConfig cfg;
        cfg.child = surfaceText(ds, "Card content");
        if (state == "elevated") cfg.priority = CardPriority::elevated;
        if (state == "filled")   cfg.priority = CardPriority::filled;
        if (state == "outlined") cfg.priority = CardPriority::outlined;
        // No fixed width here — real M3 Card shrink-wraps to its content
        // (confirmed against a real capture: ~340px physical for "Card
        // content" text, not a round dp value), and this function is
        // shared with the Android path, which needs that natural
        // shrink-wrap. iOS's own 240dp presentation width (if still
        // wanted) is applied in wrapForViewport() instead, keeping this
        // platform-specific sizing decision out of shared code.
        return ds.buildCard(cfg);
    }

    if (builder == "listTile") {
        ListTileConfig cfg;
        cfg.title = surfaceText(ds, "Title");
        if (state == "two_line")  cfg.subtitle = surfaceText(ds, "Subtitle");
        if (state == "with_icon") cfg.leading = icon(ds, "star");
        cfg.on_tap = [] {};
        return ds.buildListTile(cfg);
    }

    if (builder == "divider") {
        DividerConfig cfg;
        if (state == "indented") { cfg.indent = 16.0f; cfg.end_indent = 16.0f; }
        return ds.buildDivider(cfg);
    }

    if (builder == "appBar") {
        AppBarConfig cfg;
        cfg.title = text(state == "center_title" ? "Title" : "Navigation");
        cfg.leading = icon(ds, "chevron.left");
        cfg.actions = { icon(ds, "gear") };
        return ds.buildAppBar(cfg);
    }

    if (builder == "navigationBar") {
        NavigationBarConfig cfg;
        cfg.items = {
            {icon(ds, "house"), "First"},
            {icon(ds, "magnifyingglass"), "Second"},
            {icon(ds, "person"), "Third"},
        };
        cfg.selected_index = 0;
        cfg.on_tap = [](int) {};
        return ds.buildNavigationBar(cfg);
    }

    if (builder == "dialog") {
        DialogConfig cfg;
        cfg.title = surfaceText(ds, "Title");
        cfg.content = surfaceText(ds, "Message");
        // UIAlertController always visually places a .cancel-style action
        // last, regardless of the order actions were added in — confirmed
        // against a real-captured reference screenshot (RealCapture.swift's
        // presentDialog() adds Cancel first, but real iOS rendered it last,
        // separated from the other rows). Build the list in that same
        // display order here rather than insertion order.
        if (state == "one_action") {
            cfg.actions = { text("OK") };
        } else if (state == "two_actions") {
            cfg.actions = { text("Cancel"), text("OK") };
        } else if (state == "three_actions") {
            cfg.actions = {
                text("OK"),
                std::make_shared<Text>("Delete", TextStyle{}.withColor(Color::fromARGB(0xFFFF3B30))),
                text("Cancel"),
            };
        }
        return ds.buildDialog(cfg);
    }

    if (builder == "popupMenuButton") {
        PopupMenuConfig cfg;
        cfg.items = {{"One", [] {}}, {"Two", [] {}}};
        cfg.child = ds.buildButton(ButtonConfig{.label = text("Open Menu"), .on_pressed = [] {}, .priority = ButtonPriority::tertiary});
        cfg.on_selected = [](size_t) {};
        return ds.buildPopupMenuButton(cfg);
    }

    if (builder == "dropdownButton") {
        DropdownConfig cfg;
        cfg.items = {{"Option 1", "1"}, {"Option 2", "2"}};
        cfg.hint = "Select";
        cfg.on_changed = [](const std::string&) {};
        // Same fixed-width convention as textField above — its real Android
        // counterpart (ExposedDropdownMenuBox) needs an explicit width too,
        // matched here so both sides render the same 240dp box.
        return SizedBox::from_width(240.0f, ds.buildDropdownButton(cfg));
    }

    if (builder == "primaryActionButton") {
        PrimaryActionConfig cfg;
        cfg.on_pressed = [] {};
        if (state == "icon") cfg.icon = icon(ds, "plus");
        else                 cfg.label = text("+");
        return ds.buildPrimaryActionButton(cfg);
    }

    if (builder == "tabBar") {
        TabBarConfig cfg;
        cfg.tabs = {{"One", nullptr}, {"Two", nullptr}};
        cfg.selected_index = 0;
        cfg.on_tap = [](int) {};
        return ds.buildTabBar(cfg);
    }

    if (builder == "chip") {
        ChipConfig cfg;
        cfg.label = text("Chip");
        cfg.on_selected = [] {};
        if (state == "selected") cfg.selected = true;
        return ds.buildChip(cfg);
    }

    if (builder == "segmentedButton") {
        SegmentedConfig cfg;
        cfg.segments = {{text("Day"), nullptr}, {text("Week"), nullptr}, {text("Month"), nullptr}};
        cfg.selected_index = 0;
        cfg.on_changed = [](int) {};
        return SizedBox::from_width(280.0f, ds.buildSegmentedButton(cfg));
    }

    if (builder == "bottomSheet") {
        BottomSheetConfig cfg;
        cfg.child = surfaceText(ds, "Sheet content");
        cfg.show_drag_handle = true;
        return ds.buildBottomSheet(cfg);
    }

    if (builder == "badge") {
        BadgeConfig cfg;
        cfg.child = icon(ds, "bell");
        if (state == "number") cfg.label = "3";
        return ds.buildBadge(cfg);
    }

    if (builder == "iconButton") {
        IconButtonConfig cfg;
        cfg.icon = icon(ds, "heart");
        cfg.on_pressed = [] {};
        if (state == "filled")   cfg.selected = false;
        if (state == "selected") cfg.selected = true;
        return ds.buildIconButton(cfg);
    }

    if (builder == "stepper") {
        StepperConfig cfg;
        cfg.on_changed = [](int) {};
        if (state == "disabled") cfg.enabled = false;
        return ds.buildStepper(cfg);
    }

    if (builder == "ratingIndicator") {
        RatingConfig cfg;
        cfg.value = 3;
        cfg.max = 5;
        return ds.buildRatingIndicator(cfg);
    }

    if (builder == "actionSheet") {
        ActionSheetConfig cfg;
        cfg.title = surfaceText(ds, "Title");
        cfg.actions = {{"Save", [] {}, false}, {"Delete", [] {}, true}};
        cfg.on_cancel = [] {};
        return ds.buildActionSheet(cfg);
    }

    if (builder == "searchField") {
        SearchFieldConfig cfg;
        cfg.placeholder = "Search";
        cfg.on_changed = [](const std::string&) {};
        if (state == "filled") cfg.value = "query";
        // No fixed width here — real M3's search field is edge-to-edge
        // (confirmed against a real capture), not a narrow pill, and this
        // function is shared with the Android path. iOS's own 280pt
        // presentation width is applied explicitly in
        // renderSearchFieldCase() instead.
        return ds.buildSearchField(cfg);
    }

    if (builder == "datePicker") {
        DatePickerConfig cfg;
        cfg.label = "Aug 14, 2026";
        cfg.on_tap = [] {};
        return ds.buildDatePicker(cfg);
    }

    if (builder == "timePicker") {
        TimePickerConfig cfg;
        cfg.label = "10:30 AM";
        cfg.on_tap = [] {};
        return ds.buildTimePicker(cfg);
    }

    if (builder == "expansionTile") {
        ExpansionTileConfig cfg;
        cfg.title = surfaceText(ds, "Settings");
        cfg.children_content = surfaceText(ds, "Expanded content goes here.");
        cfg.on_expansion_changed = [](bool) {};
        if (state == "expanded") cfg.expanded = true;
        return ds.buildExpansionTile(cfg);
    }

    if (builder == "toggleButtons") {
        ToggleButtonsConfig cfg;
        cfg.items = {{text("A"), nullptr, true}, {text("B"), nullptr, false}, {text("C"), nullptr, true}};
        cfg.on_pressed = [](int) {};
        return ds.buildToggleButtons(cfg);
    }

    if (builder == "banner") {
        BannerConfig cfg;
        cfg.content = surfaceText(ds, "A banner message");
        return ds.buildBanner(cfg);
    }

    if (builder == "navigationRail") {
        NavigationRailConfig cfg;
        cfg.items = {{icon(ds, "house"), "Home"}, {icon(ds, "magnifyingglass"), "Search"}, {icon(ds, "person"), "Profile"}};
        cfg.extended = (state == "extended");
        cfg.on_tap = [](int) {};
        return ds.buildNavigationRail(cfg);
    }

    if (builder == "dataTable") {
        DataTableConfig cfg;
        cfg.columns = {"Name", "Age"};
        cfg.rows = {{surfaceText(ds, "Alice"), surfaceText(ds, "30")}, {surfaceText(ds, "Bob"), surfaceText(ds, "25")}};
        return ds.buildDataTable(cfg);
    }

    if (builder == "confirmationDialog") {
        // Matches a real iOS 26 "remove app" prompt exactly (title/message/
        // action strings), verified against a device screenshot.
        const auto& cupertino = static_cast<const CupertinoDesignSystem&>(ds);

        TextStyle title_style;
        title_style.font_size   = 19.0f;
        title_style.font_weight = FontWeight::bold;
        title_style.color       = cupertino.tokens().colors.on_surface;

        TextStyle msg_style;
        msg_style.font_size = 15.0f;
        msg_style.color     = cupertino.tokens().colors.on_surface_variant;

        ConfirmationDialogConfig cfg;
        cfg.title = std::make_shared<Text>("¿Eliminar Freeform?", title_style);
        cfg.message = std::make_shared<Text>(
            "Si la eliminas de la pantalla de inicio, la app se guardará en tu biblioteca de apps.",
            msg_style);
        cfg.actions = {
            {"Eliminar de la pantalla de inicio", [] {}, false},
            {"Eliminar app", [] {}, true},
        };
        cfg.on_cancel    = [] {};
        cfg.cancel_label = "Cancelar";
        return cupertino.buildConfirmationDialog(cfg);
    }

    return nullptr;
}

// Note: deliberately does NOT call box->layout() here. captureRenderBoxToPng()
// constructs its own per-case Renderer + IDrawBackend and lays out via
// Renderer::buildFrame() (which sets RenderObject::setActiveBackend() to that
// backend right before laying out). RenderObject::activeBackend() is a raw,
// non-owning global pointer — laying out here, before that backend exists,
// would read whatever the *previous* case's (already-destroyed) backend left
// behind, a dangling-pointer read that segfaults as soon as any RenderText
// under `widget` measures text during layout.
// Wraps in a real Theme ancestor before mounting. Most builders don't need
// this — ds.buildXxx(cfg) bakes the design system's resolved colors into
// the returned widget tree directly — but a StatefulWidget like
// DropdownButton looks up Theme::tokensOf(ctx) live from its own build(),
// which silently falls back to NullDesignSystem (light-ish defaults) with
// no Theme ancestor. That was invisible in light theme (the fallback
// happened to look close enough) but produced a stark white dropdown box
// in dark theme — confirmed by a real capture diff.
static std::shared_ptr<cw::RenderBox> mountAndLayout(const cw::DesignSystem& ds, cw::WidgetRef widget, float /*w*/, float /*h*/)
{
    auto themed = std::make_shared<cw::Theme>(std::shared_ptr<const cw::DesignSystem>(&ds, [](const cw::DesignSystem*) {}), widget);
    auto element = themed->createElement();
    element->mount(nullptr);

    auto* roe = element->findDescendantRenderObjectElement();
    if (!roe) {
        std::cerr << "No RenderObjectElement found\n";
        return nullptr;
    }
    auto box = std::dynamic_pointer_cast<cw::RenderBox>(roe->sharedRenderObject());
    if (!box) {
        std::cerr << "RenderObject is not a RenderBox\n";
        return nullptr;
    }
    return box;
}

static bool usesFullWidthLayout(const std::string& builder)
{
    // These components are rendered edge-to-edge inside the 360 pt reference
    // container in the iOS reference app, so they skip the 16 pt content
    // inset.
    static const std::vector<std::string> fullWidth = {
        "appBar", "navigationBar", "bottomSheet", "banner", "actionSheet",
    };
    return std::find(fullWidth.begin(), fullWidth.end(), builder) != fullWidth.end();
}

static cw::WidgetRef wrapWithRedBorder(cw::WidgetRef widget)
{
    using namespace cw;

    // 5 dp padding between the red border and the widget under test.
    auto innerPadding = std::make_shared<Padding>();
    innerPadding->padding = EdgeInsets::all(5.0f);
    innerPadding->child = std::move(widget);

    // 1 dp red border around the padded widget.
    auto bordered = std::make_shared<DecoratedBox>();
    bordered->decoration.border = BoxBorder::all(Color::fromARGB(0xFFFF0000), 1.0f);
    bordered->child = std::move(innerPadding);

    return bordered;
}

static std::shared_ptr<GPU::Texture> loadBackgroundTexture()
{
    static std::shared_ptr<GPU::Texture> cached;
    if (cached) return cached;

    auto device = cwt::sharedGpuDevice();
    if (!device) return nullptr;

    std::filesystem::path path = std::filesystem::path(cwt::getVisualFidelityDirectory()) /
                                 "test_images" / "liquid_glass_background.png";
    auto img = ci::Image::fromFile(path.string().c_str());
    if (!img) {
        std::cerr << "Failed to load liquid glass background: " << path << "\n";
        return nullptr;
    }

    if (img->getFormat() != ci::ImageFormat::rgba8) {
        std::cerr << "Liquid glass background must be RGBA8\n";
        return nullptr;
    }

    uint32_t w = static_cast<uint32_t>(img->getWidth());
    uint32_t h = static_cast<uint32_t>(img->getHeight());
    auto tex = device->createTexture(
        GPU::TextureType::tt2d,
        GPU::PixelFormat::rgba8unorm,
        w, h, 1, 1, 1,
        static_cast<GPU::TextureUsage>(
            static_cast<int>(GPU::TextureUsage::textureBinding) |
            static_cast<int>(GPU::TextureUsage::copyDst)));
    if (!tex) {
        std::cerr << "Failed to create background texture\n";
        return nullptr;
    }

    // The source PNG stores straight (non-premultiplied) alpha, but every GPU
    // draw pipeline (quad/rect/shape) blends assuming premultiplied-alpha
    // textures (ONE, ONE_MINUS_SRC_ALPHA). Premultiply here so this texture
    // matches that convention instead of blowing out to white where alpha < 1.
    std::vector<uint8_t> premultiplied(
        static_cast<const uint8_t*>(img->getData()),
        static_cast<const uint8_t*>(img->getData()) + static_cast<size_t>(w) * h * 4);
    for (size_t i = 0; i < premultiplied.size(); i += 4) {
        const uint8_t a = premultiplied[i + 3];
        premultiplied[i + 0] = static_cast<uint8_t>((premultiplied[i + 0] * a + 127) / 255);
        premultiplied[i + 1] = static_cast<uint8_t>((premultiplied[i + 1] * a + 127) / 255);
        premultiplied[i + 2] = static_cast<uint8_t>((premultiplied[i + 2] * a + 127) / 255);
    }

    if (!tex->upload(0, w * h * 4, premultiplied.data())) {
        std::cerr << "Failed to upload background texture\n";
        return nullptr;
    }

    cached = std::move(tex);
    return cached;
}

static bool isLiquidGlass(const std::string& theme)
{
    return theme == "liquid_glass_light" || theme == "liquid_glass_dark";
}

static bool isDarkTheme(const std::string& theme)
{
    return theme == "cupertino_dark" || theme == "liquid_glass_dark" || theme == "expressive_dark" ||
           theme == "fluent_dark";
}

static cw::WidgetRef wrapForViewport(const cw::DesignSystem& ds, cw::WidgetRef widget,
                                     const std::string& builder,
                                     const std::string& /*theme*/)
{
    using namespace cw;

    // buildWidget()'s "card" case intentionally returns a shrink-wrapped
    // card (matching real M3's Card, which sizes to its content) since
    // this function is shared with the Android path. iOS's own reference
    // capture was measured/validated at a fixed 240dp presentation width,
    // so preserve that here rather than in the shared builder.
    if (builder == "card") {
        widget = SizedBox::from_width(240.0f, widget);
    }

    // Use the same colourful non-flat background for every theme so all
    // fidelity comparisons are made against a shared, visually interesting
    // backdrop. This makes translucency/blur effects observable and keeps the
    // test matrix consistent.
    WidgetRef background;
    auto tex = loadBackgroundTexture();
    if (tex) {
        auto bgImage = std::make_shared<Image>();
        bgImage->texture = tex;
        bgImage->fit = BoxFit::fill;
        background = std::make_shared<SizedBox>(kLogicalWidth, kLogicalHeight, bgImage);
    }

    // Visual alignment aid: red 1 dp border + 5 dp internal padding shared with
    // the iOS reference screenshots.
    widget = wrapWithRedBorder(std::move(widget));

    auto viewport = std::make_shared<Stack>();
    if (background) {
        viewport->children.push_back(background);
    }

    // iOS reference places every component inside a 360 pt wide centered
    // container. Most content is inset by 16 pt (content width 328 pt). The
    // inner Center keeps intrinsically-sized widgets centered; widgets that
    // expand horizontally fill the 328 pt content area.
    auto contentCenter = std::make_shared<Center>();
    contentCenter->child = widget;

    WidgetRef content = contentCenter;
    if (!usesFullWidthLayout(builder)) {
        auto padding = std::make_shared<Padding>();
        padding->padding = EdgeInsets::all(16.0f);
        padding->child = contentCenter;
        content = padding;
    }

    // Keep the reference container transparent: the shared background image
    // is the only backdrop, matching the updated iOS reference app.
    auto referenceContainer = std::make_shared<Container>();
    referenceContainer->width = 360.0f;
    referenceContainer->child = content;

    auto outerCenter = std::make_shared<Center>();
    outerCenter->child = referenceContainer;
    viewport->children.push_back(outerCenter);

    return std::make_shared<SizedBox>(kLogicalWidth, kLogicalHeight, viewport);
}

// dialog (and, incrementally, actionSheet/searchField/navigationBar) skip
// wrapForViewport()'s artificial 360pt-wide + red-border wrapper — the iOS
// reference for these builders is now a genuinely live-presented system
// control (see ios_fidelity_reference's RealCapture.swift), positioned by
// the OS itself on the full screen, not by our own hand-picked container.
// showDialog() is the production entry point real app code uses to show a
// Dialog widget, so using it here (instead of hand-building a centered
// Stack) is what makes this capture's framing genuinely comparable to a
// real UIAlertController screenshot.
static bool renderDialogCase(const cw::DesignSystem& ds, const Case& c, const std::filesystem::path& outDir)
{
    using namespace cw;

    auto dialogWidget = buildWidget(ds, c.builder, c.state);
    if (!dialogWidget) {
        std::cerr << "Unknown builder: " << c.builder << "\n";
        return false;
    }

    WidgetRef background;
    auto tex = loadBackgroundTexture();
    if (tex) {
        auto bgImage = std::make_shared<Image>();
        bgImage->texture = tex;
        bgImage->fit = BoxFit::fill;
        background = std::make_shared<SizedBox>(kLogicalWidth, kLogicalHeight, bgImage);
    }

    // Mirrors showDialog()'s own construction (barrier + centered dialog in
    // a Stack) as a second initial_entries item, rather than calling
    // showDialog() itself after mounting: showDialog()'s Overlay::insert()
    // only marks the element dirty for a *later* rebuild, which relies on
    // some active Renderer noticing and requesting a repaint — machinery
    // that doesn't exist yet this early (captureRenderBoxToPng() hasn't
    // constructed its per-case Renderer at this point). Present from the
    // very first build sidesteps that timing question entirely, matching
    // how every other (non-dialog) case here already works.
    auto dialogPadding = std::make_shared<Padding>();
    dialogPadding->padding = EdgeInsets::all(40.0f);
    dialogPadding->child   = dialogWidget;

    auto centeredDialog = std::make_shared<Center>();
    centeredDialog->child = dialogPadding;

    // A real UIAlertController centers within the presenting view's safe
    // area, not the literal screen edges — export_references.sh's real
    // capture crops away the status bar (62pt) and home indicator (34pt,
    // both measured from a real device via RealCapture.swift's own
    // safeAreaInsets) before saving, so the comparison is only fair if
    // Center above is constrained to that same excluded sub-region too (an
    // *outer* padding, shrinking Center's incoming constraints — not padding
    // around the dialog itself, which would just add blank space instead of
    // shifting where "centered" means).
    auto safeAreaPadding = std::make_shared<Padding>();
    safeAreaPadding->padding = EdgeInsets::only(0.0f, 62.0f, 0.0f, 34.0f);
    safeAreaPadding->child   = centeredDialog;

    // Real UIAlertController dims the presenting view with a black scrim —
    // confirmed by sampling real-captured reference screenshots against
    // the raw, undimmed background asset at several points. Light theme
    // measured ~20% opacity; dark theme measured a stronger ~48% (ref
    // pixels consistently ~0.65x a light-calibrated 20%-scrim render,
    // i.e. an effective (1-0.48)/(1-0.20) ratio) — the same light/dark
    // dimming-opacity asymmetry already found elsewhere this session for
    // glass-pill fills and dialog card backgrounds (real iOS's dimming
    // colors aren't a fixed opacity; dark surfaces need more darkening to
    // read as "dimmed" against an already-dark backdrop).
    const float scrimOpacity = isDarkTheme(c.theme) ? 0.48f : 0.20f;
    auto barrierAndDialog = std::make_shared<Stack>();
    barrierAndDialog->fit = StackFit::expand;
    barrierAndDialog->children.push_back(ModalBarrier::create(Color::fromRGBA(0.0f, 0.0f, 0.0f, scrimOpacity), false, nullptr));
    barrierAndDialog->children.push_back(safeAreaPadding);

    auto overlay = std::make_shared<Overlay>();
    if (background) {
        overlay->initial_entries.push_back(OverlayEntry::create(background));
    }
    overlay->initial_entries.push_back(OverlayEntry::create(barrierAndDialog));

    auto rootWidget = std::make_shared<SizedBox>(kLogicalWidth, kLogicalHeight, overlay);

    auto root = mountAndLayout(ds, rootWidget, kLogicalWidth, kLogicalHeight);
    if (!root) return false;

    // The background PNG has semi-transparent (grain-textured) pixels, so
    // whatever sits behind it bleeds through. A real device's screenshot
    // shows black behind those pixels regardless of the app's light/dark
    // theme (that's just the screen framebuffer, not app-drawn content) —
    // confirmed by sampling a real-captured reference at a translucent
    // background pixel and solving for the backing color: it matched
    // black exactly, not the theme-conditional white this path previously
    // used for light themes (which was correct for the *other*, non-
    // dialog builders' plain-color clear background, but wrong here).
    auto outPath = outDir / (fileName(c) + ".png");
    return cwt::captureRenderBoxToPng(root, kPhysicalWidth, kPhysicalHeight,
                                      kDevicePixelRatio, Color::black(), outPath.string());
}

static bool renderSearchFieldCase(const cw::DesignSystem& ds, const Case& c, const std::filesystem::path& outDir)
{
    using namespace cw;

    auto fieldWidget = buildWidget(ds, c.builder, c.state);
    if (!fieldWidget) {
        std::cerr << "Unknown builder: " << c.builder << "\n";
        return false;
    }
    // buildWidget()'s "searchField" case intentionally returns an
    // unconstrained-width field (matching real M3's edge-to-edge search
    // field) since it's shared with the Android path. iOS's own reference
    // capture was measured/validated at a fixed 280pt presentation width,
    // so apply that here rather than in the shared builder.
    fieldWidget = SizedBox::from_width(280.0f, fieldWidget);

    WidgetRef background;
    auto tex = loadBackgroundTexture();
    if (tex) {
        auto bgImage = std::make_shared<Image>();
        bgImage->texture = tex;
        bgImage->fit = BoxFit::fill;
        background = std::make_shared<SizedBox>(kLogicalWidth, kLogicalHeight, bgImage);
    }

    // Not modal — screen furniture pinned near the top of the safe area,
    // not an overlay, so no dimming barrier. Matches RealCapture.swift's
    // addSearchField(): centerX of the safe area, 8pt below its top,
    // width 280.
    auto topAligned = std::make_shared<Align>(Alignment::topCenter());
    topAligned->child = fieldWidget;

    auto fieldPadding = std::make_shared<Padding>();
    fieldPadding->padding = EdgeInsets::only(0.0f, 8.0f, 0.0f, 0.0f);
    fieldPadding->child   = topAligned;

    // Same safe-area reasoning as renderDialogCase()/renderActionSheetCase().
    auto safeAreaPadding = std::make_shared<Padding>();
    safeAreaPadding->padding = EdgeInsets::only(0.0f, 62.0f, 0.0f, 34.0f);
    safeAreaPadding->child   = fieldPadding;

    auto overlay = std::make_shared<Overlay>();
    if (background) {
        overlay->initial_entries.push_back(OverlayEntry::create(background));
    }
    overlay->initial_entries.push_back(OverlayEntry::create(safeAreaPadding));

    auto rootWidget = std::make_shared<SizedBox>(kLogicalWidth, kLogicalHeight, overlay);

    auto root = mountAndLayout(ds, rootWidget, kLogicalWidth, kLogicalHeight);
    if (!root) return false;

    auto outPath = outDir / (fileName(c) + ".png");
    return cwt::captureRenderBoxToPng(root, kPhysicalWidth, kPhysicalHeight,
                                      kDevicePixelRatio, Color::black(), outPath.string());
}

// navigationRail's only real ground truth is an iPad UISplitViewController
// sidebar (see RealCapture.swift's addNavigationRail) — a completely
// different device class/canvas size from every other iOS builder, so
// this renders directly at the sidebar's own measured crop dimensions
// (kIpadRailWidth*Physical x kIpadPhysicalHeight-insets) rather than
// kLogicalWidth/kPhysicalWidth's iPhone canvas, matching the real capture
// exactly with no further size-reconciliation needed in
// compare_ios_cpp.py. No shared backdrop image — the real sidebar's own
// column has no such backdrop behind it either (it's the split view's
// plain system background), so the capture's own clear color already
// matches buildNavigationRail()'s own decoration fill.
static bool renderNavigationRailCase(const cw::DesignSystem& ds, const Case& c, const std::filesystem::path& outDir)
{
    using namespace cw;

    auto railWidget = buildWidget(ds, c.builder, c.state);
    if (!railWidget) {
        std::cerr << "Unknown builder: " << c.builder << "\n";
        return false;
    }

    const float railWidthPhysical = (c.state == "compact")
        ? kIpadRailWidthCompactPhysical : kIpadRailWidthExtendedPhysical;
    // Real safe-area insets differ by OS version on the same iPad
    // hardware (cupertino themes run on iOS 18.6, liquid_glass on iOS 26).
    const float topInsetPhysical = isLiquidGlass(c.theme)
        ? kIpadTopInsetLiquidGlassPhysical : kIpadTopInsetCupertinoPhysical;
    const float bottomInsetPhysical = isLiquidGlass(c.theme)
        ? kIpadBottomInsetLiquidGlassPhysical : kIpadBottomInsetCupertinoPhysical;
    const float physicalHeight = kIpadPhysicalHeight - topInsetPhysical - bottomInsetPhysical;
    const float logicalWidth  = railWidthPhysical / kIpadDevicePixelRatio;
    const float logicalHeight = physicalHeight / kIpadDevicePixelRatio;

    // buildNavigationRail()'s outer DecoratedBox/BackdropFilter otherwise
    // shrink-wraps to just its own content height — invisible as long as
    // the canvas's own clear color matched the rail's fill color exactly
    // (true before the backdrop image below was added), but a real bug:
    // a real sidebar's fill/glass material covers the *entire* column
    // height, not just the portion behind its icons/labels. Forcing a
    // tight height here (same fix pattern as Android's own
    // navigationRail full-height fix) makes the Column's own top-packed
    // MainAxisAlignment naturally position content at the top with the
    // fill/glass extending through the leftover space below.
    auto sizedRail = std::make_shared<SizedBox>(logicalWidth, logicalHeight, railWidget);

    // liquidGlass's buildNavigationRail() branch wraps the rail in a
    // BackdropFilter — it needs actual colorful content behind it to
    // blur, matching a real capture where the glass sidebar visibly
    // shows the shared backdrop bleeding through. Harmless for the
    // non-glass cupertino themes too, since their opaque DecoratedBox
    // fully occludes it.
    WidgetRef content = sizedRail;
    auto bgTex = loadBackgroundTexture();
    if (bgTex) {
        auto bgImage = std::make_shared<Image>();
        bgImage->texture = bgTex;
        bgImage->fit = BoxFit::fill;
        auto background = std::make_shared<SizedBox>(logicalWidth, logicalHeight, bgImage);

        auto stack = std::make_shared<Stack>();
        stack->fit = StackFit::expand;
        stack->children = {background, sizedRail};
        content = stack;
    }

    auto rootWidget = std::make_shared<SizedBox>(logicalWidth, logicalHeight, content);

    auto root = mountAndLayout(ds, rootWidget, logicalWidth, logicalHeight);
    if (!root) return false;

    auto outPath = outDir / (fileName(c) + ".png");
    return cwt::captureRenderBoxToPng(
        root, railWidthPhysical, physicalHeight,
        kIpadDevicePixelRatio, ds.tokens().colors.surface_variant, outPath.string());
}

// Real UITabBar is screen furniture pinned to the literal physical bottom
// edge (extending under the home indicator), unlike dialog/actionSheet
// (modal, safe-area-centered) or searchField (safe-area-top-anchored).
// export_references.sh's real-capture crop (safe_area.txt top/bottom) is
// applied uniformly to both sides after the fact — see
// run_real_capture_case() — so this only needs to match position *before*
// that crop, same as every other real-capture case.
static bool renderNavigationBarCase(const cw::DesignSystem& ds, const Case& c, const std::filesystem::path& outDir)
{
    using namespace cw;

    auto barWidget = buildWidget(ds, c.builder, c.state);
    if (!barWidget) {
        std::cerr << "Unknown builder: " << c.builder << "\n";
        return false;
    }

    WidgetRef background;
    auto tex = loadBackgroundTexture();
    if (tex) {
        auto bgImage = std::make_shared<Image>();
        bgImage->texture = tex;
        bgImage->fit = BoxFit::fill;
        background = std::make_shared<SizedBox>(kLogicalWidth, kLogicalHeight, bgImage);
    }

    // Edge-to-edge width, matching a real UITabBar — buildNavigationBar()'s
    // Row of Expanded items needs a bounded width to mean anything (same
    // "expands to fill whatever width it's given" reasoning as several
    // Android builders in renderAndroidCase()).
    auto sizedBar = SizedBox::from_width(kLogicalWidth, barWidget);

    auto bottomAligned = std::make_shared<Align>(Alignment::bottomCenter());
    bottomAligned->child = sizedBar;

    auto overlay = std::make_shared<Overlay>();
    if (background) {
        overlay->initial_entries.push_back(OverlayEntry::create(background));
    }
    overlay->initial_entries.push_back(OverlayEntry::create(bottomAligned));

    auto rootWidget = std::make_shared<SizedBox>(kLogicalWidth, kLogicalHeight, overlay);

    auto root = mountAndLayout(ds, rootWidget, kLogicalWidth, kLogicalHeight);
    if (!root) return false;

    auto outPath = outDir / (fileName(c) + ".png");
    return cwt::captureRenderBoxToPng(root, kPhysicalWidth, kPhysicalHeight,
                                      kDevicePixelRatio, Color::black(), outPath.string());
}

static bool renderActionSheetCase(const cw::DesignSystem& ds, const Case& c, const std::filesystem::path& outDir)
{
    using namespace cw;

    auto sheetWidget = buildWidget(ds, c.builder, c.state);
    if (!sheetWidget) {
        std::cerr << "Unknown builder: " << c.builder << "\n";
        return false;
    }

    WidgetRef background;
    auto tex = loadBackgroundTexture();
    if (tex) {
        auto bgImage = std::make_shared<Image>();
        bgImage->texture = tex;
        bgImage->fit = BoxFit::fill;
        background = std::make_shared<SizedBox>(kLogicalWidth, kLogicalHeight, bgImage);
    }

    // Classic action sheets sit near the bottom edge with a tight ~8pt
    // margin (edge-to-edge sheet feel) — see RealCapture.swift's
    // presentActionSheet(), a genuine UIAlertController(.actionSheet).
    // Liquid Glass redesigned this: confirmed against a real device
    // screenshot that iOS 26 presents the action sheet *centered*, like a
    // floating dialog card (~41pt margin, matching
    // buildConfirmationDialog()'s), not bottom-anchored — the same
    // "centered glass card" treatment iOS 26 gives other sheet-like
    // surfaces, not just a material swap.
    const bool glass = isLiquidGlass(c.theme);
    auto sheetPadding = std::make_shared<Padding>();
    sheetPadding->padding = EdgeInsets::all(glass ? 41.0f : 8.0f);
    sheetPadding->child   = sheetWidget;

    auto bottomAligned = std::make_shared<Align>(glass ? Alignment::center() : Alignment::bottomCenter());
    bottomAligned->child = sheetPadding;

    // Same safe-area reasoning as renderDialogCase(): export_references.sh
    // crops the status bar/home indicator off the real capture, so this
    // must be constrained to the same excluded sub-region for the
    // comparison to be fair (outer padding, shrinking the incoming
    // constraint — not padding around the sheet itself).
    auto safeAreaPadding = std::make_shared<Padding>();
    safeAreaPadding->padding = EdgeInsets::only(0.0f, 62.0f, 0.0f, 34.0f);
    safeAreaPadding->child   = bottomAligned;

    // Same light/dark dimming-scrim calibration as renderDialogCase().
    const float scrimOpacity = isDarkTheme(c.theme) ? 0.48f : 0.20f;
    auto barrierAndSheet = std::make_shared<Stack>();
    barrierAndSheet->fit = StackFit::expand;
    barrierAndSheet->children.push_back(ModalBarrier::create(Color::fromRGBA(0.0f, 0.0f, 0.0f, scrimOpacity), false, nullptr));
    barrierAndSheet->children.push_back(safeAreaPadding);

    auto overlay = std::make_shared<Overlay>();
    if (background) {
        overlay->initial_entries.push_back(OverlayEntry::create(background));
    }
    overlay->initial_entries.push_back(OverlayEntry::create(barrierAndSheet));

    auto rootWidget = std::make_shared<SizedBox>(kLogicalWidth, kLogicalHeight, overlay);

    auto root = mountAndLayout(ds, rootWidget, kLogicalWidth, kLogicalHeight);
    if (!root) return false;

    auto outPath = outDir / (fileName(c) + ".png");
    return cwt::captureRenderBoxToPng(root, kPhysicalWidth, kPhysicalHeight,
                                      kDevicePixelRatio, Color::black(), outPath.string());
}

static bool renderAndroidCase(const cw::DesignSystem& ds, const Case& c, const std::filesystem::path& outDir)
{
    using namespace cw;

    auto widget = buildWidget(ds, c.builder, c.state);
    if (!widget) {
        std::cerr << "Unknown builder: " << c.builder << "\n";
        return false;
    }

    // These expand to fill whatever width they're given (slider; divider's
    // Container has no intrinsic width; listTile's text column sits in a
    // Row's Expanded, which needs a bounded parent width to mean anything)
    // — like textField/segmentedButton, which the iOS path wraps
    // explicitly too (see buildWidget()'s "textField"/"segmentedButton"
    // cases) — but Center() hands them only a *loose* max-411dp
    // constraint with no minimum, so left unwrapped they collapse instead
    // of expanding. Fixed here rather than in buildWidget() itself, which
    // is shared with the iOS path and already works there without this.
    // Same "expands to fill whatever width it's given" issue as slider/
    // divider/listTile/tabBar above — but for "dialog" specifically, the
    // width isn't arbitrary: 280dp is Compose AlertDialog's own real
    // sizing, confirmed by measuring a real capture's card width (734px
    // physical / 2.625 DPR = 280dp exactly). buildDialog()'s own
    // max_width=560 never actually constrains anything here since it's
    // wider than the whole screen.
    if (c.builder == "slider" || c.builder == "divider" || c.builder == "listTile" ||
        c.builder == "tabBar" || c.builder == "dialog" || c.builder == "expansionTile") {
        widget = SizedBox::from_width(280.0f, widget);
    }
    // Real M3 TopAppBar/NavigationBar/ModalBottomSheet/actionSheet's sheet/
    // banner/DataTable are all edge-to-edge, unlike the fixed-280dp
    // components above — confirmed for DataTable against a real capture
    // showing it spans the full screen width with no card/surface backing
    // at all (backdrop visible straight through), unlike listTile/dialog's
    // own bounded-card presentation.
    if (c.builder == "appBar" || c.builder == "navigationBar" || c.builder == "bottomSheet" ||
        c.builder == "banner" || c.builder == "actionSheet" || c.builder == "dataTable" ||
        c.builder == "searchField") {
        widget = SizedBox::from_width(kAndroidLogicalWidth, widget);
    }
    // Real Compose TopAppBar reserves space for the status bar inset above
    // its own ~64dp content row via its default windowInsets parameter —
    // it does this even though this reference app hides the system bars,
    // since the inset is still logically reported. Confirmed by measuring
    // a real capture's bar: ~305px physical tall, matching
    // kAndroidStatusBarLogicalHeight's 136px inset plus a ~168px content
    // row almost exactly. Match it with a same-colored strip above the
    // widget (buildAppBar() itself shouldn't bake this in — reserving
    // status-bar space is a Scaffold/host concern in production, not the
    // app bar widget's own).
    if (c.builder == "appBar") {
        auto statusStrip = std::make_shared<Container>();
        statusStrip->color = ds.tokens().colors.surface;
        auto sizedStrip = std::make_shared<SizedBox>(kAndroidLogicalWidth, kAndroidStatusBarLogicalHeight, statusStrip);
        auto col = std::make_shared<Column>();
        col->main_axis_size = MainAxisSize::min;
        col->children = {sizedStrip, widget};
        widget = col;
    }
    // Real M3 NavigationRail is full-height screen furniture (a vertical
    // side rail whose items pack toward the top), unlike the shrink-
    // wrapped treatment every other intrinsically-sized builder gets —
    // buildNavigationRail() itself just returns a MainAxisSize::min
    // Column (same "doesn't dictate its own screen-filling behavior"
    // convention as buildAppBar()/buildNavigationBar()), so the harness
    // has to supply the full-height constraint. The android reference
    // app's own case host also just Center()s every case with no edge
    // docking, so only the height was actually wrong here — horizontal
    // centering below is already correct.
    if (c.builder == "navigationRail") {
        widget = SizedBox::from_height(kAndroidLogicalHeight, widget);
    }

    // PopupMenuButton's real menu is overlay-driven (opened via a runtime
    // tap, positioned relative to the trigger's live on-screen offset) —
    // the same overlay-anchor machinery that's kept both this and
    // dropdownButton's "open" state out of automated capture all session.
    // But for a static snapshot, a real anchored menu directly below-left
    // of its trigger with nothing else on screen looks identical to just
    // stacking [trigger, menu] in a Column — DropdownMenu's default
    // position is exactly that, no runtime anchor lookup needed. Building
    // the menu panel by hand here (mirroring PopupMenuButtonState::open()'s
    // private construction, which isn't reusable outside a live tap).
    if (c.builder == "popupMenuButton" && c.state == "open") {
        const auto& colors = ds.tokens().colors;
        constexpr float kItemRowHeight    = 48.0f;
        constexpr float kMenuOuterPadding = 16.0f; // 8dp top + 8dp bottom
        constexpr int   kItemCount        = 2;
        std::vector<WidgetRef> item_widgets;
        for (const std::string& label : {std::string("One"), std::string("Two")}) {
            auto padded = std::make_shared<Padding>();
            padded->padding = EdgeInsets::symmetric(12.0f, 0.0f);
            padded->child   = std::make_shared<Text>(
                label, TextStyle{}.withFontSize(14.0f).withColor(colors.on_surface));

            // 48dp row height + width_factor=1.0 shrink-wrap — see the
            // identical fix (and its full rationale) just applied to
            // PopupMenuButtonState::open()'s own item construction in
            // src/widgets/popup_menu_button.cpp.
            auto align = std::make_shared<Align>();
            align->alignment    = Alignment::centerLeft();
            align->width_factor = 1.0f;
            align->child        = padded;

            auto row = std::make_shared<ConstrainedBox>();
            row->additional_constraints =
                BoxConstraints{0.0f, std::numeric_limits<float>::infinity(), kItemRowHeight, kItemRowHeight};
            row->child = align;

            item_widgets.push_back(row);
        }
        auto itemsCol = std::make_shared<Column>();
        itemsCol->main_axis_size       = MainAxisSize::min;
        // start, not stretch — see the identical fix (and its full
        // rationale) just applied to
        // PopupMenuButtonState::open()'s own item Column in
        // src/widgets/popup_menu_button.cpp.
        itemsCol->cross_axis_alignment = CrossAxisAlignment::start;
        itemsCol->children             = std::move(item_widgets);

        // Real M3 DropdownMenu uses surfaceContainer (not plain surface)
        // and enforces a 112dp minimum width — see the identical fix (and
        // its full rationale, including the exact real-capture-sampled hex
        // values) just applied to MaterialDesignSystem::buildPopupMenuButton()
        // in campello_material/src/material_design_system.cpp.
        const bool  dark = isDarkTheme(c.theme);
        const Color surfaceContainer = dark ? Color::fromARGB(0xFF211F26) : Color::fromARGB(0xFFF3EDF7);

        // Real M3 DropdownMenu's own container adds vertical content
        // padding around the item list (measured against a real capture:
        // the whole panel ran ~15dp taller than 2×48dp items alone would
        // account for) — matches Compose's default 8dp top/bottom.
        auto itemsPadded = std::make_shared<Padding>();
        itemsPadded->padding = EdgeInsets::symmetric(0.0f, kMenuOuterPadding / 2.0f);
        itemsPadded->child   = itemsCol;

        const float elevation = ds.tokens().elevation.level2;
        BoxDecoration menuDeco;
        menuDeco.color         = surfaceContainer;
        menuDeco.border_radius = ds.tokens().shape.radius_xs;
        menuDeco.box_shadow    = {BoxShadow{
            Color::fromRGBA(0.0f, 0.0f, 0.0f, 0.15f),
            Offset{0.0f, elevation * 0.5f},
            elevation * 2.0f
        }};
        auto menuDecorated = std::make_shared<DecoratedBox>();
        menuDecorated->decoration = menuDeco;
        menuDecorated->child      = itemsPadded;

        auto menuConstrained = std::make_shared<ConstrainedBox>();
        menuConstrained->additional_constraints =
            BoxConstraints{112.0f, std::numeric_limits<float>::infinity(), 0.0f,
                           std::numeric_limits<float>::infinity()};
        menuConstrained->child = menuDecorated;
        WidgetRef menuPanel    = menuConstrained;

        auto column = std::make_shared<Column>();
        column->main_axis_size       = MainAxisSize::min;
        column->cross_axis_alignment = CrossAxisAlignment::start;
        // DropdownMenu is a real Android Popup, positioned in window
        // coordinates that include the (hidden-but-still-reserved) status
        // bar — unlike the trigger, which is positioned by normal Compose
        // layout within the content view and excludes it. The two only
        // share a coordinate frame once flattened into a screenshot, so
        // the popup lands one status-bar-height lower than a naive
        // "right below the trigger" gap would predict — the same root
        // cause as the AlertDialog status-bar centering fix above, just
        // manifesting as an anchor-relative offset here instead of a
        // centering offset. Confirmed against a real capture: the menu's
        // top edge sat exactly kAndroidStatusBarLogicalHeight lower than
        // this reconstruction produced before accounting for it.
        const float gapHeight = 4.0f + kAndroidStatusBarLogicalHeight;

        // Below this block, renderAndroidCase() wraps *this whole widget*
        // in one Center() shared by every builder (including the
        // already-correct "closed" case, at 0.32% diff). Real Android
        // doesn't do that for this case — the trigger is centered on its
        // own (the Popup menu doesn't count towards its parent's measured
        // size at all), so only the trigger's own height should determine
        // where it lands vertically. Simply Column-ing [trigger, gap,
        // menu] and centering the *whole stack* redistributes the added
        // gap+menu height evenly above/below, dragging the trigger itself
        // upward — confirmed by a real capture showing the trigger's own
        // position shift once the status-bar gap fix above was added.
        // Prepending an invisible spacer of exactly half the extra
        // (gap+menu) height counteracts that redistribution: it makes
        // Center() place *this* padding-adjusted block such that the
        // trigger itself lands exactly where Center()-ing the trigger
        // alone would (solve center-of(padding+trigger+gap+menu) for the
        // trigger's absolute position == center-of(trigger) alone).
        const float menuHeight  = kItemCount * kItemRowHeight + kMenuOuterPadding;
        const float counterPad  = (gapHeight + menuHeight) / 2.0f;

        column->children = {SizedBox::from_height(counterPad), widget,
                             SizedBox::from_height(gapHeight), menuPanel};
        widget = column;
    }

    auto centered = std::make_shared<Center>();
    centered->child = widget;

    // Compose's AlertDialog is a real modal — its own Dialog window dims
    // the content behind it, which the real capture's screenshot includes.
    // Match that here so the comparison isn't measuring "dimmed real
    // capture vs undimmed C++ render" — the same class of bug found and
    // fixed for iOS dialog/actionSheet. Opacity confirmed empirically by
    // solving scrimmed-vs-unscrimmed background samples from real
    // captures: ~0.60 for *both* themes (unlike iOS's liquid glass tint,
    // which needed different values per brightness, Android's dialog dim
    // is a fixed window-level system default, not theme-dependent).
    WidgetRef content = centered;
    if (c.builder == "dialog") {
        auto statusBarPad = std::make_shared<Padding>();
        statusBarPad->padding = EdgeInsets::only(0.0f, kAndroidStatusBarLogicalHeight, 0.0f, 0.0f);
        statusBarPad->child   = centered;

        auto barrier = ModalBarrier::create(Color::fromRGBA(0.0f, 0.0f, 0.0f, 0.60f), false, nullptr);
        auto stack = std::make_shared<Stack>();
        stack->fit = StackFit::expand;
        stack->children = {barrier, statusBarPad};
        content = stack;
    } else if (c.builder == "bottomSheet" || c.builder == "actionSheet") {
        // Real ModalBottomSheet is bottom-anchored (edge-to-edge, flush
        // with the physical bottom), not centered like AlertDialog, and
        // dims the content behind it with its own scrim — same class of
        // fix as "dialog" above. 0.32 is an initial estimate (Compose's
        // M3 baseline scrim token); pending confirmation against a real
        // capture, same empirical-calibration approach already used for
        // dialog's 60% value.
        auto bottomAligned = std::make_shared<Align>();
        bottomAligned->alignment = Alignment::bottomCenter();
        bottomAligned->child = widget;

        auto barrier = ModalBarrier::create(Color::fromRGBA(0.0f, 0.0f, 0.0f, 0.32f), false, nullptr);
        auto stack = std::make_shared<Stack>();
        stack->fit = StackFit::expand;
        stack->children = {barrier, bottomAligned};
        content = stack;
    }

    // Same colourful non-flat backdrop as the iOS/Cupertino side (see
    // wrapForViewport()) — a plain colorScheme.background fill hides
    // translucency/scrim/blur differences behind a uniform color, so
    // Android now shares the identical liquid_glass_background.png asset,
    // stretched to the device canvas. android_fidelity_reference's
    // MainActivity paints the same bundled PNG behind its real capture, so
    // both sides of the comparison are stretching the same source image to
    // the same target size (no red-border alignment aid; that convention
    // is iOS-mockup-specific).
    WidgetRef background;
    auto bgTex = loadBackgroundTexture();
    if (bgTex) {
        auto bgImage = std::make_shared<Image>();
        bgImage->texture = bgTex;
        bgImage->fit = BoxFit::fill;
        background = std::make_shared<SizedBox>(kAndroidLogicalWidth, kAndroidLogicalHeight, bgImage);
    } else {
        auto flat = std::make_shared<Container>();
        flat->color = ds.tokens().colors.background;
        background = flat;
    }

    auto backgroundStack = std::make_shared<Stack>();
    backgroundStack->fit = StackFit::expand;
    backgroundStack->children = {background, content};

    auto rootWidget = std::make_shared<SizedBox>(kAndroidLogicalWidth, kAndroidLogicalHeight, backgroundStack);

    auto root = mountAndLayout(ds, rootWidget, kAndroidLogicalWidth, kAndroidLogicalHeight);
    if (!root) return false;

    auto outPath = outDir / (fileName(c) + ".png");
    return cwt::captureRenderBoxToPng(root, kAndroidPhysicalWidth, kAndroidPhysicalHeight,
                                      kAndroidDevicePixelRatio, ds.tokens().colors.background,
                                      outPath.string());
}

// Mirrors renderAndroidCase() exactly: a Windows desktop app has no
// "mockup" convention either (no shared background image, no red alignment
// border — those are iOS-mockup-specific), so windows_fidelity_reference's
// captures are a plain component centered on a solid ds.tokens().colors.
// background fill, same as android_fidelity_reference. Unlike iOS,
// "dialog"/"actionSheet" need no special real-capture framing here either —
// buildWidget() already returns their bare widget (ds.buildDialog()/
// ds.buildActionSheet()) directly, which centers/backgrounds identically to
// every other builder; the iOS-only renderDialogCase()/renderActionSheetCase()
// exist specifically to match a real UIAlertController's full-screen-scrim
// positioning, which windows_fidelity_reference's references don't attempt
// to replicate.
static bool renderFluentCase(const cw::DesignSystem& ds, const Case& c, const std::filesystem::path& outDir)
{
    using namespace cw;

    auto widget = buildWidget(ds, c.builder, c.state);
    if (!widget) {
        std::cerr << "Unknown builder: " << c.builder << "\n";
        return false;
    }

    // Same rationale as renderAndroidCase()'s identical block: slider/
    // divider/listTile expand to fill whatever width they're given, but
    // Center() hands them only a loose max-width constraint with no
    // minimum, so left unwrapped they collapse instead of expanding.
    if (c.builder == "slider" || c.builder == "divider" || c.builder == "listTile") {
        widget = SizedBox::from_width(280.0f, widget);
    }

    // bottomSheet/actionSheet/dataTable/expansionTile/navigationRail/
    // searchField hit a different problem: their outer Column uses
    // CrossAxisAlignment::stretch (matching MaterialDesignSystem's own
    // identical pattern for these same builders) or an Expanded child,
    // which fills to the *maximum* extent of a loose constraint rather
    // than hugging content — fine under wrapForViewport()'s fixed 360pt
    // container (what Cupertino/Material's own dialog/bottomSheet/etc.
    // actually render through), but renderFluentCase()'s bare Center()
    // hands them a loose 0..480x0..360 constraint with no upper bound of
    // its own, so they expand to fill the entire canvas.
    //
    // SizedBox::from_width is the *wrong* fix here — per its own doc
    // comment it fixes width but makes height expand to fill, which is
    // exactly the bug for these Column-based widgets (confirmed: it
    // shrank bottomSheet_partial's width correctly but left it still
    // filling the full canvas height). ConstrainedBox bounds only max
    // width, leaving height unconstrained/loose so Column's
    // MainAxisSize::min can still shrink-wrap its content vertically.
    if (c.builder == "bottomSheet" || c.builder == "actionSheet" || c.builder == "dataTable" ||
        c.builder == "expansionTile" || c.builder == "navigationRail" || c.builder == "searchField") {
        auto constrained = std::make_shared<ConstrainedBox>();
        constrained->additional_constraints = BoxConstraints{0.0f, 280.0f, 0.0f, std::numeric_limits<float>::infinity()};
        constrained->child = widget;
        widget = constrained;
    }

    auto centered = std::make_shared<Center>();
    centered->child = widget;

    auto background = std::make_shared<Container>();
    background->color = ds.tokens().colors.background;
    background->child = centered;

    auto rootWidget = std::make_shared<SizedBox>(kFluentLogicalWidth, kFluentLogicalHeight, background);

    auto root = mountAndLayout(ds, rootWidget, kFluentLogicalWidth, kFluentLogicalHeight);
    if (!root) return false;

    auto outPath = outDir / (fileName(c) + ".png");
    return cwt::captureRenderBoxToPng(root, kFluentPhysicalWidth, kFluentPhysicalHeight,
                                      kFluentDevicePixelRatio, ds.tokens().colors.background,
                                      outPath.string());
}

static bool renderCase(const cw::DesignSystem& ds, const Case& c, const std::filesystem::path& outDir)
{
    if (isAndroidTheme(c.theme)) {
        return renderAndroidCase(ds, c, outDir);
    }
    if (isFluentTheme(c.theme)) {
        return renderFluentCase(ds, c, outDir);
    }
    if (c.builder == "dialog") {
        return renderDialogCase(ds, c, outDir);
    }
    if (c.builder == "actionSheet") {
        return renderActionSheetCase(ds, c, outDir);
    }
    if (c.builder == "searchField") {
        return renderSearchFieldCase(ds, c, outDir);
    }
    if (c.builder == "navigationBar") {
        return renderNavigationBarCase(ds, c, outDir);
    }
    if (c.builder == "navigationRail") {
        return renderNavigationRailCase(ds, c, outDir);
    }

    auto widget = buildWidget(ds, c.builder, c.state);
    if (!widget) {
        std::cerr << "Unknown builder: " << c.builder << "\n";
        return false;
    }

    auto rootWidget = wrapForViewport(ds, widget, c.builder, c.theme);

    auto root = mountAndLayout(ds, rootWidget, kLogicalWidth, kLogicalHeight);
    if (!root) return false;

    auto outPath = outDir / (fileName(c) + ".png");
    // Matches iOS's hosting view.backgroundColor = .systemBackground showing
    // through the shared background PNG's translucent regions (its alpha
    // channel is well below 255 across most of the image, by design).
    const cw::Color clearColor = isDarkTheme(c.theme) ? cw::Color::black() : cw::Color::white();
    // Full production Renderer path (layout + paint + GPU raster), matching
    // exactly how a real platform run loop drives the renderer — this is
    // what makes DPR-correct text sizing and BackdropFilter/liquid-glass
    // rendering work without any manual scaling hacks.
    return cwt::captureRenderBoxToPng(root, kPhysicalWidth, kPhysicalHeight,
                                      kDevicePixelRatio, clearColor, outPath.string());
}

int main(int argc, char* argv[])
{
    // DPR-correct font sizing (RenderText/RenderParagraph) is now handled by
    // cw::Renderer::setDevicePixelRatio() inside captureRenderBoxToPng() —
    // no manual RenderObject::setActiveDevicePixelRatio() call needed here.

    std::filesystem::path outputRoot = std::filesystem::path(cwt::getVisualFidelityDirectory()) / "cpp_output";

    int rendered = 0;
    int failed = 0;

    for (const auto& themeName : themes()) {
        auto ds = makeDesignSystem(themeName);
        if (!ds) continue;

        auto themeDir = outputRoot / themeName;
        std::filesystem::create_directories(themeDir);

        bool isAndroid = isAndroidTheme(themeName);
        auto themeBuilders = isAndroid ? androidBuilders()
                             : isFluentTheme(themeName) ? fluentBuilders()
                             : builders();
        for (const auto& builderName : themeBuilders) {
            for (const auto& state : (isAndroid ? androidBuilderStates(builderName) : builderStates(builderName))) {
                Case c{themeName, builderName, state};
                std::cout << "Rendering " << themeName << "/" << fileName(c) << "\n";
                if (renderCase(*ds, c, themeDir)) {
                    ++rendered;
                } else {
                    std::cerr << "FAILED: " << themeName << "/" << fileName(c) << "\n";
                    ++failed;
                }
            }
        }
    }

    std::cout << "Rendered " << rendered << " C++ component screenshots (" << failed << " failed)\n";
    return failed > 0 ? 1 : 0;
}
