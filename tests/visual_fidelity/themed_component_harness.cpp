// Themed component fidelity harness.
// Renders every Cupertino/Material DesignSystem builder configuration that
// the iOS reference app captures, producing PNGs for pixel diff.

#include <campello_widgets/campello_widgets.hpp>
#include <campello_cupertino/cupertino_design_system.hpp>
#include <campello_material/material_design_system.hpp>
#include "visual_fidelity.hpp"

#include <campello_gpu/device.hpp>
#include <campello_gpu/texture.hpp>
#include <campello_image/image.hpp>

#include <filesystem>
#include <fstream>
#include <iostream>
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
            "expressive_light", "expressive_dark"};
}

static bool isAndroidTheme(const std::string& theme)
{
    return theme == "expressive_light" || theme == "expressive_dark";
}

// Only the builders android_fidelity_reference/ComponentCatalog.kt actually
// implements — extend both sides together as coverage grows, same
// incremental-per-builder discipline used for the iOS dialog work.
static std::vector<std::string> androidBuilders()
{
    return {"button", "switch", "card", "slider", "chip", "divider", "listTile", "textField",
            "segmentedButton"};
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
    return nullptr;
}

static cw::WidgetRef text(const std::string& str, const cw::TextStyle& style = cw::TextStyle{})
{
    return std::make_shared<cw::Text>(str, style);
}

