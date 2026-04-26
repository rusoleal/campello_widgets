#include <campello_widgets/campello_widgets.hpp>
#include <campello_widgets/macos/run_app.hpp>
#include <campello_widgets/widgets/gesture_detector.hpp>
#include <campello_widgets/widgets/debug_overlay_panel.hpp>

#include <string>
#include <sstream>
#include <iomanip>
#include <vector>
#include <algorithm>
#include <memory>
#include <iostream>

namespace cw = systems::leal::campello_widgets;

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

static cw::WidgetRef withPointerCursor(cw::WidgetRef child)
{
    auto region    = std::make_shared<cw::MouseRegion>();
    region->cursor = cw::SystemMouseCursor::pointer;
    region->child  = std::move(child);
    return region;
}

static cw::TextStyle makeStyle(float size, cw::Color color)
{
    cw::TextStyle s{};
    s.font_family = "Helvetica Neue";
    s.font_size   = size;
    s.color       = color;
    return s;
}

static std::string fmtFloat(float v)
{
    std::ostringstream ss;
    ss << std::fixed << std::setprecision(1) << v;
    return ss.str();
}

static std::string fmtOffset(const std::string& label, cw::Offset o)
{
    std::ostringstream ss;
    ss << std::fixed << std::setprecision(1);
    if (!label.empty()) ss << label << " ";
    ss << (o.x >= 0 ? "+" : "") << o.x << ", " << (o.y >= 0 ? "+" : "") << o.y;
    return ss.str();
}

// ---------------------------------------------------------------------------
// 1. Counter
// ---------------------------------------------------------------------------

class CounterDemo;

class CounterDemoState : public cw::State<CounterDemo>
{
public:
    void initState() override { count_ = 0; }

    cw::WidgetRef build(cw::BuildContext&) override
    {
        auto titleStyle = makeStyle(16.0f, cw::Color::fromRGB(0.5f, 0.5f, 0.55f));
        auto countStyle = makeStyle(96.0f, count_ >= 0
            ? cw::Color::fromRGB(0.08f, 0.47f, 0.95f)
            : cw::Color::fromRGB(0.85f, 0.20f, 0.15f));
        auto hintStyle  = makeStyle(13.0f, cw::Color::fromRGB(0.65f, 0.65f, 0.65f));

        auto label = cw::mw<cw::Text>(count_ == 0 ? "press + to start"
                              : count_ == 1 ? "1 press so far"
                              : std::to_string(count_) + " presses so far", hintStyle);

        auto counter_col = cw::mw<cw::Column>(
            cw::MainAxisAlignment::center,
            cw::CrossAxisAlignment::center,
            cw::WidgetList{
                cw::mw<cw::Text>("tap count", titleStyle),
                cw::mw<cw::Padding>(cw::EdgeInsets::symmetric(0.0f, 16.0f),
                    cw::mw<cw::Text>(std::to_string(count_), countStyle)),
                label,
            }
        );

        auto mkBtn = [&](const std::string& txt, cw::Color c, std::function<void()> fn) -> cw::WidgetRef {
            auto box = std::make_shared<cw::Container>();
            box->width   = 64.0f;
            box->height  = 64.0f;
            box->color   = c;
            box->child   = cw::mw<cw::Center>(cw::mw<cw::Text>(txt, makeStyle(28.0f, cw::Color::white())));
            auto det = std::make_shared<cw::GestureDetector>();
            det->on_tap = std::move(fn);
            det->child  = box;
            return withPointerCursor(det);
        };

        auto btns = cw::mw<cw::Row>(
            cw::MainAxisAlignment::center,
            cw::CrossAxisAlignment::center,
            cw::WidgetList{
                mkBtn("-", cw::Color::fromRGB(0.85f, 0.20f, 0.15f), [this] { std::cerr << "[CounterDemo] minus tapped, count=" << count_ << "\n"; setState([this]{ --count_; }); }),
                cw::mw<cw::SizedBox>(24.0f),
                mkBtn("0", cw::Color::fromRGB(0.50f, 0.50f, 0.55f), [this] { std::cerr << "[CounterDemo] zero tapped, count=" << count_ << "\n"; setState([this]{ count_ = 0; }); }),
                cw::mw<cw::SizedBox>(24.0f),
                mkBtn("+", cw::Color::fromRGB(0.08f, 0.47f, 0.95f), [this] { std::cerr << "[CounterDemo] plus tapped, count=" << count_ << "\n"; setState([this]{ ++count_; }); }),
            }
        );

        auto root = cw::mw<cw::Column>(
            cw::MainAxisAlignment::center,
            cw::CrossAxisAlignment::center,
            cw::WidgetList{
                cw::mw<cw::Expanded>(cw::mw<cw::Center>(counter_col)),
                cw::mw<cw::Padding>(cw::EdgeInsets::only(0.0f, 0.0f, 0.0f, 40.0f), btns),
            }
        );

        auto bg = std::make_shared<cw::Container>();
        bg->color = cw::Color::fromRGB(0.97f, 0.97f, 1.0f);
        bg->child = root;
        return bg;
    }

private:
    int count_ = 0;
};

class CounterDemo : public cw::StatefulWidget
{
public:
    std::unique_ptr<cw::StateBase> createState() const override
    {
        return std::make_unique<CounterDemoState>();
    }
};

// ---------------------------------------------------------------------------
// 2. ListView
// ---------------------------------------------------------------------------

struct Contact
{
    std::string name;
    std::string role;
    cw::Color   color;
};

static const std::vector<Contact> kContacts = {
    { "Alice Martin",   "Design Lead",      cw::Color::fromRGB(0.08f, 0.47f, 0.95f) },
    { "Bob Chen",       "Backend Engineer", cw::Color::fromRGB(0.95f, 0.30f, 0.10f) },
    { "Carol Davies",   "Product Manager",  cw::Color::fromRGB(0.10f, 0.70f, 0.40f) },
    { "David Kim",      "iOS Developer",    cw::Color::fromRGB(0.75f, 0.20f, 0.80f) },
    { "Eva Rossi",      "Data Scientist",   cw::Color::fromRGB(0.95f, 0.65f, 0.05f) },
    { "Frank M\u00fcller","DevOps Engineer",cw::Color::fromRGB(0.08f, 0.47f, 0.95f) },
    { "Grace Lee",      "QA Engineer",      cw::Color::fromRGB(0.95f, 0.30f, 0.10f) },
    { "Henry Patel",    "Android Dev",      cw::Color::fromRGB(0.10f, 0.70f, 0.40f) },
    { "Isla Torres",    "UX Researcher",    cw::Color::fromRGB(0.75f, 0.20f, 0.80f) },
    { "Jack Wilson",    "Frontend Dev",     cw::Color::fromRGB(0.95f, 0.65f, 0.05f) },
    { "Karen Nakamura", "Tech Lead",        cw::Color::fromRGB(0.08f, 0.47f, 0.95f) },
    { "Liam O'Brien",   "Site Reliability", cw::Color::fromRGB(0.95f, 0.30f, 0.10f) },
    { "Maya Singh",     "Machine Learning", cw::Color::fromRGB(0.10f, 0.70f, 0.40f) },
    { "Noah Clark",     "Security Eng.",    cw::Color::fromRGB(0.75f, 0.20f, 0.80f) },
    { "Olivia Brown",   "Platform Eng.",    cw::Color::fromRGB(0.95f, 0.65f, 0.05f) },
    { "Paul Johnson",   "API Designer",     cw::Color::fromRGB(0.08f, 0.47f, 0.95f) },
    { "Quinn Adams",    "Infra Engineer",   cw::Color::fromRGB(0.95f, 0.30f, 0.10f) },
    { "Rita Okonkwo",   "Full Stack Dev",   cw::Color::fromRGB(0.10f, 0.70f, 0.40f) },
    { "Sam Foster",     "Release Mgr.",     cw::Color::fromRGB(0.75f, 0.20f, 0.80f) },
    { "Tara Reeves",    "Scrum Master",     cw::Color::fromRGB(0.95f, 0.65f, 0.05f) },
};

