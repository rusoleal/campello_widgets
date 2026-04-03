#pragma once

#include <campello_widgets/widgets/multi_child_render_object_widget.hpp>
#include <campello_widgets/ui/axis.hpp>
#include <campello_widgets/ui/flex_properties.hpp>
#include <campello_widgets/ui/wrap_properties.hpp>

namespace systems::leal::campello_widgets
{

    /**
     * @brief Lays out children in a flow that wraps onto multiple lines.
     *
     * Children are placed sequentially along `direction`. When there is not
     * enough room for the next child, a new run begins. `spacing` controls
     * the gap between children within a run; `run_spacing` controls the gap
     * between runs.
     *
     * @code
     * auto w = std::make_shared<Wrap>();
     * w->spacing     = 8.0f;
     * w->run_spacing = 8.0f;
     * w->children    = { chipA, chipB, chipC };
     * @endcode
     */
    class Wrap : public MultiChildRenderObjectWidget
    {
    public:
        Axis               direction             = Axis::horizontal;
        WrapAlignment      alignment             = WrapAlignment::start;
        float              spacing               = 0.0f;
        WrapRunAlignment   run_alignment         = WrapRunAlignment::start;
        float              run_spacing           = 0.0f;
        WrapCrossAlignment cross_axis_alignment  = WrapCrossAlignment::start;

        Wrap() = default;
        explicit Wrap(WidgetList ch)
        {
            children = ch;
        }
        explicit Wrap(std::vector<WidgetRef> ch)
        {
            children = std::move(ch);
        }
        explicit Wrap(Axis dir, float space, float run_space, WidgetList ch)
            : direction(dir), spacing(space), run_spacing(run_space)
        {
            children = ch;
        }

        std::shared_ptr<RenderObject> createRenderObject() const override;
        void updateRenderObject(RenderObject& ro) const override;

        void insertRenderObjectChild(
            RenderObject& parent,
            std::shared_ptr<RenderBox> child_box,
            int index) const override;

        void clearRenderObjectChildren(RenderObject& parent) const override;
    };

} // namespace systems::leal::campello_widgets
