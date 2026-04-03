#include <campello_widgets/campello_widgets.hpp>
#include <campello_widgets/macos/run_app.hpp>

#include <string>
#include <memory>

namespace cw = systems::leal::campello_widgets;

// ---------------------------------------------------------------------------
// Helper to create tree nodes
// ---------------------------------------------------------------------------

static std::shared_ptr<cw::TreeNode> createNode(const std::string& name)
{
    cw::TextStyle style{};
    style.font_family = "Helvetica Neue";
    style.font_size   = 14.0f;
    style.color       = cw::Color::fromRGB(0.1f, 0.1f, 0.1f);

    auto node = std::make_shared<cw::TreeNode>();
    node->content = cw::mw<cw::Text>(name, style);
    return node;
}

static std::shared_ptr<cw::TreeNode> createFileNode(const std::string& name)
{
    cw::TextStyle style{};
    style.font_family = "Helvetica Neue";
    style.font_size   = 13.0f;
    style.color       = cw::Color::fromRGB(0.3f, 0.3f, 0.3f);

    auto node = std::make_shared<cw::TreeNode>();
    node->content = cw::mw<cw::Text>(name, style);
    return node;
}

// ---------------------------------------------------------------------------
// TreeView Example - File explorer-like demo
// ---------------------------------------------------------------------------

class TreeViewApp : public cw::StatefulWidget
{
public:
    std::unique_ptr<cw::StateBase> createState() const override;
};

class TreeViewState : public cw::State<TreeViewApp>
{
public:
    void initState() override
    {
        // Build sample file tree
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

        // Create controller and expand root
        controller_ = std::make_shared<cw::TreeController>();
        controller_->expand(root_.get());
    }

    cw::WidgetRef build(cw::BuildContext&) override
    {
        cw::TextStyle titleStyle{};
        titleStyle.font_family = "Helvetica Neue";
        titleStyle.font_size   = 16.0f;
        titleStyle.font_weight = cw::FontWeight::bold;
        titleStyle.color       = cw::Color::fromRGB(0.1f, 0.1f, 0.1f);

        auto title = std::make_shared<cw::Container>();
        title->color = cw::Color::fromRGB(0.95f, 0.95f, 0.98f);
        title->padding = cw::EdgeInsets::symmetric(16.0f, 14.0f);
        title->child = cw::mw<cw::Text>("TreeView Example — File Explorer", titleStyle);

        cw::TextStyle infoStyle{};
        infoStyle.font_family = "Helvetica Neue";
        infoStyle.font_size   = 12.0f;
        infoStyle.color       = cw::Color::fromRGB(0.5f, 0.5f, 0.5f);

        auto info = std::make_shared<cw::Container>();
        info->color = cw::Color::fromRGB(0.97f, 0.97f, 0.99f);
        info->padding = cw::EdgeInsets::symmetric(16.0f, 8.0f);
        info->child = cw::mw<cw::Text>(
            "Tap ▶/▼ to expand/collapse • Drag to scroll vertically and horizontally",
            infoStyle);

        // TreeView
        auto tree = std::make_shared<cw::TreeView>();
        tree->root = root_;
        tree->controller = controller_;
        tree->indent_width = 20.0f;
        tree->row_height = 36.0f;
        tree->physics = std::make_shared<cw::BouncingScrollPhysics>();

        tree->row_builder = [this](
            cw::BuildContext&,
            const cw::TreeNode& node,
            int depth,
            bool is_expanded,
            bool has_children) -> cw::WidgetRef
        {
            // Indentation
            auto indent = cw::mw<cw::SizedBox>(depth * 20.0f);

            // Expand/collapse icon
            cw::WidgetRef icon;
            if (has_children)
            {
                cw::TextStyle iconStyle{};
                iconStyle.font_family = "Helvetica Neue";
                iconStyle.font_size   = 10.0f;
                iconStyle.color       = cw::Color::fromRGB(0.5f, 0.5f, 0.5f);
                icon = cw::mw<cw::Text>(is_expanded ? "▼" : "▶", iconStyle);
            }
            else
            {
                icon = cw::mw<cw::SizedBox>(16.0f);
            }

            // Content (node.content is already a WidgetRef)
            cw::WidgetRef content;
            if (node.content)
                content = node.content;
            else
                content = cw::mw<cw::Text>("(empty)");

            auto row = cw::mw<cw::Row>(
                cw::MainAxisAlignment::start,
                cw::CrossAxisAlignment::center,
                cw::WidgetList{
                    indent,
                    cw::mw<cw::SizedBox>(8.0f),
                    cw::mw<cw::SizedBox>(16.0f, 16.0f, icon),
                    cw::mw<cw::SizedBox>(4.0f),
                    content,
                }
            );

            // Make tappable for expand/collapse
            if (has_children)
            {
                auto tap = std::make_shared<cw::GestureDetector>();
                tap->on_tap = [this, &node] {
                    controller_->toggleExpanded(&node);
                };
                tap->child = row;

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

        auto column = cw::mw<cw::Column>(
            cw::MainAxisAlignment::start,
            cw::CrossAxisAlignment::stretch,
            cw::WidgetList{
                title,
                info,
                cw::mw<cw::Expanded>(tree),
            }
        );

        auto bg = std::make_shared<cw::Container>();
        bg->color = cw::Color::fromRGB(1.0f, 1.0f, 1.0f);
        bg->child = column;
        return bg;
    }

private:
    std::shared_ptr<cw::TreeNode> root_;
    std::shared_ptr<cw::TreeController> controller_;
};

std::unique_ptr<cw::StateBase> TreeViewApp::createState() const
{
    return std::make_unique<TreeViewState>();
}

// ---------------------------------------------------------------------------
// Entry point
// ---------------------------------------------------------------------------

int main()
{
    cw::DebugFlags::showDebugBanner        = false;
    cw::DebugFlags::showPerformanceOverlay = true;

    return cw::runApp(
        std::make_shared<TreeViewApp>(),
        "campello_widgets — Tree View",
        500.0f,
        600.0f);
}