class ListViewDemo;

class ListViewDemoState : public cw::State<ListViewDemo>
{
public:
    void initState() override { selected_ = -1; }

    cw::WidgetRef build(cw::BuildContext&) override
    {
        auto headerStyle = makeStyle(13.0f, cw::Color::fromRGB(0.45f, 0.45f, 0.50f));
        auto titleStyle  = makeStyle(15.0f, cw::Color::fromRGB(0.10f, 0.10f, 0.10f));
        auto subStyle    = makeStyle(12.0f, cw::Color::fromRGB(0.50f, 0.50f, 0.50f));
        auto selStyle    = makeStyle(13.0f, cw::Color::fromRGB(0.10f, 0.10f, 0.40f));

        std::string sel_text = selected_ >= 0
            ? "Selected: " + kContacts[selected_].name + " \u2014 " + kContacts[selected_].role
            : "Tap a row to select";

        auto sel_box = std::make_shared<cw::Container>();
        sel_box->color   = selected_ >= 0
            ? cw::Color::fromRGB(0.88f, 0.94f, 1.0f)
            : cw::Color::fromRGB(0.97f, 0.97f, 0.99f);
        sel_box->padding = cw::EdgeInsets::symmetric(16.0f, 10.0f);
        sel_box->child   = cw::mw<cw::Text>(sel_text, selStyle);

        auto list = std::make_shared<cw::ListView>();
        list->item_count  = static_cast<int>(kContacts.size());
        list->item_extent = 64.0f;
        list->physics     = std::make_shared<cw::BouncingScrollPhysics>();

        list->builder = [this, titleStyle, subStyle](cw::BuildContext&, int i) -> cw::WidgetRef
        {
            const Contact& c    = kContacts[i];
            const bool selected = (i == selected_);

            cw::TextStyle avatarStyle = makeStyle(18.0f, cw::Color::white());
            auto avatar = std::make_shared<cw::Container>();
            avatar->width  = 40.0f;
            avatar->height = 40.0f;
            avatar->color  = c.color;
            avatar->child  = cw::mw<cw::Center>(
                cw::mw<cw::Text>(std::string(1, c.name[0]), avatarStyle));

            cw::TextStyle name_style = titleStyle;
            if (selected) name_style.color = cw::Color::fromRGB(0.05f, 0.35f, 0.85f);

            auto text_col = cw::mw<cw::Column>(
                cw::MainAxisAlignment::center,
                cw::CrossAxisAlignment::start,
                cw::WidgetList{
                    cw::mw<cw::Text>(c.name, name_style),
                    cw::mw<cw::Padding>(cw::EdgeInsets::only(0.0f, 3.0f, 0.0f, 0.0f),
                        cw::mw<cw::Text>(c.role, subStyle)),
                }
            );

            auto row = cw::mw<cw::Row>(
                cw::MainAxisAlignment::start,
                cw::CrossAxisAlignment::center,
                cw::WidgetList{
                    avatar,
                    cw::mw<cw::SizedBox>(14.0f),
                    cw::mw<cw::Expanded>(text_col),
                }
            );

            auto cell = std::make_shared<cw::Container>();
            cell->padding = cw::EdgeInsets::symmetric(16.0f, 0.0f);
            cell->color   = selected
                ? cw::Color::fromRGB(0.90f, 0.95f, 1.0f)
                : (i % 2 == 0 ? cw::Color::fromRGB(1.0f, 1.0f, 1.0f)
                              : cw::Color::fromRGB(0.97f, 0.97f, 0.99f));
            cell->child = row;

            auto tap = std::make_shared<cw::GestureDetector>();
            tap->on_tap = [this, i] {
                setState([this, i] { selected_ = (selected_ == i) ? -1 : i; });
            };
            tap->child = cell;
            return withPointerCursor(tap);
        };

        auto root = cw::mw<cw::Column>(
            cw::MainAxisAlignment::start,
            cw::CrossAxisAlignment::stretch,
            cw::WidgetList{
                cw::mw<cw::Padding>(cw::EdgeInsets::symmetric(16.0f, 12.0f),
                    cw::mw<cw::Text>("Contacts (" + std::to_string(kContacts.size()) + ")", headerStyle)),
                sel_box,
                cw::mw<cw::Expanded>(list),
            }
        );

        auto bg = std::make_shared<cw::Container>();
        bg->color = cw::Color::fromRGB(1.0f, 1.0f, 1.0f);
        bg->child = root;
        return bg;
    }

private:
    int selected_ = -1;
};

class ListViewDemo : public cw::StatefulWidget
{
public:
    std::unique_ptr<cw::StateBase> createState() const override
    {
        return std::make_unique<ListViewDemoState>();
    }
};

// ---------------------------------------------------------------------------
// 3. Animations
// ---------------------------------------------------------------------------

static const cw::Color kBlue   = cw::Color::fromRGB(0.08f, 0.47f, 0.95f);
static const cw::Color kOrange = cw::Color::fromRGB(0.95f, 0.40f, 0.10f);
static const cw::Color kGreen  = cw::Color::fromRGB(0.10f, 0.70f, 0.40f);
static const cw::Color kPurple = cw::Color::fromRGB(0.60f, 0.20f, 0.80f);

class TweenSection;

class TweenSectionState : public cw::State<TweenSection>
{
public:
    void initState() override { ctrl_ = std::make_shared<cw::AnimationController>(800.0); }

    cw::WidgetRef build(cw::BuildContext&) override
    {
        auto bar_builder = std::make_shared<cw::AnimatedBuilder>();
        bar_builder->animation = ctrl_;
        bar_builder->builder = [ctrl = ctrl_](cw::BuildContext&) -> cw::WidgetRef
        {
            cw::CurvedAnimation curved(*ctrl, cw::Curves::easeInOut);
            const double t = curved.value();
            const float  w = cw::Tween<float>{24.0f, 280.0f}.evaluate(t);
            const cw::Color c = cw::Tween<cw::Color>{kBlue, kOrange}.evaluate(t);
            auto bar = std::make_shared<cw::Container>();
            bar->width  = w;
            bar->height = 24.0f;
            bar->color  = c;
            return bar;
        };

        auto tap = std::make_shared<cw::GestureDetector>();
        tap->on_tap = [this] {
            if (ctrl_->status() == cw::AnimationStatus::completed ||
                ctrl_->status() == cw::AnimationStatus::forward)
                ctrl_->reverse();
            else
                ctrl_->forward();
        };
        tap->child = cw::mw<cw::Padding>(cw::EdgeInsets::all(16.0f),
            cw::mw<cw::Column>(
                cw::MainAxisAlignment::start,
                cw::CrossAxisAlignment::start,
                cw::WidgetList{
                    cw::mw<cw::Text>("Tween bar \u2014 tap to toggle", makeStyle(12.0f, cw::Color::fromRGB(0.5f, 0.5f, 0.55f))),
                    cw::mw<cw::Padding>(cw::EdgeInsets::only(0.0f, 10.0f, 0.0f, 0.0f), bar_builder),
                }
            ));

        auto box = std::make_shared<cw::Container>();
        box->color = cw::Color::fromRGB(0.97f, 0.97f, 1.0f);
        box->child = withPointerCursor(tap);
        return box;
    }

private:
    std::shared_ptr<cw::AnimationController> ctrl_;
};

