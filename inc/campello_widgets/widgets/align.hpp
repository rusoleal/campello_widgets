#pragma once

#include <optional>
#include <campello_widgets/diagnostics/diagnostic_property.hpp>
#include <campello_widgets/diagnostics/debug_assert.hpp>
#include <campello_widgets/widgets/single_child_render_object_widget.hpp>
#include <campello_widgets/ui/alignment.hpp>

namespace systems::leal::campello_widgets
{

    /**
     * @brief Aligns its child within itself using an `Alignment`.
     *
     * If `width_factor` or `height_factor` is provided the box sizes itself
     * to the corresponding child dimension multiplied by the factor.
     * Otherwise it fills the available space.
     */
    class Align : public SingleChildRenderObjectWidget
    {
    public:
        Alignment            alignment    = Alignment::center();
        std::optional<float> width_factor;
        std::optional<float> height_factor;

        Align() = default;
        explicit Align(Alignment a, WidgetRef c = nullptr)
        {
            alignment = a;
            child     = std::move(c);
        }

        
        void debugValidate() const override
        {
            if (width_factor.has_value())
                CW_ASSERT_MSG(*width_factor >= 0.0f, "Align.width_factor must be non-negative");
            if (height_factor.has_value())
                CW_ASSERT_MSG(*height_factor >= 0.0f, "Align.height_factor must be non-negative");

        }
        std::shared_ptr<RenderObject> createRenderObject() const override;
        void updateRenderObject(RenderObject& ro) const override;

        // Factory method
        static std::shared_ptr<Align> create(Alignment alignment, WidgetRef child = nullptr) {
            auto a = std::make_shared<Align>();
            a->alignment = alignment;
            a->child = std::move(child);
            return a;
        }
        void debugFillProperties(DiagnosticsPropertyBuilder& properties) const override;

    };

} // namespace systems::leal::campello_widgets
