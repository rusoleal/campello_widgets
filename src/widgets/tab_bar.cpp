#include <campello_widgets/widgets/tab_bar.hpp>
#include <campello_widgets/widgets/row.hpp>
#include <campello_widgets/widgets/column.hpp>
#include <campello_widgets/widgets/expanded.hpp>
#include <campello_widgets/widgets/padding.hpp>
#include <campello_widgets/widgets/sized_box.hpp>
#include <campello_widgets/widgets/colored_box.hpp>
#include <campello_widgets/widgets/decorated_box.hpp>
#include <campello_widgets/widgets/gesture_detector.hpp>
#include <campello_widgets/widgets/text.hpp>
#include <campello_widgets/widgets/align.hpp>
#include <campello_widgets/widgets/stack.hpp>
#include <campello_widgets/widgets/animated_switcher.hpp>
#include <campello_widgets/widgets/build_context.hpp>
#include <campello_widgets/ui/box_decoration.hpp>
#include <campello_widgets/ui/alignment.hpp>
#include <campello_widgets/ui/text_style.hpp>
#include <campello_widgets/ui/stack_fit.hpp>

namespace systems::leal::campello_widgets
{

    // =========================================================================
    // TabControllerState
    // =========================================================================

    class TabControllerState : public State<DefaultTabController>
    {
    public:
        int index_ = 0;

        void initState() override
        {
            index_ = widget().initial_index;
        }

        void setIndex(int i)
        {
            if (i == index_) return;
            setState([this, i]() { index_ = i; });
        }

        WidgetRef build(BuildContext&) override
        {
            const auto& w = widget();

            auto scope        = std::make_shared<TabScope>();
            scope->index      = index_;
            scope->length     = w.length;
            scope->state      = this;
            scope->child      = w.child;
            return scope;
        }
    };

    // =========================================================================
    // TabScope
    // =========================================================================

    bool TabScope::updateShouldNotify(const InheritedWidget& old) const
    {
        const auto& o = static_cast<const TabScope&>(old);
        return o.index != index || o.length != length;
    }

    /*static*/ const TabScope* TabScope::of(BuildContext& ctx)
    {
        return ctx.dependOnInheritedWidgetOfExactType<TabScope>();
    }

    // =========================================================================
    // DefaultTabController
    // =========================================================================

    std::unique_ptr<StateBase> DefaultTabController::createState() const
    {
        return std::make_unique<TabControllerState>();
    }

    // =========================================================================
    // TabBar
    // =========================================================================

    WidgetRef TabBar::build(BuildContext& ctx) const
    {
        const TabScope* scope = ctx.dependOnInheritedWidgetOfExactType<TabScope>();
        const int active = scope ? scope->index : 0;

        std::vector<WidgetRef> tab_widgets;
        for (int i = 0; i < static_cast<int>(tabs.size()); ++i) {
            const auto& tab = tabs[i];
            const bool  is_active = (i == active);

            // Label
            WidgetRef label;
            if (tab.child) {
                label = tab.child;
            } else {
                TextStyle ts;
                ts.color     = is_active ? label_color : unselected_label_color;
                ts.font_size = 14.0f;
                ts.font_weight = FontWeight::bold;
                label = std::make_shared<Text>(tab.text, ts);
            }

            // Indicator bar at the bottom
            WidgetRef indicator;
            if (is_active) {
                auto bar     = std::make_shared<SizedBox>();
                bar->height  = indicator_weight;
                auto colored = std::make_shared<ColoredBox>();
                colored->color = indicator_color;
                colored->child = bar;
                indicator = colored;
            } else {
                auto bar    = std::make_shared<SizedBox>();
                bar->height = indicator_weight;
                indicator   = bar;
            }

            // Tab column: label + indicator
            auto col = std::make_shared<Column>();
            col->main_axis_size       = MainAxisSize::min;
            col->cross_axis_alignment = CrossAxisAlignment::center;

            auto padded     = std::make_shared<Padding>();
            padded->padding = EdgeInsets::symmetric(4.0f, 16.0f);
            padded->child   = label;

            col->children = {padded, indicator};

            // Tap handler
            if (scope) {
                auto state_ptr = scope->state;
                int  idx       = i;
                auto g         = std::make_shared<GestureDetector>();
                g->on_tap      = [state_ptr, idx]() { state_ptr->setIndex(idx); };
                g->child       = col;
                tab_widgets.push_back(std::make_shared<Expanded>(g));
            } else {
                tab_widgets.push_back(std::make_shared<Expanded>(col));
            }
        }

        auto row = std::make_shared<Row>();
        row->main_axis_size = MainAxisSize::max;
        row->children = std::move(tab_widgets);

        if (background_color.a > 0.0f) {
            auto colored = std::make_shared<ColoredBox>();
            colored->color = background_color;
            colored->child = row;
            return colored;
        }

        return row;
    }

    // =========================================================================
    // TabBarView
    // =========================================================================

    WidgetRef TabBarView::build(BuildContext& ctx) const
    {
        const TabScope* scope = ctx.dependOnInheritedWidgetOfExactType<TabScope>();
        const int idx = scope ? scope->index : 0;

        if (children.empty()) return nullptr;
        const int clamped = (idx >= 0 && idx < static_cast<int>(children.size())) ? idx : 0;

        auto switcher = std::make_shared<AnimatedSwitcher>();
        switcher->child         = children[clamped];
        switcher->duration_ms   = 200.0;
        return switcher;
    }

} // namespace systems::leal::campello_widgets