class TweenSection : public cw::StatefulWidget
{
public:
    std::unique_ptr<cw::StateBase> createState() const override
    {
        return std::make_unique<TweenSectionState>();
    }
};

class ImplicitSection;

class ImplicitSectionState : public cw::State<ImplicitSection>
{
public:
    void initState() override { expanded_ = false; }

    cw::WidgetRef build(cw::BuildContext&) override
    {
        auto card = std::make_shared<cw::AnimatedContainer>();
        card->duration_ms = 500.0;
        card->curve       = cw::Curves::easeInOut;
        card->color       = expanded_
            ? cw::Color::fromRGB(0.85f, 0.95f, 1.0f)
            : cw::Color::fromRGB(0.93f, 0.93f, 0.97f);
        card->width       = expanded_ ? 280.0f : 160.0f;
        card->height      = expanded_ ? 80.0f  : 40.0f;
        card->child       = cw::mw<cw::Center>(
            cw::mw<cw::Text>(expanded_ ? "shrink me" : "expand me",
                makeStyle(13.0f, cw::Color::fromRGB(0.2f, 0.2f, 0.4f))));

        auto tap = std::make_shared<cw::GestureDetector>();
        tap->on_tap = [this] { setState([this] { expanded_ = !expanded_; }); };
        tap->child  = card;

        auto box = std::make_shared<cw::Container>();
        box->padding = cw::EdgeInsets::all(16.0f);
        box->color   = cw::Color::fromRGB(0.97f, 1.0f, 0.97f);
        box->child   = cw::mw<cw::Column>(
            cw::MainAxisAlignment::start,
            cw::CrossAxisAlignment::start,
            cw::WidgetList{
                cw::mw<cw::Text>("AnimatedContainer \u2014 tap card", makeStyle(12.0f, cw::Color::fromRGB(0.5f, 0.5f, 0.55f))),
                cw::mw<cw::Padding>(cw::EdgeInsets::only(0.0f, 10.0f, 0.0f, 0.0f), withPointerCursor(tap)),
            }
        );
        return box;
    }

private:
    bool expanded_ = false;
};

class ImplicitSection : public cw::StatefulWidget
{
public:
    std::unique_ptr<cw::StateBase> createState() const override
    {
        return std::make_unique<ImplicitSectionState>();
    }
};

class OpacitySection;

class OpacitySectionState : public cw::State<OpacitySection>
{
public:
    void initState() override { visible_ = true; }

    cw::WidgetRef build(cw::BuildContext&) override
    {
        auto target = std::make_shared<cw::AnimatedOpacity>();
        target->duration_ms = 600.0;
        target->opacity     = visible_ ? 1.0f : 0.0f;
        auto hello_box = std::make_shared<cw::Container>();
        hello_box->width  = 120.0f;
        hello_box->height = 48.0f;
        hello_box->color  = kGreen;
        hello_box->child  = cw::mw<cw::Center>(cw::mw<cw::Text>("hello!", makeStyle(18.0f, cw::Color::white())));
        target->child = hello_box;

        auto toggle_btn = std::make_shared<cw::Container>();
        toggle_btn->padding = cw::EdgeInsets::symmetric(12.0f, 6.0f);
        toggle_btn->color   = kPurple;
        toggle_btn->child   = cw::mw<cw::Text>(visible_ ? "fade out" : "fade in", makeStyle(13.0f, cw::Color::white()));

        auto tap = std::make_shared<cw::GestureDetector>();
        tap->on_tap = [this] { setState([this] { visible_ = !visible_; }); };
        tap->child  = toggle_btn;

        auto row = cw::mw<cw::Row>(
            cw::MainAxisAlignment::start,
            cw::CrossAxisAlignment::center,
            cw::WidgetList{
                withPointerCursor(tap),
                cw::mw<cw::SizedBox>(16.0f),
                target,
            }
        );

        auto box = std::make_shared<cw::Container>();
        box->padding = cw::EdgeInsets::all(16.0f);
        box->color   = cw::Color::fromRGB(0.97f, 0.97f, 1.0f);
        box->child   = cw::mw<cw::Column>(
            cw::MainAxisAlignment::start,
            cw::CrossAxisAlignment::start,
            cw::WidgetList{
                cw::mw<cw::Text>("AnimatedOpacity \u2014 press button", makeStyle(12.0f, cw::Color::fromRGB(0.5f, 0.5f, 0.55f))),
                cw::mw<cw::Padding>(cw::EdgeInsets::only(0.0f, 10.0f, 0.0f, 0.0f), row),
            }
        );
        return box;
    }

private:
    bool visible_ = true;
};

class OpacitySection : public cw::StatefulWidget
{
public:
    std::unique_ptr<cw::StateBase> createState() const override
    {
        return std::make_unique<OpacitySectionState>();
    }
};

class StaggerSection;

class StaggerSectionState : public cw::State<StaggerSection>
{
public:
    void initState() override { ctrl_ = std::make_shared<cw::AnimationController>(1200.0); }

    cw::WidgetRef build(cw::BuildContext&) override
    {
        using CurveFn = cw::CurvedAnimation::CurveFn;
        struct BarSpec { cw::Color color; CurveFn curve; };
        const BarSpec specs[] = {
            { kBlue,   cw::Curves::easeIn    },
            { kGreen,  cw::Curves::easeInOut },
            { kOrange, cw::Curves::easeOut   },
        };

        std::vector<cw::WidgetRef> bars;
        for (int i = 0; i < 3; ++i)
        {
            auto bar_builder = std::make_shared<cw::AnimatedBuilder>();
            bar_builder->animation = ctrl_;
            bar_builder->builder = [ctrl = ctrl_, color = specs[i].color, curve = specs[i].curve](cw::BuildContext&) -> cw::WidgetRef
            {
                cw::CurvedAnimation curved(*ctrl, curve);
                const double t = curved.value();
                const float  w = cw::Tween<float>{16.0f, 260.0f}.evaluate(t);
                auto bar = std::make_shared<cw::Container>();
                bar->width  = w;
                bar->height = 20.0f;
                bar->color  = color;
                return bar;
            };
            bars.push_back(cw::mw<cw::Padding>(
                cw::EdgeInsets::only(0.0f, i > 0 ? 6.0f : 0.0f, 0.0f, 0.0f),
                bar_builder));
        }

        auto tap = std::make_shared<cw::GestureDetector>();
        tap->on_tap = [this] {
            if (ctrl_->status() == cw::AnimationStatus::completed ||
                ctrl_->status() == cw::AnimationStatus::forward)
                ctrl_->reverse();
            else
                ctrl_->forward();
        };
        tap->child = cw::mw<cw::Padding>(cw::EdgeInsets::all(12.0f),
            cw::mw<cw::Column>(cw::MainAxisAlignment::start, cw::CrossAxisAlignment::start, bars));

        auto box = std::make_shared<cw::Container>();
        box->padding = cw::EdgeInsets::all(16.0f);
        box->color   = cw::Color::fromRGB(1.0f, 0.97f, 0.97f);
        box->child   = cw::mw<cw::Column>(
            cw::MainAxisAlignment::start,
            cw::CrossAxisAlignment::start,
            cw::WidgetList{
                cw::mw<cw::Text>("Different curves \u2014 tap to toggle", makeStyle(12.0f, cw::Color::fromRGB(0.5f, 0.5f, 0.55f))),
                cw::mw<cw::Padding>(cw::EdgeInsets::only(0.0f, 10.0f, 0.0f, 0.0f), withPointerCursor(tap)),
            }
        );
        return box;
    }

private:
    std::shared_ptr<cw::AnimationController> ctrl_;
};

