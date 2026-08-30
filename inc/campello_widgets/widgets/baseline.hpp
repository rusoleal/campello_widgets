#pragma once

#include <campello_widgets/widgets/single_child_render_object_widget.hpp>
#include <campello_widgets/ui/text_baseline.hpp>

namespace systems::leal::campello_widgets
{

    /**
     * @brief Positions its child so its baseline sits `baseline` logical
     * pixels down from this widget's own top edge.
     *
     * Matches Flutter's `Baseline` widget.
     *
     * @code
     * auto w = std::make_shared<Baseline>();
     * w->baseline = 40.0f;
     * w->child    = mw<Text>("aligned to this baseline");
     * @endcode
     */
    class Baseline : public SingleChildRenderObjectWidget
    {
    public:
        float        baseline      = 0.0f;
        TextBaseline baseline_type = TextBaseline::alphabetic;

        Baseline() = default;
        explicit Baseline(float b, WidgetRef c = nullptr) : baseline(b)
        {
            child = std::move(c);
        }

        std::shared_ptr<RenderObject> createRenderObject() const override;
        void updateRenderObject(RenderObject& ro) const override;
    };

} // namespace systems::leal::campello_widgets
