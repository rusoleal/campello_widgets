#pragma once

#include <optional>
#include <campello_widgets/widgets/single_child_render_object_widget.hpp>
#include <campello_widgets/ui/alignment.hpp>

namespace systems::leal::campello_widgets
{

    /**
     * @brief Lays out its child with a different (typically looser) set of
     * constraints than this box itself received, letting the child overflow
     * this box's own bounds -- unclipped, matching Flutter's default.
     *
     * Any of `min_width`/`max_width`/`min_height`/`max_height` left unset
     * falls back to this box's own received bound on that axis.
     *
     * Matches Flutter's `OverflowBox` widget.
     *
     * @code
     * auto w = std::make_shared<OverflowBox>();
     * w->max_width  = 500.0f;  // child may be wider than the parent allows
     * w->child      = someChild;
     * @endcode
     */
    class OverflowBox : public SingleChildRenderObjectWidget
    {
    public:
        std::optional<float> min_width;
        std::optional<float> max_width;
        std::optional<float> min_height;
        std::optional<float> max_height;
        Alignment             alignment = Alignment::center();

        OverflowBox() = default;
        explicit OverflowBox(WidgetRef c) { child = std::move(c); }

        std::shared_ptr<RenderObject> createRenderObject() const override;
        void updateRenderObject(RenderObject& ro) const override;
    };

} // namespace systems::leal::campello_widgets
