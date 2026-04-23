#include <campello_widgets/widgets/debug_overlay_panel.hpp>
#include <campello_widgets/widgets/switch.hpp>
#include <campello_widgets/widgets/text.hpp>
#include <campello_widgets/widgets/container.hpp>
#include <campello_widgets/widgets/column.hpp>
#include <campello_widgets/widgets/row.hpp>
#include <campello_widgets/widgets/padding.hpp>
#include <campello_widgets/widgets/gesture_detector.hpp>
#include <campello_widgets/widgets/align.hpp>
#include <campello_widgets/widgets/stack.hpp>
#include <campello_widgets/widgets/positioned.hpp>
#include <campello_widgets/widgets/sized_box.hpp>
#include <campello_widgets/ui/debug_flags.hpp>
#include <campello_widgets/diagnostics/widget_inspector.hpp>
#include <campello_widgets/campello_widgets.hpp>

namespace systems::leal::campello_widgets
{

    class DebugOverlayPanelState : public State<DebugOverlayPanel>
    {
    public:
        bool visible = false;

        void initState() override {}

        WidgetRef build(BuildContext&) override
        {
            auto content = widget().child;
            if (!content) content = std::make_shared<Container>();

            if (!visible)
            {
                // Just a small floating button in the corner
                auto btn = std::make_shared<Container>();
                btn->color = Color::fromRGBA(0.1f, 0.1f, 0.1f, 0.7f);
                btn->child = mw<Padding>(EdgeInsets::all(8.0f),
                    mw<Text>("DEBUG", TextStyle{Color::white(), 10.0f, {}}));

                auto tap = std::make_shared<GestureDetector>();
                tap->on_tap = [this]() { setState([this] { visible = true; }); };
                tap->child = btn;

                auto overlay = std::make_shared<Stack>();
                overlay->children = {content, tap};
                return overlay;
            }

            // Full panel
            auto panel = buildPanel();

            auto overlay = std::make_shared<Stack>();
            overlay->children = {content, panel};
            return overlay;
        }

    private:
        WidgetRef buildPanel()
        {
            auto panel_col = std::make_shared<Column>();
            panel_col->main_axis_alignment = MainAxisAlignment::start;
            panel_col->cross_axis_alignment = CrossAxisAlignment::start;
            panel_col->children = {
                mw<Padding>(EdgeInsets::all(12.0f), mw<Text>("Debug Settings", TextStyle{Color::white(), 16.0f, {}})),
                buildSwitch("Paint Size",       &DebugFlags::paintSizeEnabled),
                buildSwitch("Repaint Rainbow",  &DebugFlags::repaintRainbowEnabled),
                buildSwitch("Debug Banner",     &DebugFlags::showDebugBanner),
                buildSwitch("Performance",      &DebugFlags::showPerformanceOverlay),
                buildSwitch("Print Rebuilds",   &DebugFlags::printRebuildsEnabled),
                buildSwitch("Paint Baselines",  &DebugFlags::paintBaselinesEnabled),
                buildSwitch("Paint Pointers",   &DebugFlags::paintPointersEnabled),
                buildButton("Dump Widget Tree", [this]() { WidgetInspector::instance().dumpWidgetTree(); }),
                buildButton("Dump Render Tree", [this]() { WidgetInspector::instance().dumpRenderObjectTree(); }),
                buildButton("Close", [this]() { const_cast<DebugOverlayPanelState*>(this)->setState([this] { visible = false; }); }),
            };

            auto panel_bg = std::make_shared<Container>();
            panel_bg->color = Color::fromRGBA(0.12f, 0.12f, 0.14f, 0.95f);
            panel_bg->child = mw<Padding>(EdgeInsets::all(8.0f), panel_col);

            auto sized = std::make_shared<SizedBox>();
            sized->width = 220.0f;
            sized->child = panel_bg;

            auto positioned = std::make_shared<Positioned>();
            positioned->right = 8.0f;
            positioned->top = 8.0f;
            positioned->child = sized;

            return positioned;
        }

        WidgetRef buildSwitch(const std::string& label, bool* flag)
        {
            auto sw = std::make_shared<Switch>();
            sw->value = *flag;
            sw->on_changed = [flag](bool v) { *flag = v; };
            sw->width = 36.0f;
            sw->height = 20.0f;

            auto label_text = mw<Text>(label, TextStyle{Color::fromRGB(0.85f, 0.85f, 0.85f), 12.0f, {}});

            auto row = std::make_shared<Row>();
            row->main_axis_alignment = MainAxisAlignment::spaceBetween;
            row->cross_axis_alignment = CrossAxisAlignment::center;
            row->children = {label_text, sw};

            return mw<Padding>(EdgeInsets::symmetric(4.0f, 8.0f), row);
        }

        WidgetRef buildButton(const std::string& label, std::function<void()> onTap)
        {
            auto btn_bg = std::make_shared<Container>();
            btn_bg->color = Color::fromRGBA(0.2f, 0.2f, 0.25f, 1.0f);
            btn_bg->child = mw<Padding>(EdgeInsets::all(8.0f),
                mw<Align>(Alignment::center(), mw<Text>(label, TextStyle{Color::white(), 12.0f, {}})));

            auto tap = std::make_shared<GestureDetector>();
            tap->on_tap = std::move(onTap);
            tap->child = btn_bg;

            return mw<Padding>(EdgeInsets::symmetric(4.0f, 8.0f), tap);
        }
    };

    std::unique_ptr<StateBase> DebugOverlayPanel::createState() const
    {
        return std::make_unique<DebugOverlayPanelState>();
    }

} // namespace systems::leal::campello_widgets
