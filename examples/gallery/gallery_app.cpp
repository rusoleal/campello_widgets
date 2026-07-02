#include "gallery_app.hpp"
#include "assets/mountains_jpeg.h"
#include <campello_widgets/campello_widgets.hpp>

#include <cmath>
#include <string>
#include <vector>
#include <sstream>
#include <iomanip>

namespace cw = systems::leal::campello_widgets;

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
static const cw::Color kContent = cw::Color::fromRGB(0.94f, 0.94f, 0.96f);

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------
static cw::TextStyle ts(float size, cw::Color color = cw::Color::fromRGB(0.10f, 0.10f, 0.10f))
{
    cw::TextStyle s{};
    s.font_size = size;
    s.color     = color;
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

static cw::WidgetRef card(cw::WidgetRef content, float pad = 16.0f)
{
    cw::BoxDecoration deco;
    deco.color         = cw::Color::white();
    deco.border_radius = 8.0f;
    deco.box_shadow    = { cw::BoxShadow{cw::Color::fromRGBA(0,0,0,0.07f), {0,2}, 6.0f, 0.0f} };
    auto box = std::make_shared<cw::DecoratedBox>(deco);
    box->child = cw::mw<cw::Padding>(cw::EdgeInsets::all(pad), std::move(content));
    return box;
}

static cw::WidgetRef subheading(const std::string& text)
{
    return cw::mw<cw::Padding>(
        cw::EdgeInsets::only(0.0f, 0.0f, 0.0f, 8.0f),
        cw::mw<cw::Text>(text, ts(11.0f, cw::Color::fromRGB(0.5f, 0.5f, 0.55f))));
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
    cw::WidgetRef build(cw::BuildContext&) const override
    {
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
        stack_bg->color = cw::Color::fromRGB(0.94f, 0.94f, 0.97f);
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
                    subheading("WRAP — wraps children into multiple runs"),
                    card(wrap),
                    vspace(20.0f),
                    subheading("STACK + POSITIONED — overlapping layers"),
                    card(stack_bg, 8.0f),
                    vspace(20.0f),
                    subheading("ASPECT RATIO — maintains 16:9 regardless of width"),
                    ar_box,
                }));
        auto bg = std::make_shared<cw::Container>();
        bg->color = kContent;
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
    }

    cw::WidgetRef build(cw::BuildContext&) override
    {
        auto mkCb = [this](const std::string& lbl, bool val, std::function<void(bool)> fn) {
            auto cb = std::make_shared<cw::Checkbox>();
            cb->value      = val;
            cb->on_changed = std::move(fn);
            return cw::mw<cw::Row>(cw::MainAxisAlignment::start, cw::CrossAxisAlignment::center,
                cw::WidgetList{ cb, hspace(8.0f), cw::mw<cw::Text>(lbl, ts(14.0f)) });
        };

        auto mkSw = [this](const std::string& lbl, bool val, std::function<void(bool)> fn) {
            auto sw = std::make_shared<cw::Switch>();
            sw->value      = val;
            sw->on_changed = std::move(fn);
            return cw::mw<cw::Row>(cw::MainAxisAlignment::start, cw::CrossAxisAlignment::center,
                cw::WidgetList{ sw, hspace(10.0f), cw::mw<cw::Text>(lbl, ts(14.0f)) });
        };

        // Slider
        auto sl = std::make_shared<cw::Slider>();
        sl->value = slider_; sl->min = 0.0f; sl->max = 1.0f;
        sl->on_changed = [this](float v) { setState([this, v] { slider_ = v; }); };
        std::ostringstream oss; oss << std::fixed << std::setprecision(2) << slider_;
        auto slider_row = cw::mw<cw::Row>(cw::MainAxisAlignment::start, cw::CrossAxisAlignment::center,
            cw::WidgetList{
                cw::mw<cw::Expanded>(sl),
                hspace(12.0f),
                cw::mw<cw::Text>(oss.str(), ts(13.0f, kBlue)),
            });

        // RadioGroup
        auto mkRadio = [](int val, const std::string& lbl) -> cw::WidgetRef {
            return cw::mw<cw::Row>(cw::MainAxisAlignment::start, cw::CrossAxisAlignment::center,
                cw::WidgetList{
                    std::make_shared<cw::Radio>(val),
                    hspace(6.0f),
                    cw::mw<cw::Text>(lbl, ts(14.0f)),
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
        using DD = cw::DropdownButton<std::string>;
        using Item = cw::DropdownMenuItem<std::string>;
        auto dd = std::make_shared<DD>();
        dd->value = dd_val_;
        dd->hint  = "Select fruit…";
        dd->items = {
            Item{"Apple",      cw::mw<cw::Text>("Apple",      ts(14.0f))},
            Item{"Banana",     cw::mw<cw::Text>("Banana",     ts(14.0f))},
            Item{"Cherry",     cw::mw<cw::Text>("Cherry",     ts(14.0f))},
            Item{"Dragonfruit",cw::mw<cw::Text>("Dragonfruit",ts(14.0f))},
            Item{"Elderberry", cw::mw<cw::Text>("Elderberry", ts(14.0f))},
        };
        dd->on_changed = [this](std::string v) { setState([this, v] { dd_val_ = v; }); };
        auto dd_width = std::make_shared<cw::ConstrainedBox>();
        dd_width->additional_constraints = cw::BoxConstraints{0.0f, 240.0f, 0.0f, 9999.0f};
        dd_width->child = dd;

        auto scroll = std::make_shared<cw::SingleChildScrollView>();
        scroll->physics = std::make_shared<cw::BouncingScrollPhysics>();
        scroll->child   = cw::mw<cw::Padding>(cw::EdgeInsets::all(20.0f),
            cw::mw<cw::Column>(cw::MainAxisAlignment::start, cw::CrossAxisAlignment::start,
                cw::WidgetList{
                    subheading("CHECKBOX"),
                    card(cw::mw<cw::Column>(cw::MainAxisAlignment::start, cw::CrossAxisAlignment::start,
                        cw::WidgetList{
                            mkCb("Enable feature A", cb_a_, [this](bool v){setState([this,v]{cb_a_=v;});}),
                            vspace(8.0f),
                            mkCb("Enable feature B", cb_b_, [this](bool v){setState([this,v]{cb_b_=v;});}),
                            vspace(8.0f),
                            mkCb("Enable feature C", cb_c_, [this](bool v){setState([this,v]{cb_c_=v;});}),
                        })),
                    vspace(20.0f),
                    subheading("SWITCH"),
                    card(cw::mw<cw::Column>(cw::MainAxisAlignment::start, cw::CrossAxisAlignment::start,
                        cw::WidgetList{
                            mkSw("Notifications", sw_a_, [this](bool v){setState([this,v]{sw_a_=v;});}),
                            vspace(10.0f),
                            mkSw("Dark mode", sw_b_, [this](bool v){setState([this,v]{sw_b_=v;});}),
                        })),
                    vspace(20.0f),
                    subheading("SLIDER"),
                    card(slider_row),
                    vspace(20.0f),
                    subheading("RADIO GROUP"),
                    card(rg),
                    vspace(20.0f),
                    subheading("DROPDOWN BUTTON (uses Overlay)"),
                    dd_width,
                    vspace(20.0f),
                }));

        auto bg = std::make_shared<cw::Container>();
        bg->color = kContent;
        bg->child = scroll;
        return bg;
    }

private:
    bool  cb_a_ = true, cb_b_ = false, cb_c_ = true;
    bool  sw_a_ = true, sw_b_ = false;
    float slider_ = 0.6f;
    int   radio_  = 1;
    std::string dd_val_ = "Banana";
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

    cw::WidgetRef build(cw::BuildContext&) override
    {
        // Size showcase
        std::vector<cw::WidgetRef> sizes;
        for (float sz : {10.0f, 12.0f, 14.0f, 18.0f, 24.0f, 32.0f, 48.0f}) {
            std::ostringstream o; o << sz << "px — The quick brown fox";
            sizes.push_back(cw::mw<cw::Text>(o.str(), ts(sz)));
        }

        // Weight / style showcase
        auto bold_style = ts(16.0f); bold_style.font_weight = cw::FontWeight::bold;
        auto italic_style = ts(16.0f); italic_style.italic = true;
        auto combo_style  = ts(16.0f);
        combo_style.font_weight = cw::FontWeight::bold; combo_style.italic = true;
        auto weights = cw::mw<cw::Column>(cw::MainAxisAlignment::start, cw::CrossAxisAlignment::start,
            cw::WidgetList{
                cw::mw<cw::Text>("Regular weight", ts(16.0f)),
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
            cw::InlineTextSpan::create("Rich text with ", ts(15.0f)),
            cw::InlineTextSpan::create("mixed", ts(15.0f, kBlue)),
            cw::InlineTextSpan::create(" colors and ", ts(15.0f)),
            cw::InlineTextSpan::create("bold", bold_span_style),
            cw::InlineTextSpan::create(" inline spans.", ts(15.0f)),
        };
        auto rich = cw::RichText::create(spans);

        // Single-line TextField
        auto tf = std::make_shared<cw::TextField>();
        tf->controller  = ctrl_;
        tf->placeholder = "Type something…";
        tf->style       = ts(15.0f);

        auto scroll = std::make_shared<cw::SingleChildScrollView>();
        scroll->physics = std::make_shared<cw::BouncingScrollPhysics>();
        scroll->child   = cw::mw<cw::Padding>(cw::EdgeInsets::all(20.0f),
            cw::mw<cw::Column>(cw::MainAxisAlignment::start, cw::CrossAxisAlignment::start,
                cw::WidgetList{
                    subheading("TEXT SIZES"),
                    card(cw::mw<cw::Column>(cw::MainAxisAlignment::start,
                        cw::CrossAxisAlignment::start, sizes)),
                    vspace(20.0f),
                    subheading("FONT WEIGHT & STYLE"),
                    card(weights),
                    vspace(20.0f),
                    subheading("RICH TEXT"),
                    card(rich),
                    vspace(20.0f),
                    subheading("TEXT FIELD"),
                    tf,
                    vspace(20.0f),
                }));

        auto bg = std::make_shared<cw::Container>();
        bg->color = kContent;
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

    cw::WidgetRef build(cw::BuildContext&) override
    {
        auto mkTab = [this](const std::string& label, int idx) -> cw::WidgetRef {
            const bool active = (tab_ == idx);
            cw::BoxDecoration d;
            d.color = active ? kBlue : cw::Color::fromRGB(0.92f, 0.92f, 0.95f);
            d.border_radius = 6.0f;
            auto box = std::make_shared<cw::DecoratedBox>(d);
            box->child = cw::mw<cw::Padding>(cw::EdgeInsets::symmetric(16.0f, 8.0f),
                cw::mw<cw::Text>(label, ts(13.0f, active ? cw::Color::white()
                    : cw::Color::fromRGB(0.4f, 0.4f, 0.45f))));
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
            lv->builder = [this](cw::BuildContext&, int i) -> cw::WidgetRef {
                const bool sel = (i == selected_);
                const cw::Color colors[] = { kBlue, kGreen, kOrange, kPurple, kTeal };
                cw::Color av_color = colors[i % 5];

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
                                    ts(14.0f, sel ? kBlue : cw::Color::fromRGB(0.1f,0.1f,0.1f))),
                                vspace(2.0f),
                                cw::mw<cw::Text>("Scroll me — row " + std::to_string(i + 1),
                                    ts(11.0f, cw::Color::fromRGB(0.55f,0.55f,0.6f))),
                            }),
                    });

                auto cell = std::make_shared<cw::Container>();
                cell->padding = cw::EdgeInsets::symmetric(16.0f, 0.0f);
                cell->color   = sel ? cw::Color::fromRGB(0.88f, 0.94f, 1.0f)
                                    : (i % 2 ? cw::Color::fromRGB(0.97f,0.97f,0.99f) : cw::Color::white());
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
            gv->builder = [this](cw::BuildContext&, int i) -> cw::WidgetRef {
                const cw::Color colors[] = { kBlue, kGreen, kOrange, kPurple, kTeal, kRed, kAmber };
                cw::Color c = colors[i % 7];
                const bool sel = (i == selected_);
                auto fill = std::make_shared<cw::Container>();
                fill->color = sel ? cw::Color::fromRGB(0.88f,0.94f,1.0f) : c;
                fill->child = cw::mw<cw::Center>(
                    cw::mw<cw::Text>(std::to_string(i + 1),
                        ts(16.0f, sel ? kBlue : cw::Color::white())));
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
        bg->color = kContent;
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

    cw::WidgetRef build(cw::BuildContext&) override
    {
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
        aa_box->color = cw::Color::fromRGB(0.93f, 0.93f, 0.97f);
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
                    subheading("ANIMATED SWITCHER — tap to swap widgets with cross-fade"),
                    card(cw::mw<cw::Column>(cw::MainAxisAlignment::start, cw::CrossAxisAlignment::start,
                        cw::WidgetList{
                            switcher,
                            vspace(10.0f),
                            tapBtn("Swap", kPurple, [this] { setState([this] { show_a_ = !show_a_; }); }),
                        })),
                    vspace(20.0f),
                    subheading("ANIMATED ALIGN — tap to cycle corners"),
                    card(cw::mw<cw::Column>(cw::MainAxisAlignment::start, cw::CrossAxisAlignment::start,
                        cw::WidgetList{
                            aa_box,
                            vspace(10.0f),
                            tapBtn("Next corner", kTeal, [this] {
                                setState([this] { align_idx_ = (align_idx_ + 1) % 4; });
                            }),
                        })),
                    vspace(20.0f),
                    subheading("EXPLICIT TRANSITIONS — Rotate / Fade / Scale"),
                    card(anim_builder),
                }));

        auto bg = std::make_shared<cw::Container>();
        bg->color = kContent;
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
        zone_color_ = cw::Color::fromRGB(0.90f, 0.90f, 0.93f);
        text_color_ = cw::Color::fromRGB(0.40f, 0.40f, 0.45f);
        dropped_   = 0;
    }

    cw::WidgetRef build(cw::BuildContext&) override
    {
        // GestureDetector zone
        auto zone_center = cw::mw<cw::Column>();
        {
            auto col = std::static_pointer_cast<cw::Column>(zone_center);
            col->main_axis_alignment  = cw::MainAxisAlignment::center;
            col->cross_axis_alignment = cw::CrossAxisAlignment::center;
            col->main_axis_size       = cw::MainAxisSize::min;
            col->children = {
                cw::mw<cw::Text>(gesture_, ts(28.0f, text_color_)),
                vspace(6.0f),
                cw::mw<cw::Text>(detail_, ts(12.0f, cw::Color::fromRGB(0.85f,0.85f,0.90f))),
            };
        }
        auto zone_bg = std::make_shared<cw::Container>();
        zone_bg->color  = zone_color_;
        zone_bg->child  = cw::mw<cw::Center>(zone_center);

        auto detector = std::make_shared<cw::GestureDetector>();
        detector->child = zone_bg;
        detector->on_tap = [this] {
            setState([this] {
                gesture_="tap"; detail_=""; zone_color_=kBlue; text_color_=cw::Color::white();
            });
        };
        detector->on_double_tap = [this] {
            setState([this] {
                gesture_="double tap"; detail_=""; zone_color_=kGreen; text_color_=cw::Color::white();
            });
        };
        detector->on_long_press = [this] {
            setState([this] {
                gesture_="long press"; detail_=""; zone_color_=kOrange; text_color_=cw::Color::white();
            });
        };
        detector->on_pan_update = [this](cw::Offset d) {
            std::ostringstream o;
            o << std::fixed << std::setprecision(1) << "Δ " << d.x << ", " << d.y;
            setState([this, s = o.str()] {
                gesture_="pan"; detail_=s; zone_color_=kPurple; text_color_=cw::Color::white();
            });
        };
        detector->on_pan_end = [this] {
            setState([this] {
                gesture_="pan end"; detail_=""; zone_color_=cw::Color::fromRGB(0.60f,0.40f,0.85f);
                text_color_=cw::Color::white();
            });
        };
        detector->on_scroll = [this](cw::Offset d) {
            std::ostringstream o;
            o << std::fixed << std::setprecision(1) << "Δ " << d.x << ", " << d.y;
            setState([this, s = o.str()] {
                gesture_="scroll"; detail_=s; zone_color_=kTeal; text_color_=cw::Color::white();
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
        target_widget->builder = [dropped_count](cw::BuildContext&, bool hovering) -> cw::WidgetRef {
            auto box = std::make_shared<cw::Container>();
            box->width = 160.0f; box->height = 80.0f;
            box->color = hovering ? kGreen : cw::Color::fromRGB(0.88f, 0.88f, 0.92f);
            const std::string label = dropped_count > 0
                ? "dropped " + std::to_string(dropped_count) + "x"
                : (hovering ? "release!" : "drop here");
            box->child = cw::mw<cw::Center>(
                cw::mw<cw::Text>(label, ts(13.0f,
                    hovering || dropped_count > 0 ? cw::Color::white()
                        : cw::Color::fromRGB(0.5f, 0.5f, 0.55f))));
            return std::make_shared<cw::ClipRRect>(12.0f, box);
        };
        target_widget->on_accept = [this](const int&) {
            setState([this] { dropped_++; });
        };

        auto dnd_row = cw::mw<cw::Row>(
            cw::MainAxisAlignment::start, cw::CrossAxisAlignment::center,
            cw::WidgetList{ drag, hspace(24.0f),
                cw::mw<cw::Text>("→", ts(20.0f, cw::Color::fromRGB(0.6f,0.6f,0.6f))),
                hspace(24.0f), target_widget });

        auto content = cw::mw<cw::Padding>(cw::EdgeInsets::all(20.0f),
            cw::mw<cw::Column>(cw::MainAxisAlignment::start, cw::CrossAxisAlignment::stretch,
                cw::WidgetList{
                    subheading("GESTURE DETECTOR — tap · double-tap · long-press · pan · scroll"),
                    cw::mw<cw::Expanded>(card(detector, 0.0f)),
                    vspace(20.0f),
                    subheading("DRAGGABLE + DRAG TARGET"),
                    card(dnd_row),
                }));

        auto bg = std::make_shared<cw::Container>();
        bg->color = kContent;
        bg->child = content;
        return bg;
    }

private:
    std::string gesture_, detail_;
    cw::Color   zone_color_, text_color_;
    int         dropped_ = 0;
};

class GesturesSection : public cw::StatefulWidget {
public:
    std::unique_ptr<cw::StateBase> createState() const override
    { return std::make_unique<GesturesSectionState>(); }
};

// ---------------------------------------------------------------------------
// 7. CLIPPING & FX — ClipRRect, ClipOval, DecoratedBox, Opacity, BackdropFilter
// ---------------------------------------------------------------------------
class ClippingSection : public cw::StatelessWidget
{
public:
    cw::WidgetRef build(cw::BuildContext&) const override
    {
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
        fancy.color         = cw::Color::white();
        fancy.border_radius = 12.0f;
        fancy.border        = cw::BoxBorder::all(kBlue, 2.0f);
        fancy.box_shadow    = {
            cw::BoxShadow{cw::Color::fromRGBA(0,0,0,0.15f), {0,4}, 12.0f, 0.0f},
            cw::BoxShadow{cw::Color::fromRGBA(0.08f,0.47f,0.95f,0.2f), {0,0}, 8.0f, 2.0f},
        };
        auto fancy_box = std::make_shared<cw::DecoratedBox>(fancy);
        fancy_box->child = cw::mw<cw::Padding>(cw::EdgeInsets::all(20.0f),
            cw::mw<cw::Text>("DecoratedBox\nborder + dual shadow", ts(13.0f, kBlue)));

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
                    cw::mw<cw::Text>(o.str(), ts(10.0f, cw::Color::fromRGB(0.55f,0.55f,0.6f))),
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

        auto scroll = std::make_shared<cw::SingleChildScrollView>();
        scroll->physics = std::make_shared<cw::BouncingScrollPhysics>();
        scroll->child   = cw::mw<cw::Padding>(cw::EdgeInsets::all(20.0f),
            cw::mw<cw::Column>(cw::MainAxisAlignment::start, cw::CrossAxisAlignment::start,
                cw::WidgetList{
                    subheading("CLIP RRECT + CLIP OVAL"),
                    card(clips_row),
                    vspace(20.0f),
                    subheading("DECORATED BOX — border + shadow"),
                    card(fancy_box, 0.0f),
                    vspace(20.0f),
                    subheading("OPACITY — five levels 0.2 → 1.0"),
                    card(opacity_row),
                    vspace(20.0f),
                    subheading("BACKDROP FILTER — frosted glass"),
                    blur_container,
                    vspace(20.0f),
                }));

        auto root = std::make_shared<cw::Container>();
        root->color = kContent;
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

    cw::WidgetRef build(cw::BuildContext&) override
    {
        const std::string key_display = last_key_.empty() ? "press a key" : last_key_;
        const cw::Color key_color = last_kind_ == cw::KeyEventKind::up
            ? cw::Color::fromRGB(0.70f, 0.70f, 0.75f)
            : kBlue;

        const std::string typed_display = typed_.empty() ? "typed text appears here" : typed_;
        const cw::Color typed_color = typed_.empty()
            ? cw::Color::fromRGB(0.75f, 0.75f, 0.78f)
            : cw::Color::fromRGB(0.1f, 0.1f, 0.1f);

        std::vector<cw::WidgetRef> log_items;
        for (auto it = log_.rbegin(); it != log_.rend(); ++it) {
            log_items.push_back(cw::mw<cw::Text>(*it,
                ts(11.0f, cw::Color::fromRGB(0.55f, 0.55f, 0.6f))));
        }
        auto log_col = std::make_shared<cw::Column>();
        log_col->main_axis_alignment  = cw::MainAxisAlignment::start;
        log_col->cross_axis_alignment = cw::CrossAxisAlignment::start;
        log_col->main_axis_size       = cw::MainAxisSize::min;
        log_col->children             = log_items;

        auto center_content = cw::mw<cw::Column>(
            cw::MainAxisAlignment::center, cw::CrossAxisAlignment::center,
            cw::WidgetList{
                cw::mw<cw::Text>("last key event", ts(12.0f, cw::Color::fromRGB(0.55f,0.55f,0.6f))),
                vspace(8.0f),
                cw::mw<cw::Text>(key_display, ts(64.0f, key_color)),
                vspace(4.0f),
                cw::mw<cw::Text>(last_key_.empty() ? "" : kindStr(last_kind_),
                    ts(12.0f, cw::Color::fromRGB(0.55f,0.55f,0.6f))),
                vspace(32.0f),
                cw::mw<cw::Text>("typed", ts(12.0f, cw::Color::fromRGB(0.55f,0.55f,0.6f))),
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
        bg->color = kContent;
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

static cw::WidgetRef fitSample(const std::string& label, cw::BoxFit fit)
{
    auto box = std::make_shared<cw::Container>();
    box->width  = kFitSampleBoxSize;
    box->height = kFitSampleBoxSize;
    box->color  = cw::Color::fromRGB(0.90f, 0.90f, 0.93f);
    box->child  = mountainsImage(fit, kFitSampleBoxSize, kFitSampleBoxSize);
    auto clipped = std::make_shared<cw::ClipRRect>(8.0f, box);

    return cw::mw<cw::Column>(cw::MainAxisAlignment::start, cw::CrossAxisAlignment::center,
        cw::WidgetList{
            clipped,
            vspace(6.0f),
            cw::mw<cw::Text>(label, ts(11.0f, cw::Color::fromRGB(0.4f, 0.4f, 0.45f))),
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

static cw::WidgetRef labeledTransform(const char* label, cw::WidgetRef image, cw::Matrix4 transform)
{
    auto t = std::make_shared<cw::Transform>();
    t->transform = transform;
    t->child = std::make_shared<cw::ClipRRect>(8.0f, std::move(image));

    return cw::mw<cw::Column>(cw::MainAxisAlignment::start, cw::CrossAxisAlignment::center,
        cw::WidgetList{
            t, vspace(6.0f),
            cw::mw<cw::Text>(label, ts(11.0f, cw::Color::fromRGB(0.4f, 0.4f, 0.45f))),
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

    cw::WidgetRef build(cw::BuildContext&) override
    {
        const float angle = static_cast<float>(anim_ctrl_->value());

        return cw::mw<cw::Row>(cw::MainAxisAlignment::start, cw::CrossAxisAlignment::center,
            cw::WidgetList{
                labeledTransform("rotate — X axis", image_x_,
                    perspectiveRotationX(angle)),
                hspace(24.0f),
                labeledTransform("rotate — Y axis", image_y_,
                    perspectiveRotationY(angle + 1.2f)),
                hspace(24.0f),
                labeledTransform("rotate — Z axis", image_z_,
                    cw::RenderTransform::rotation(angle)),
                hspace(24.0f),
                labeledTransform("scale", image_scale_,
                    cw::RenderTransform::scaling(0.65f + 0.15f * std::sin(angle))),
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
    cw::WidgetRef build(cw::BuildContext&) const override
    {
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
        fit_list_view->builder = [](cw::BuildContext&, int i) -> cw::WidgetRef {
            const auto& [label, fit] = kFitModes[static_cast<size_t>(i)];
            return cw::mw<cw::Padding>(cw::EdgeInsets::only(0.0f, 0.0f, 16.0f, 0.0f),
                fitSample(label, fit));
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
                    subheading("BOX FIT — fill · contain · cover · fitWidth · fitHeight · none · scaleDown"),
                    repaintBoundary(card(fit_list)),
                    vspace(20.0f),
                    subheading("DECORATIONS — ClipRRect · ClipOval · border + shadow"),
                    repaintBoundary(card(deco_row)),
                    vspace(20.0f),
                    subheading("TRANSFORM — animated flip (X/Y) · rotation (Z) · scale"),
                    repaintBoundary(card(transform_row)),
                    vspace(20.0f),
                    subheading("BACKDROP FILTER — blur the photo itself"),
                    repaintBoundary(blur_container),
                    vspace(20.0f),
                }));

        auto root = std::make_shared<cw::Container>();
        root->color = kContent;
        root->child = scroll;
        return root;
    }
};

// ---------------------------------------------------------------------------
// Gallery Shell — left sidebar nav + section content
// ---------------------------------------------------------------------------
static const std::vector<std::string> kSectionNames = {
    "Layout", "Controls", "Text & Input", "Lists",
    "Animations", "Gestures", "Clipping & FX", "Keyboard", "Images",
};

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
        default: return std::make_shared<LayoutSection>();
    }
}

class GalleryShell;

class GalleryShellState : public cw::State<GalleryShell>
{
public:
    void initState() override { selected_ = 4; }

    cw::WidgetRef build(cw::BuildContext&) override
    {
        // Left sidebar
        std::vector<cw::WidgetRef> nav_items;
        for (int i = 0; i < (int)kSectionNames.size(); ++i) {
            const bool active = (i == selected_);
            auto label = cw::mw<cw::Text>(kSectionNames[i],
                ts(14.0f, active ? kBlue : cw::Color::fromRGB(0.25f, 0.25f, 0.30f)));
            auto item_container = std::make_shared<cw::Container>();
            item_container->padding = cw::EdgeInsets::symmetric(16.0f, 11.0f);
            item_container->color   = active
                ? cw::Color::fromRGB(0.88f, 0.94f, 1.0f)
                : cw::Color::white();
            item_container->child   = label;

            cw::WidgetRef item_bg = item_container;
            if (active) {
                // Left accent bar as a Row sibling. Use CrossAxisAlignment::center
                // (not stretch) so the Row sizes to content height instead of
                // consuming all remaining column space.
                auto accent = std::make_shared<cw::Container>();
                accent->width = 3.0f; accent->height = 36.0f; accent->color = kBlue;
                item_container->padding = cw::EdgeInsets::only(13.0f, 11.0f, 16.0f, 11.0f);
                auto row = cw::mw<cw::Row>(
                    cw::MainAxisAlignment::start, cw::CrossAxisAlignment::center,
                    cw::WidgetList{ accent, cw::mw<cw::Expanded>(item_container) });
                item_bg = row;
            }

            auto g   = std::make_shared<cw::GestureDetector>();
            g->on_tap = [this, i] { setState([this, i] { selected_ = i; }); };
            g->child  = item_bg;
            nav_items.push_back(ptr(g));
        }

        auto sidebar_title = std::make_shared<cw::Container>();
        sidebar_title->padding = cw::EdgeInsets::symmetric(16.0f, 18.0f);
        sidebar_title->color   = cw::Color::white();
        sidebar_title->child   = cw::mw<cw::Column>(
            cw::MainAxisAlignment::start, cw::CrossAxisAlignment::start, cw::MainAxisSize::min,
            cw::WidgetList{
                cw::mw<cw::Text>("Gallery", ts(18.0f, cw::Color::fromRGB(0.08f, 0.08f, 0.12f))),
                vspace(2.0f),
                cw::mw<cw::Text>("campello_widgets", ts(10.0f, cw::Color::fromRGB(0.55f, 0.55f, 0.60f))),
            });

        auto title_divider = std::make_shared<cw::Container>();
        title_divider->height = 1.0f;
        title_divider->color  = cw::Color::fromRGB(0.90f, 0.90f, 0.93f);

        std::vector<cw::WidgetRef> nav_col_children = { sidebar_title, title_divider };
        nav_col_children.insert(nav_col_children.end(), nav_items.begin(), nav_items.end());

        auto nav_col = cw::mw<cw::Column>(
            cw::MainAxisAlignment::start, cw::CrossAxisAlignment::stretch, nav_col_children);

        cw::BoxDecoration sidebar_deco;
        sidebar_deco.color = cw::Color::white();
        sidebar_deco.box_shadow = {
            cw::BoxShadow{cw::Color::fromRGBA(0,0,0,0.08f), {2,0}, 8.0f, 0.0f}
        };
        auto sidebar_box = std::make_shared<cw::DecoratedBox>(sidebar_deco);
        sidebar_box->child = nav_col;

        auto sidebar = std::make_shared<cw::SizedBox>(200.0f, std::nullopt, sidebar_box);

        // Vertical divider
        auto divider = std::make_shared<cw::Container>();
        divider->width = 1.0f;
        divider->color = cw::Color::fromRGB(0.87f, 0.87f, 0.90f);

        // Right content
        auto content = cw::mw<cw::Expanded>(buildSection(selected_));

        auto root = cw::mw<cw::Row>(
            cw::MainAxisAlignment::start, cw::CrossAxisAlignment::stretch,
            cw::WidgetList{ sidebar, divider, content });

        auto bg = std::make_shared<cw::Container>();
        bg->color = cw::Color::fromRGB(0.97f, 0.97f, 0.99f);
        bg->child = root;
        return bg;
    }

private:
    int selected_ = 0;
};

class GalleryShell : public cw::StatefulWidget {
public:
    std::unique_ptr<cw::StateBase> createState() const override
    { return std::make_unique<GalleryShellState>(); }
};

// ---------------------------------------------------------------------------
// Public entry point
// ---------------------------------------------------------------------------
namespace systems::leal::campello_widgets
{
    std::shared_ptr<Widget> buildGalleryApp()
    {
        ImageLoader::instance().initialize(4);

        auto shell = std::make_shared<GalleryShell>();
        auto entry = std::make_shared<OverlayEntry>(shell);
        return std::make_shared<Overlay>(
            std::vector<std::shared_ptr<OverlayEntry>>{ entry });
    }
}