class StaggerSection : public cw::StatefulWidget
{
public:
    std::unique_ptr<cw::StateBase> createState() const override
    {
        return std::make_unique<StaggerSectionState>();
    }
};

class AnimationDemo : public cw::StatelessWidget
{
public:
    cw::WidgetRef build(cw::BuildContext&) const override
    {
        auto scroll = std::make_shared<cw::SingleChildScrollView>();
        scroll->physics = std::make_shared<cw::BouncingScrollPhysics>();
        scroll->child   = cw::mw<cw::Column>(
            cw::MainAxisAlignment::start,
            cw::CrossAxisAlignment::stretch,
            cw::WidgetList{
                cw::mw<TweenSection>(),
                cw::mw<cw::SizedBox>(std::nullopt, 2.0f),
                cw::mw<ImplicitSection>(),
                cw::mw<cw::SizedBox>(std::nullopt, 2.0f),
                cw::mw<OpacitySection>(),
                cw::mw<cw::SizedBox>(std::nullopt, 2.0f),
                cw::mw<StaggerSection>(),
            }
        );

        auto bg = std::make_shared<cw::Container>();
        bg->color = cw::Color::fromRGB(0.95f, 0.95f, 0.98f);
        bg->child = scroll;
        return bg;
    }
};

// ---------------------------------------------------------------------------
// 4. Gestures
// ---------------------------------------------------------------------------

class GestureDemo;

class GestureDemoState : public cw::State<GestureDemo>
{
public:
    void initState() override
    {
        last_gesture_       = "interact with the zone";
        zone_color_         = cw::Color::fromRGB(0.90f, 0.90f, 0.93f);
        gesture_text_color_ = cw::Color::fromRGB(0.40f, 0.40f, 0.45f);
    }

    cw::WidgetRef build(cw::BuildContext&) override
    {
        auto gestureStyle = makeStyle(36.0f, gesture_text_color_);
        auto detailStyle  = makeStyle(13.0f, cw::Color::fromRGB(0.90f, 0.90f, 1.0f));
        auto logLabelStyle= makeStyle(11.0f, cw::Color::fromRGB(0.55f, 0.55f, 0.60f));
        auto logStyle     = makeStyle(11.0f, cw::Color::fromRGB(0.55f, 0.55f, 0.60f));

        auto zone_col = std::make_shared<cw::Column>();
        zone_col->main_axis_alignment  = cw::MainAxisAlignment::center;
        zone_col->cross_axis_alignment = cw::CrossAxisAlignment::center;
        zone_col->main_axis_size       = cw::MainAxisSize::min;
        zone_col->children = {
            cw::mw<cw::Text>(last_gesture_, gestureStyle),
            cw::mw<cw::SizedBox>(std::nullopt, 8.0f),
            cw::mw<cw::Text>(detail_, detailStyle),
        };

        auto zone_bg = std::make_shared<cw::Container>();
        zone_bg->color = zone_color_;
        zone_bg->child = cw::mw<cw::Center>(zone_col);

        auto detector = std::make_shared<cw::GestureDetector>();
        detector->child = zone_bg;

        detector->on_tap = [this] {
            setState([this] {
                last_gesture_ = "tap"; detail_ = "";
                zone_color_ = cw::Color::fromRGB(0.08f, 0.47f, 0.95f);
                gesture_text_color_ = cw::Color::white();
                pushLog("tap");
            });
        };
        detector->on_double_tap = [this] {
            setState([this] {
                last_gesture_ = "double tap"; detail_ = "";
                zone_color_ = cw::Color::fromRGB(0.10f, 0.70f, 0.40f);
                gesture_text_color_ = cw::Color::white();
                pushLog("double tap");
            });
        };
        detector->on_long_press = [this] {
            setState([this] {
                last_gesture_ = "long press"; detail_ = "";
                zone_color_ = cw::Color::fromRGB(0.90f, 0.45f, 0.10f);
                gesture_text_color_ = cw::Color::white();
                pushLog("long press");
            });
        };
        detector->on_pan_update = [this](cw::Offset delta) {
            setState([this, delta] {
                pan_acc_.x += delta.x; pan_acc_.y += delta.y;
                last_gesture_ = "pan";
                detail_ = fmtOffset("delta", delta) + "   total " + fmtOffset("", pan_acc_);
                zone_color_ = cw::Color::fromRGB(0.45f, 0.20f, 0.80f);
                gesture_text_color_ = cw::Color::white();
                pushLog("pan_update  " + fmtOffset("", delta));
            });
        };
        detector->on_pan_end = [this] {
            setState([this] {
                pan_acc_ = cw::Offset::zero();
                last_gesture_ = "pan end"; detail_ = "";
                zone_color_ = cw::Color::fromRGB(0.60f, 0.40f, 0.85f);
                gesture_text_color_ = cw::Color::white();
                pushLog("pan_end");
            });
        };
        detector->on_scroll = [this](cw::Offset delta) {
            setState([this, delta] {
                scroll_acc_.x += delta.x; scroll_acc_.y += delta.y;
                last_gesture_ = "scroll";
                detail_ = fmtOffset("delta", delta) + "   total " + fmtOffset("", scroll_acc_);
                zone_color_ = cw::Color::fromRGB(0.05f, 0.65f, 0.75f);
                gesture_text_color_ = cw::Color::white();
                pushLog("scroll  " + fmtOffset("", delta));
            });
        };

        std::vector<cw::WidgetRef> log_items;
        for (auto it = log_.rbegin(); it != log_.rend(); ++it)
            log_items.push_back(cw::mw<cw::Text>(*it, logStyle));

        auto log_col = std::make_shared<cw::Column>();
        log_col->main_axis_alignment  = cw::MainAxisAlignment::start;
        log_col->cross_axis_alignment = cw::CrossAxisAlignment::start;
        log_col->main_axis_size       = cw::MainAxisSize::min;
        log_col->children             = std::move(log_items);

        auto log_section = std::make_shared<cw::Column>();
        log_section->main_axis_alignment  = cw::MainAxisAlignment::start;
        log_section->cross_axis_alignment = cw::CrossAxisAlignment::start;
        log_section->main_axis_size       = cw::MainAxisSize::min;
        log_section->children = {
            cw::mw<cw::Text>("event log", logLabelStyle),
            cw::mw<cw::SizedBox>(std::nullopt, 6.0f),
            log_col,
        };

        auto root = cw::mw<cw::Column>(
            cw::MainAxisAlignment::start,
            cw::CrossAxisAlignment::stretch,
            cw::WidgetList{
                cw::mw<cw::Expanded>(detector),
                cw::mw<cw::Padding>(
                    cw::EdgeInsets::only(32.0f, 16.0f, 32.0f, 32.0f),
                    log_section),
            }
        );

        auto bg = std::make_shared<cw::Container>();
        bg->color = cw::Color::fromRGB(0.97f, 0.97f, 1.0f);
        bg->child = root;
        return bg;
    }

private:
    void pushLog(std::string entry)
    {
        log_.push_back(std::move(entry));
        if (log_.size() > 6)
            log_.erase(log_.begin());
    }

    std::string last_gesture_;
    std::string detail_;
    cw::Color   zone_color_;
    cw::Color   gesture_text_color_;
    cw::Offset  pan_acc_;
    cw::Offset  scroll_acc_;
    std::vector<std::string> log_;
};

class GestureDemo : public cw::StatefulWidget
{
public:
    std::unique_ptr<cw::StateBase> createState() const override
    {
        return std::make_unique<GestureDemoState>();
    }
};

// ---------------------------------------------------------------------------
// 5. TextField
// ---------------------------------------------------------------------------

