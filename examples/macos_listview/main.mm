#include <campello_widgets/campello_widgets.hpp>
#include <campello_widgets/macos/run_app.hpp>
#include <campello_widgets/widgets/gesture_detector.hpp>

#include <string>
#include <vector>

namespace cw = systems::leal::campello_widgets;

// Wraps a widget in a MouseRegion that shows the pointer cursor on hover.
static cw::WidgetRef withPointerCursor(cw::WidgetRef child)
{
    auto region    = std::make_shared<cw::MouseRegion>();
    region->cursor = cw::SystemMouseCursor::pointer;
    region->child  = std::move(child);
    return region;
}

// ---------------------------------------------------------------------------
// Sample data
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
    { "Frank Müller",   "DevOps Engineer",  cw::Color::fromRGB(0.08f, 0.47f, 0.95f) },
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

// ---------------------------------------------------------------------------
// ListViewApp
// ---------------------------------------------------------------------------

class ListViewApp;

class ListViewState : public cw::State<ListViewApp>
{
public:
    void initState() override { selected_ = -1; }

    cw::WidgetRef build(cw::BuildContext&) override
    {
        cw::TextStyle headerStyle{};
        headerStyle.font_family = "Helvetica Neue";
        headerStyle.font_size   = 13.0f;
        headerStyle.color       = cw::Color::fromRGB(0.45f, 0.45f, 0.50f);

        cw::TextStyle titleStyle{};
        titleStyle.font_family = "Helvetica Neue";
        titleStyle.font_size   = 15.0f;
        titleStyle.color       = cw::Color::fromRGB(0.10f, 0.10f, 0.10f);

        cw::TextStyle subStyle{};
        subStyle.font_family = "Helvetica Neue";
        subStyle.font_size   = 12.0f;
        subStyle.color       = cw::Color::fromRGB(0.50f, 0.50f, 0.50f);

        cw::TextStyle selectedStyle{};
        selectedStyle.font_family = "Helvetica Neue";
        selectedStyle.font_size   = 13.0f;
        selectedStyle.color       = cw::Color::fromRGB(0.10f, 0.10f, 0.40f);

        // Header bar
        auto header_box = std::make_shared<cw::Container>();
        header_box->color   = cw::Color::fromRGB(0.95f, 0.95f, 0.98f);
        header_box->padding = cw::EdgeInsets::symmetric(16.0f, 12.0f);
        header_box->child   = cw::make<cw::Text>(
            std::string("Contacts  (") + std::to_string(kContacts.size()) + ")",
            headerStyle);

        // Selection info bar
        std::string sel_text = selected_ >= 0
            ? "Selected: " + kContacts[selected_].name + " — " + kContacts[selected_].role
            : "Tap a row to select";

        auto sel_box = std::make_shared<cw::Container>();
        sel_box->color   = selected_ >= 0
            ? cw::Color::fromRGB(0.88f, 0.94f, 1.0f)
            : cw::Color::fromRGB(0.97f, 0.97f, 0.99f);
        sel_box->padding = cw::EdgeInsets::symmetric(16.0f, 10.0f);
        sel_box->child   = cw::make<cw::Text>(sel_text, selectedStyle);

        // Contact list
        auto list = std::make_shared<cw::ListView>();
        list->item_count  = static_cast<int>(kContacts.size());
        list->item_extent = 64.0f;
        list->physics     = std::make_shared<cw::BouncingScrollPhysics>();

        list->builder = [this, titleStyle, subStyle](cw::BuildContext&, int i) -> cw::WidgetRef
        {
            const Contact& c   = kContacts[i];
            const bool selected = (i == selected_);

            // Avatar circle
            cw::TextStyle avatarStyle{};
            avatarStyle.font_family = "Helvetica Neue";
            avatarStyle.font_size   = 18.0f;
            avatarStyle.color       = cw::Color::white();

            auto avatar = std::make_shared<cw::Container>();
            avatar->width  = 40.0f;
            avatar->height = 40.0f;
            avatar->color  = c.color;
            avatar->child  = cw::make<cw::Center>(
                cw::make<cw::Text>(std::string(1, c.name[0]), avatarStyle));

            // Name + role column
            cw::TextStyle name_style = titleStyle;
            if (selected) name_style.color = cw::Color::fromRGB(0.05f, 0.35f, 0.85f);

            auto text_col = cw::make<cw::Column>(
                cw::MainAxisAlignment::center,
                cw::CrossAxisAlignment::start,
                cw::WidgetList{
                    cw::make<cw::Text>(c.name, name_style),
                    cw::make<cw::Padding>(
                        cw::EdgeInsets::only(0.0f, 3.0f, 0.0f, 0.0f),
                        cw::make<cw::Text>(c.role, subStyle)),
                }
            );

            auto row = cw::make<cw::Row>(
                cw::MainAxisAlignment::start,
                cw::CrossAxisAlignment::center,
                cw::WidgetList{
                    avatar,
                    cw::make<cw::SizedBox>(14.0f),
                    cw::make<cw::Expanded>(text_col),
                }
            );

            auto cell = std::make_shared<cw::Container>();
            cell->padding = cw::EdgeInsets::symmetric(16.0f, 0.0f);
            cell->color   = selected
                ? cw::Color::fromRGB(0.90f, 0.95f, 1.0f)
                : (i % 2 == 0
                    ? cw::Color::fromRGB(1.0f,  1.0f,  1.0f)
                    : cw::Color::fromRGB(0.97f, 0.97f, 0.99f));
            cell->child = row;

            auto tap = std::make_shared<cw::GestureDetector>();
            tap->on_tap = [this, i] {
                setState([this, i] {
                    selected_ = (selected_ == i) ? -1 : i;
                });
            };
            tap->child = cell;
            return withPointerCursor(tap);
        };

        auto root_col = cw::make<cw::Column>(
            cw::MainAxisAlignment::start,
            cw::CrossAxisAlignment::stretch,
            cw::WidgetList{
                header_box,
                sel_box,
                cw::make<cw::Expanded>(list),
            }
        );

        auto bg = std::make_shared<cw::Container>();
        bg->color = cw::Color::fromRGB(1.0f, 1.0f, 1.0f);
        bg->child = root_col;
        return bg;
    }

private:
    int selected_ = -1;
};

class ListViewApp : public cw::StatefulWidget
{
public:
    std::unique_ptr<cw::StateBase> createState() const override
    {
        return std::make_unique<ListViewState>();
    }
};

// ---------------------------------------------------------------------------
// Entry point
// ---------------------------------------------------------------------------

int main()
{
    cw::DebugFlags::showDebugBanner        = false;
    cw::DebugFlags::showPerformanceOverlay = true;

    return cw::runApp(
        std::make_shared<ListViewApp>(),
        "campello_widgets — List View",
        400.0f,
        600.0f);
}