static cw::WidgetRef icon(const std::string& /*name*/)
{
    // TODO: replace with real icon glyph once campello_widgets has an Icon widget.
    return text("★");
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
        cfg.child = text("Card content");
        if (state == "elevated") cfg.priority = CardPriority::elevated;
        if (state == "filled")   cfg.priority = CardPriority::filled;
        if (state == "outlined") cfg.priority = CardPriority::outlined;
        return SizedBox::from_width(240.0f, ds.buildCard(cfg));
    }

    if (builder == "listTile") {
        ListTileConfig cfg;
        cfg.title = text("Title");
        if (state == "two_line")  cfg.subtitle = text("Subtitle");
        if (state == "with_icon") cfg.leading = icon("star");
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
        cfg.leading = icon("chevron.left");
        cfg.actions = { icon("gear") };
        return ds.buildAppBar(cfg);
    }

    if (builder == "navigationBar") {
        NavigationBarConfig cfg;
        cfg.items = {
            {icon("house"), "First"},
            {icon("magnifyingglass"), "Second"},
            {icon("person"), "Third"},
        };
        cfg.selected_index = 0;
        cfg.on_tap = [](int) {};
        return ds.buildNavigationBar(cfg);
    }

    if (builder == "dialog") {
        DialogConfig cfg;
        cfg.title = text("Title");
        cfg.content = text("Message");
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
        cfg.child = ds.buildButton(ButtonConfig{.label = text("Open Menu"), .priority = ButtonPriority::tertiary, .on_pressed = [] {}});
        cfg.on_selected = [](size_t) {};
        return ds.buildPopupMenuButton(cfg);
    }

    if (builder == "dropdownButton") {
        DropdownConfig cfg;
        cfg.items = {{"Option 1", "1"}, {"Option 2", "2"}};
        cfg.hint = "Select";
        cfg.on_changed = [](const std::string&) {};
        return ds.buildDropdownButton(cfg);
    }

    if (builder == "primaryActionButton") {
        PrimaryActionConfig cfg;
        cfg.on_pressed = [] {};
        if (state == "icon") cfg.icon = icon("plus");
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
        cfg.child = text("Sheet content");
        cfg.show_drag_handle = true;
        return ds.buildBottomSheet(cfg);
    }

    if (builder == "badge") {
        BadgeConfig cfg;
        cfg.child = icon("bell");
        if (state == "number") cfg.label = "3";
        return ds.buildBadge(cfg);
    }

    if (builder == "iconButton") {
        IconButtonConfig cfg;
        cfg.icon = icon("heart");
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
        cfg.title = text("Title");
        cfg.actions = {{"Save", [] {}, false}, {"Delete", [] {}, true}};
        cfg.on_cancel = [] {};
        return ds.buildActionSheet(cfg);
    }

    if (builder == "searchField") {
        SearchFieldConfig cfg;
        cfg.placeholder = "Search";
        cfg.on_changed = [](const std::string&) {};
        if (state == "filled") cfg.value = "query";
        return SizedBox::from_width(280.0f, ds.buildSearchField(cfg));
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
        cfg.title = text("Settings");
        cfg.children_content = text("Expanded content goes here.");
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
        cfg.content = text("A banner message");
        return ds.buildBanner(cfg);
    }

    if (builder == "navigationRail") {
        NavigationRailConfig cfg;
        cfg.items = {{icon("house"), "Home"}, {icon("magnifyingglass"), "Search"}, {icon("person"), "Profile"}};
        cfg.extended = (state == "extended");
        cfg.on_tap = [](int) {};
        return ds.buildNavigationRail(cfg);
    }

    if (builder == "dataTable") {
        DataTableConfig cfg;
        cfg.columns = {"Name", "Age"};
        cfg.rows = {{text("Alice"), text("30")}, {text("Bob"), text("25")}};
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
static std::shared_ptr<cw::RenderBox> mountAndLayout(cw::WidgetRef widget, float /*w*/, float /*h*/)
{
    auto element = widget->createElement();
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
    return theme == "cupertino_dark" || theme == "liquid_glass_dark" || theme == "expressive_dark";
}

static cw::WidgetRef wrapForViewport(const cw::DesignSystem& ds, cw::WidgetRef widget,
                                     const std::string& builder,
                                     const std::string& /*theme*/)
{
    using namespace cw;

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

    auto root = mountAndLayout(rootWidget, kLogicalWidth, kLogicalHeight);
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
    // width 280 (already applied by buildWidget()'s SizedBox wrapper).
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

    auto root = mountAndLayout(rootWidget, kLogicalWidth, kLogicalHeight);
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

    auto root = mountAndLayout(rootWidget, kLogicalWidth, kLogicalHeight);
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
    if (c.builder == "slider" || c.builder == "divider" || c.builder == "listTile") {
        widget = SizedBox::from_width(280.0f, widget);
    }

    auto centered = std::make_shared<Center>();
    centered->child = widget;

    // Plain colorScheme.background fill, matching MaterialExpressiveTheme's
    // Surface(color = MaterialTheme.colorScheme.background) in
    // android_fidelity_reference's MainActivity — no shared background
    // image or red-border alignment aid; those are iOS-mockup-specific
    // conventions with no Android equivalent.
    auto background = std::make_shared<Container>();
    background->color = ds.tokens().colors.background;
    background->child = centered;

    auto rootWidget = std::make_shared<SizedBox>(kAndroidLogicalWidth, kAndroidLogicalHeight, background);

    auto root = mountAndLayout(rootWidget, kAndroidLogicalWidth, kAndroidLogicalHeight);
    if (!root) return false;

    auto outPath = outDir / (fileName(c) + ".png");
    return cwt::captureRenderBoxToPng(root, kAndroidPhysicalWidth, kAndroidPhysicalHeight,
                                      kAndroidDevicePixelRatio, ds.tokens().colors.background,
                                      outPath.string());
}

static bool renderCase(const cw::DesignSystem& ds, const Case& c, const std::filesystem::path& outDir)
{
    if (c.builder == "dialog") {
        return renderDialogCase(ds, c, outDir);
    }
    if (c.builder == "actionSheet" && !isAndroidTheme(c.theme)) {
        return renderActionSheetCase(ds, c, outDir);
    }
    if (c.builder == "searchField" && !isAndroidTheme(c.theme)) {
        return renderSearchFieldCase(ds, c, outDir);
    }
    if (isAndroidTheme(c.theme)) {
        return renderAndroidCase(ds, c, outDir);
    }

    auto widget = buildWidget(ds, c.builder, c.state);
    if (!widget) {
        std::cerr << "Unknown builder: " << c.builder << "\n";
        return false;
    }

    auto rootWidget = wrapForViewport(ds, widget, c.builder, c.theme);

    auto root = mountAndLayout(rootWidget, kLogicalWidth, kLogicalHeight);
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

        for (const auto& builderName : (isAndroidTheme(themeName) ? androidBuilders() : builders())) {
            for (const auto& state : builderStates(builderName)) {
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