class TextFieldDemo;

class TextFieldDemoState : public cw::State<TextFieldDemo>
{
public:
    void initState() override
    {
        singleController_ = std::make_shared<cw::TextEditingController>();
        multiController_  = std::make_shared<cw::TextEditingController>();
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
        auto labelStyle = makeStyle(14.0f, cw::Color::fromRGB(0.5f, 0.5f, 0.5f));
        auto countStyle = makeStyle(12.0f, cw::Color::fromRGB(0.6f, 0.6f, 0.6f));
        auto hintStyle  = makeStyle(12.0f, cw::Color::fromRGB(0.7f, 0.7f, 0.7f));
        hintStyle.italic = true;

        auto textFieldStyle = makeStyle(16.0f, cw::Color::fromRGB(0.2f, 0.2f, 0.2f));

        auto singleField = std::make_shared<cw::TextField>();
        singleField->controller   = singleController_;
        singleField->placeholder  = "Type something...";
        singleField->style        = textFieldStyle;
        singleField->on_changed   = [this](const std::string& text) {
            characterCount_ = static_cast<int>(text.size());
            setState([]{});
        };

        auto multiField = std::make_shared<cw::TextField>();
        multiField->controller = multiController_;
        multiField->placeholder = "Enter multiple lines of text...";
        multiField->style       = textFieldStyle;
        multiField->max_lines   = 8;
        multiField->min_lines   = 4;
        multiField->on_changed  = [this](const std::string& text) {
            lineCount_ = static_cast<int>(std::count(text.begin(), text.end(), '\n')) + 1;
            setState([]{});
        };

        auto constrainedMultiField = std::make_shared<cw::ConstrainedBox>();
        constrainedMultiField->additional_constraints = cw::BoxConstraints(0.0f, 10000.0f, 150.0f, 200.0f);
        constrainedMultiField->child = multiField;

        auto countText = cw::mw<cw::Text>(
            "Characters: " + std::to_string(characterCount_) +
            " | Lines in multiline: " + std::to_string(lineCount_), countStyle);

        auto content = cw::mw<cw::Column>(
            cw::MainAxisAlignment::start,
            cw::CrossAxisAlignment::stretch,
            cw::WidgetList{
                cw::mw<cw::Text>("Single-line TextField:", labelStyle),
                cw::mw<cw::SizedBox>(std::nullopt, 8.0f),
                singleField,
                cw::mw<cw::SizedBox>(std::nullopt, 24.0f),
                cw::mw<cw::Text>("Multiline TextField (max 8 lines, scrollable):", labelStyle),
                cw::mw<cw::SizedBox>(std::nullopt, 8.0f),
                constrainedMultiField,
                cw::mw<cw::SizedBox>(std::nullopt, 16.0f),
                countText,
                cw::mw<cw::SizedBox>(std::nullopt, 8.0f),
                cw::mw<cw::Text>(
                    "Tip: Use mouse wheel or trackpad to scroll the multiline field. "
                    "Press Ctrl+Enter to submit multiline text.", hintStyle),
            }
        );

        auto bg = std::make_shared<cw::Container>();
        bg->color = cw::Color::fromRGB(0.97f, 0.97f, 0.98f);
        bg->child = cw::mw<cw::Padding>(cw::EdgeInsets::all(32.0f), content);
        return bg;
    }

private:
    std::shared_ptr<cw::TextEditingController> singleController_;
    std::shared_ptr<cw::TextEditingController> multiController_;
    int characterCount_ = 0;
    int lineCount_ = 15;
};

class TextFieldDemo : public cw::StatefulWidget
{
public:
    std::unique_ptr<cw::StateBase> createState() const override
    {
        return std::make_unique<TextFieldDemoState>();
    }
};

// ---------------------------------------------------------------------------
// 6. Keyboard
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

class KeyboardDemo;

class KeyboardDemoState : public cw::State<KeyboardDemo>
{
public:
    void initState() override { node_ = std::make_shared<cw::FocusNode>(); }

    cw::WidgetRef build(cw::BuildContext&) override
    {
        auto labelStyle = makeStyle(13.0f, cw::Color::fromRGB(0.55f, 0.55f, 0.60f));
        auto keyStyle   = makeStyle(72.0f, cw::Color::fromRGB(0.08f, 0.47f, 0.95f));
        auto upStyle    = makeStyle(72.0f, cw::Color::fromRGB(0.70f, 0.70f, 0.75f));
        auto repeatStyle= makeStyle(64.0f, cw::Color::fromRGB(0.08f, 0.47f, 0.95f));
        auto typedStyle = makeStyle(22.0f, cw::Color::fromRGB(0.15f, 0.15f, 0.20f));
        auto logStyle   = makeStyle(12.0f, cw::Color::fromRGB(0.60f, 0.60f, 0.65f));

        cw::TextStyle& activeKeyStyle =
            last_kind_ == cw::KeyEventKind::up     ? upStyle :
            last_kind_ == cw::KeyEventKind::repeat ? repeatStyle : keyStyle;

        const std::string keyName = last_key_.empty() ? "press a key" : modifierPrefix(last_mods_) + last_key_;

        const std::string typed_display = typed_.empty() ? "typed text appears here" : typed_;
        auto typed_row = cw::mw<cw::Text>(typed_display, typedStyle);
        if (typed_.empty()) {
            typed_row = cw::mw<cw::Text>(typed_display, makeStyle(22.0f, cw::Color::fromRGB(0.75f, 0.75f, 0.78f)));
        }

        std::vector<cw::WidgetRef> log_items;
        for (auto it = log_.rbegin(); it != log_.rend(); ++it)
            log_items.push_back(cw::mw<cw::Text>(*it, logStyle));

        auto log_col = std::make_shared<cw::Column>();
        log_col->main_axis_alignment  = cw::MainAxisAlignment::start;
        log_col->cross_axis_alignment = cw::CrossAxisAlignment::start;
        log_col->main_axis_size       = cw::MainAxisSize::min;
        log_col->children             = std::move(log_items);

        auto center_col = std::make_shared<cw::Column>();
        center_col->main_axis_alignment  = cw::MainAxisAlignment::center;
        center_col->cross_axis_alignment = cw::CrossAxisAlignment::center;
        center_col->main_axis_size       = cw::MainAxisSize::min;
        center_col->children = {
            cw::mw<cw::Text>("last key event", labelStyle),
            cw::mw<cw::SizedBox>(std::nullopt, 8.0f),
            cw::mw<cw::Text>(keyName, activeKeyStyle),
            cw::mw<cw::SizedBox>(std::nullopt, 4.0f),
            cw::mw<cw::Text>(last_key_.empty() ? "" : kindLabel(last_kind_), makeStyle(13.0f, cw::Color::fromRGB(0.55f, 0.55f, 0.60f))),
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

            const std::string entry = modifierPrefix(e.modifiers) + keyCodeName(e.key_code)
                + "  [" + kindLabel(e.kind) + "]";
            log_.push_back(entry);
            if (log_.size() > 6)
                log_.erase(log_.begin());

            if (e.kind != cw::KeyEventKind::up && e.character != 0)
            {
                if (e.key_code == cw::KeyCode::backspace)
                {
                    if (!typed_.empty()) typed_.pop_back();
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

class KeyboardDemo : public cw::StatefulWidget
{
public:
    std::unique_ptr<cw::StateBase> createState() const override
    {
        return std::make_unique<KeyboardDemoState>();
    }
};

// ---------------------------------------------------------------------------
// 7. TableView
// ---------------------------------------------------------------------------

static std::string columnLetter(int col)
{
    std::string result;
    while (col >= 0)
    {
        result = static_cast<char>('A' + (col % 26)) + result;
        col = col / 26 - 1;
    }
    return result;
}

class TableViewDemo : public cw::StatelessWidget
{
public:
    cw::WidgetRef build(cw::BuildContext&) const override
    {
        const int num_rows = 1000;
        const int num_cols = 26;

        auto headerStyle = makeStyle(13.0f, cw::Color::fromRGB(0.3f, 0.3f, 0.3f));
        headerStyle.font_weight = cw::FontWeight::bold;
        auto cellStyle = makeStyle(12.0f, cw::Color::fromRGB(0.2f, 0.2f, 0.2f));

        auto table = std::make_shared<cw::TableView>();
        table->extents = {num_rows, num_cols};

        table->row_spans.reserve(num_rows);
        for (int r = 0; r < num_rows; ++r)
        {
            cw::TableSpan span;
            span.extent = (r == 0) ? 36.0f : 32.0f;
            span.pinned = (r == 0);
            table->row_spans.push_back(span);
        }

        table->column_spans.reserve(num_cols);
        for (int c = 0; c < num_cols; ++c)
        {
            cw::TableSpan span;
            span.extent = (c == 0) ? 60.0f : 100.0f;
            span.pinned = (c == 0);
            table->column_spans.push_back(span);
        }

        table->cell_builder = [headerStyle, cellStyle, num_rows, num_cols](
            cw::BuildContext&, int row, int col) -> cw::WidgetRef
        {
            bool is_header_row = (row == 0);
            bool is_header_col = (col == 0);
            bool is_corner = is_header_row && is_header_col;

            cw::Color bg_color;
            if (is_corner)
                bg_color = cw::Color::fromRGB(0.85f, 0.85f, 0.88f);
            else if (is_header_row || is_header_col)
                bg_color = cw::Color::fromRGB(0.92f, 0.92f, 0.95f);
            else if (row % 2 == 0)
                bg_color = cw::Color::white();
            else
                bg_color = cw::Color::fromRGB(0.97f, 0.97f, 0.99f);

            std::string text;
            if (is_corner) text = "";
            else if (is_header_row) text = columnLetter(col);
            else if (is_header_col) text = std::to_string(row);
            else text = std::to_string(row * num_cols + col);

            cw::TextStyle style = (is_header_row || is_header_col) ? headerStyle : cellStyle;

            auto container = std::make_shared<cw::Container>();
            container->color = bg_color;
            container->padding = cw::EdgeInsets::symmetric(8.0f, 0.0f);
            container->child = cw::mw<cw::Center>(cw::mw<cw::Text>(text, style));
            return container;
        };

        table->physics = std::make_shared<cw::BouncingScrollPhysics>();
        return table;
    }
};

// ---------------------------------------------------------------------------
// 8. TreeView
// ---------------------------------------------------------------------------

static std::shared_ptr<cw::TreeNode> createNode(const std::string& name)
{
    auto node = std::make_shared<cw::TreeNode>();
    node->content = cw::mw<cw::Text>(name, makeStyle(14.0f, cw::Color::fromRGB(0.1f, 0.1f, 0.1f)));
    return node;
}

static std::shared_ptr<cw::TreeNode> createFileNode(const std::string& name)
{
    auto node = std::make_shared<cw::TreeNode>();
    node->content = cw::mw<cw::Text>(name, makeStyle(13.0f, cw::Color::fromRGB(0.3f, 0.3f, 0.3f)));
    return node;
}

class TreeViewDemo;

class TreeViewDemoState : public cw::State<TreeViewDemo>
{
public:
    ~TreeViewDemoState()
    {
        std::cerr << "[TreeViewDemoState] destructor called, controller=" << controller_.get() << "\n";
    }

    void initState() override
    {
        std::cerr << "[TreeViewDemoState] initState called\n";
        root_ = createNode("project");
        root_->children = {
            createFileNode("README.md"),
            createFileNode("CMakeLists.txt"),
            createFileNode("LICENSE"),
        };

        auto src = createNode("src");
        src->children = {
            createFileNode("main.cpp"),
            createFileNode("utils.cpp"),
            createFileNode("utils.hpp"),
        };
        root_->children.push_back(src);

        auto inc = createNode("include");
        inc->children = {
            createFileNode("api.hpp"),
            createFileNode("types.hpp"),
        };
        root_->children.push_back(inc);

        auto tests = createNode("tests");
        tests->children = {
            createFileNode("test_main.cpp"),
            createFileNode("test_utils.cpp"),
        };
        root_->children.push_back(tests);

        auto docs = createNode("docs");
        docs->children = {
            createFileNode("index.md"),
            createFileNode("api.md"),
            createFileNode("examples.md"),
        };
        auto guides = createNode("guides");
        guides->children = {
            createFileNode("getting-started.md"),
            createFileNode("advanced.md"),
        };
        docs->children.push_back(guides);
        root_->children.push_back(docs);

        controller_ = std::make_shared<cw::TreeController>();
        std::cerr << "[TreeViewDemoState] controller_ created: " << controller_.get() << "\n";
        controller_->expand(root_.get());
    }

    cw::WidgetRef build(cw::BuildContext&) override
    {
        auto tree = std::make_shared<cw::TreeView>();
        tree->root = root_;
        tree->controller = controller_;
        tree->indent_width = 20.0f;
        tree->row_height = 36.0f;
        tree->physics = std::make_shared<cw::BouncingScrollPhysics>();

        tree->row_builder = [this](cw::BuildContext&, const cw::TreeNode& node, int depth,
                                   bool is_expanded, bool has_children) -> cw::WidgetRef
        {
            auto indent = cw::mw<cw::SizedBox>(depth * 20.0f);

            cw::WidgetRef icon;
            if (has_children)
            {
                icon = cw::mw<cw::Text>(is_expanded ? "\u25bc" : "\u25b6",
                    makeStyle(10.0f, cw::Color::fromRGB(0.5f, 0.5f, 0.5f)));
            }
            else
            {
                icon = cw::mw<cw::SizedBox>(16.0f);
            }

            auto row = cw::mw<cw::Row>(
                cw::MainAxisAlignment::start,
                cw::CrossAxisAlignment::center,
                cw::WidgetList{
                    indent,
                    cw::mw<cw::SizedBox>(8.0f),
                    cw::mw<cw::SizedBox>(16.0f, 16.0f, icon),
                    cw::mw<cw::SizedBox>(4.0f),
                    node.content ? node.content : cw::mw<cw::Text>("(empty)"),
                }
            );

            if (has_children)
            {
                auto tap = std::make_shared<cw::GestureDetector>();
                tap->on_tap = [controller = controller_, node_ptr = &node]
                {
                    std::cerr << "[TreeViewDemoState] on_tap fired, controller=" << controller.get() << ", node_ptr=" << node_ptr << "\n";
                    if (controller)
                        controller->toggleExpanded(node_ptr);
                };
                tap->child  = row;

                auto container = std::make_shared<cw::Container>();
                container->color = cw::Color::white();
                container->padding = cw::EdgeInsets::symmetric(8.0f, 0.0f);
                container->child = tap;
                return container;
            }

            auto container = std::make_shared<cw::Container>();
            container->color = cw::Color::white();
            container->padding = cw::EdgeInsets::symmetric(8.0f, 0.0f);
            container->child = row;
            return container;
        };

        auto bg = std::make_shared<cw::Container>();
        bg->color = cw::Color::fromRGB(1.0f, 1.0f, 1.0f);
        bg->child = tree;
        return bg;
    }

private:
    std::shared_ptr<cw::TreeNode> root_;
    std::shared_ptr<cw::TreeController> controller_;
};

class TreeViewDemo : public cw::StatefulWidget
{
public:
    std::unique_ptr<cw::StateBase> createState() const override
    {
        return std::make_unique<TreeViewDemoState>();
    }
};

// ---------------------------------------------------------------------------
// 9. Image
// ---------------------------------------------------------------------------

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "../../tests/third_party/stb_image_write.h"

static std::vector<uint8_t> createTestPNG()
{
    const int width = 100, height = 100;
    std::vector<uint8_t> pixels(width * height * 4);
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            int idx = (y * width + x) * 4;
            pixels[idx + 0] = 255;
            pixels[idx + 1] = 0;
            pixels[idx + 2] = 0;
            pixels[idx + 3] = 255;
        }
    }
    int png_size = 0;
    unsigned char* png = stbi_write_png_to_mem(pixels.data(), width * 4, width, height, 4, &png_size);
    std::vector<uint8_t> result(png, png + png_size);
    free(png);
    return result;
}

class ImageDemo : public cw::StatelessWidget
{
public:
    cw::WidgetRef build(cw::BuildContext&) const override
    {
        cw::ImageLoader::instance().initialize(4);
        auto bytes = createTestPNG();

        auto image = cw::ImageWidget::memory(bytes, cw::BoxFit::fill, 100.0f, 100.0f);

        auto box = std::make_shared<cw::Container>();
        box->width  = 150.0f;
        box->height = 150.0f;
        box->color  = cw::Color::fromRGB(0, 0, 1);
        box->child  = image;

        auto label = cw::mw<cw::Text>("Generated 100\u00d7100 red PNG via ImageWidget",
            makeStyle(14.0f, cw::Color::fromRGB(0.4f, 0.4f, 0.4f)));

        auto root = cw::mw<cw::Column>(
            cw::MainAxisAlignment::center,
            cw::CrossAxisAlignment::center,
            cw::WidgetList{
                cw::mw<cw::Center>(box),
                cw::mw<cw::SizedBox>(std::nullopt, 24.0f),
                label,
            }
        );

        auto bg = std::make_shared<cw::Container>();
        bg->color = cw::Color::fromRGB(0.97f, 0.97f, 1.0f);
        bg->child = root;
        return bg;
    }
};

// ---------------------------------------------------------------------------
// Root showcase app with TabBar
// ---------------------------------------------------------------------------

// ---------------------------------------------------------------------------
// 10. Theme Demo
// ---------------------------------------------------------------------------

class ThemeDemo : public cw::StatefulWidget
{
public:
    std::function<void()> on_toggle_theme;

    std::unique_ptr<cw::StateBase> createState() const override;
};

class ThemeDemoState : public cw::State<ThemeDemo>
{
public:
    bool switch_on_ = false;
    bool checkbox_checked_ = false;

    cw::WidgetRef build(cw::BuildContext& ctx) override
    {
        const auto& w = widget();
        const auto* ts = cw::Theme::textStyleOf(ctx, cw::TextRole::title_large);
        const auto* body = cw::Theme::textStyleOf(ctx, cw::TextRole::body_medium);
        const auto* label = cw::Theme::textStyleOf(ctx, cw::TextRole::label_medium);
        const auto* title_medium = cw::Theme::textStyleOf(ctx, cw::TextRole::title_medium);

        auto heading = cw::mw<cw::Text>(
            "Adaptive Widgets",
            ts ? *ts : cw::TextStyle{}.withFontSize(20.0f).bold());

        auto sub = cw::mw<cw::Text>(
            "These widgets change appearance based on the active DesignSystem.",
            body ? *body : cw::TextStyle{}.withFontSize(12.0f));

        // Button row
        auto btn_primary = cw::mw<cw::Button>(
            cw::mw<cw::Text>("Primary"),
            [](){});
        btn_primary->priority = cw::ButtonPriority::primary;

        auto btn_secondary = cw::mw<cw::Button>(
            cw::mw<cw::Text>("Secondary"),
            [](){});
        btn_secondary->priority = cw::ButtonPriority::secondary;

        auto btn_danger = cw::mw<cw::Button>(
            cw::mw<cw::Text>("Danger"),
            [](){});
        btn_danger->priority = cw::ButtonPriority::danger;

        auto btn_row = cw::mw<cw::Row>(
            cw::MainAxisAlignment::start,
            cw::CrossAxisAlignment::center,
            cw::WidgetList{
                btn_primary,
                cw::SizedBox::from_width(12.0f),
                btn_secondary,
                cw::SizedBox::from_width(12.0f),
                btn_danger,
            });

        // Card demo
        auto card_content = cw::mw<cw::Column>(
            cw::MainAxisAlignment::start,
            cw::CrossAxisAlignment::start,
            cw::WidgetList{
                cw::mw<cw::Text>("Card Title",
                    title_medium ? *title_medium : cw::TextStyle{}.withFontSize(16.0f).bold()),
                cw::mw<cw::Padding>(
                    cw::EdgeInsets::only(0.0f, 4.0f, 0.0f, 0.0f),
                    cw::mw<cw::Text>("This card's colors come from the design system tokens.",
                        body ? *body : cw::TextStyle{}.withFontSize(12.0f))),
            });
        auto card = cw::mw<cw::Card>(card_content);
        card->priority = cw::CardPriority::elevated;

        // Switch + Checkbox row
        auto sw = cw::mw<cw::Switch>(switch_on_, [this](bool v) {
            setState([this, v]() { switch_on_ = v; });
        });

        auto cb = cw::mw<cw::Checkbox>(checkbox_checked_, [this](bool v) {
            setState([this, v]() { checkbox_checked_ = v; });
        });

        auto controls_row = cw::mw<cw::Row>(
            cw::MainAxisAlignment::start,
            cw::CrossAxisAlignment::center,
            cw::WidgetList{
                cw::mw<cw::Text>("Switch:", label ? *label : cw::TextStyle{}.withFontSize(14.0f)),
                cw::SizedBox::from_width(8.0f),
                sw,
                cw::SizedBox::from_width(24.0f),
                cw::mw<cw::Text>("Checkbox:", label ? *label : cw::TextStyle{}.withFontSize(14.0f)),
                cw::SizedBox::from_width(8.0f),
                cb,
            });

        // Divider
        auto div = cw::mw<cw::Divider>();

        // Typography scale demo
        auto typography_heading = cw::mw<cw::Text>(
            "Typography Scale",
            ts ? *ts : cw::TextStyle{}.withFontSize(20.0f).bold());

        auto makeTypeDemo = [&](const std::string& name, cw::TextRole role) -> cw::WidgetRef {
            const auto* style = cw::Theme::textStyleOf(ctx, role);
            return cw::mw<cw::Text>(name, style ? *style : cw::TextStyle{});
        };

        auto type_col = cw::mw<cw::Column>(
            cw::MainAxisAlignment::start,
            cw::CrossAxisAlignment::start,
            cw::WidgetList{
                makeTypeDemo("Display Large", cw::TextRole::display_large),
                makeTypeDemo("Headline Large", cw::TextRole::headline_large),
                makeTypeDemo("Title Large", cw::TextRole::title_large),
                makeTypeDemo("Body Large", cw::TextRole::body_large),
                makeTypeDemo("Label Large", cw::TextRole::label_large),
            });

        // Dark mode toggle button
        auto toggle_btn = cw::mw<cw::Button>(
            cw::mw<cw::Text>("Toggle Dark Mode"),
            w.on_toggle_theme);
        toggle_btn->priority = cw::ButtonPriority::primary;

        auto toggle_row = cw::mw<cw::Row>(
            cw::MainAxisAlignment::center,
            cw::CrossAxisAlignment::center,
            cw::WidgetList{ toggle_btn });

        return cw::mw<cw::SingleChildScrollView>(
            cw::mw<cw::Padding>(
                cw::EdgeInsets::all(24.0f),
                cw::mw<cw::Column>(
                    cw::MainAxisAlignment::start,
                    cw::CrossAxisAlignment::stretch,
                    cw::WidgetList{
                        heading,
                        cw::mw<cw::Padding>(cw::EdgeInsets::only(0.0f, 4.0f, 0.0f, 16.0f), sub),
                        btn_row,
                        cw::mw<cw::Padding>(cw::EdgeInsets::only(0.0f, 16.0f, 0.0f, 16.0f), card),
                        controls_row,
                        cw::mw<cw::Padding>(cw::EdgeInsets::only(0.0f, 16.0f, 0.0f, 16.0f), div),
                        typography_heading,
                        cw::mw<cw::Padding>(cw::EdgeInsets::only(0.0f, 8.0f, 0.0f, 16.0f), type_col),
                        toggle_row,
                    })));
    }
};

std::unique_ptr<cw::StateBase> ThemeDemo::createState() const
{
    return std::make_unique<ThemeDemoState>();
}

// ---------------------------------------------------------------------------
// ShowcaseApp
// ---------------------------------------------------------------------------

class ShowcaseApp : public cw::StatefulWidget
{
public:
    std::unique_ptr<cw::StateBase> createState() const override;
};

class ShowcaseAppState : public cw::State<ShowcaseApp>
{
public:
    std::shared_ptr<const cw::DesignSystem> ds_;
    bool follow_system_ = true;

    void initState() override
    {
        ds_ = std::make_shared<cw::CampelloDesignSystem>(cw::CampelloDesignSystem::light());
    }

    void toggleTheme()
    {
        setState([this]() {
            follow_system_ = false; // User override — stop following system
            bool is_dark = (ds_->tokens().brightness == cw::Brightness::dark);
            ds_ = std::make_shared<cw::CampelloDesignSystem>(
                is_dark ? cw::CampelloDesignSystem::light()
                        : cw::CampelloDesignSystem::dark());
        });
    }

    cw::WidgetRef build(cw::BuildContext& ctx) override
    {
        // If following system, sync with MediaQuery platform brightness
        if (follow_system_) {
            const auto* media = cw::MediaQuery::of(ctx);
            if (media) {
                bool want_dark = (media->platform_brightness == cw::Brightness::dark);
                bool is_dark = (ds_->tokens().brightness == cw::Brightness::dark);
                if (want_dark != is_dark) {
                    ds_ = std::make_shared<cw::CampelloDesignSystem>(
                        want_dark ? cw::CampelloDesignSystem::dark()
                                  : cw::CampelloDesignSystem::light());
                }
            }
        }
        auto tabCtrl = std::make_shared<cw::DefaultTabController>();
        tabCtrl->length = 10;
        tabCtrl->child = cw::mw<cw::Column>(
            cw::MainAxisAlignment::start,
            cw::CrossAxisAlignment::stretch,
            cw::WidgetList{
                cw::mw<cw::TabBar>(std::vector<cw::Tab>{
                    cw::Tab{"Theme"},
                    cw::Tab{"Counter"},
                    cw::Tab{"List"},
                    cw::Tab{"Animate"},
                    cw::Tab{"Gestures"},
                    cw::Tab{"TextField"},
                    cw::Tab{"Keyboard"},
                    cw::Tab{"Table"},
                    cw::Tab{"Tree"},
                    cw::Tab{"Image"},
                }),
                [&]() -> cw::WidgetRef {
                    auto theme_demo = std::make_shared<ThemeDemo>();
                    theme_demo->on_toggle_theme = [this]() { toggleTheme(); };

                    auto tabView = std::make_shared<cw::TabBarView>(std::vector<cw::WidgetRef>{
                        theme_demo,
                        std::make_shared<CounterDemo>(),
                        std::make_shared<ListViewDemo>(),
                        std::make_shared<AnimationDemo>(),
                        std::make_shared<GestureDemo>(),
                        std::make_shared<TextFieldDemo>(),
                        std::make_shared<KeyboardDemo>(),
                        std::make_shared<TableViewDemo>(),
                        std::make_shared<TreeViewDemo>(),
                        std::make_shared<ImageDemo>(),
                    });
                    tabView->animation_duration_ms = 0.0;
                    return cw::mw<cw::Expanded>(tabView);
                }(),
            }
        );
        return cw::mw<cw::Theme>(ds_, tabCtrl);
    }
};

std::unique_ptr<cw::StateBase> ShowcaseApp::createState() const
{
    return std::make_unique<ShowcaseAppState>();
}

// ---------------------------------------------------------------------------
// Menu bar
// ---------------------------------------------------------------------------

static std::vector<cw::PlatformMenuRef> buildMenuBar()
{
    return {
        cw::PlatformMenu::create("File", {
            cw::PlatformMenuItemLabel::create("New", "Cmd+N", []() {}),
            cw::PlatformMenuItemLabel::create("Open...", "Cmd+O", []() {}),
            cw::PlatformMenuItemSeparator::create(),
            cw::PlatformMenuItemLabel::create("Save", "Cmd+S", []() {}),
            cw::PlatformMenuItemSeparator::create(),
            cw::PlatformProvidedMenuItem::close(),
        }),
        cw::PlatformMenu::create("Edit", {
            cw::PlatformProvidedMenuItem::undo(),
            cw::PlatformProvidedMenuItem::redo(),
            cw::PlatformMenuItemSeparator::create(),
            cw::PlatformProvidedMenuItem::cut(),
            cw::PlatformProvidedMenuItem::copy(),
            cw::PlatformProvidedMenuItem::paste(),
            cw::PlatformMenuItemSeparator::create(),
            cw::PlatformProvidedMenuItem::select_all(),
        }),
        cw::PlatformMenu::create("View", {
            cw::PlatformMenuItemLabel::create("Toggle Dark Mode", "Cmd+T", []() {
                // Menu callback will be wired up after app creation
            }),
            cw::PlatformMenuItemSeparator::create(),
            cw::PlatformMenuItemLabel::create("Zoom In",  "Cmd++", []() {}),
            cw::PlatformMenuItemLabel::create("Zoom Out", "Cmd+-", []() {}),
            cw::PlatformMenuItemSeparator::create(),
            cw::PlatformProvidedMenuItem::toggle_fullscreen(),
        }),
        cw::PlatformMenu::create("Window", {
            cw::PlatformProvidedMenuItem::minimize(),
            cw::PlatformProvidedMenuItem::zoom(),
            cw::PlatformMenuItemSeparator::create(),
            cw::PlatformProvidedMenuItem::bring_all_to_front(),
        }),
        cw::PlatformMenu::create("Help", {
            cw::PlatformMenuItemLabel::create("Documentation", []() {}),
            cw::PlatformMenuItemSeparator::create(),
            cw::PlatformProvidedMenuItem::about(),
        }),
    };
}

// ---------------------------------------------------------------------------
// Entry point
// ---------------------------------------------------------------------------

int main()
{
    cw::DebugFlags::showDebugBanner       = false;
    cw::DebugFlags::showPerformanceOverlay = true;
    cw::DebugFlags::paintSizeEnabled      = false;

    auto menus = buildMenuBar();
    auto app   = std::make_shared<ShowcaseApp>();
    auto menu_bar = cw::PlatformMenuBar::create(menus, app);

    auto overlay = std::make_shared<cw::DebugOverlayPanel>();
    overlay->child = menu_bar;

    return cw::runApp(
        overlay,
        "campello_widgets \u2014 Showcase",
        1100.0f,
        800.0f);
}
