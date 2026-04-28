#pragma once

#include <campello_widgets/diagnostics/diagnostic_property.hpp>
#include <campello_widgets/diagnostics/debug_assert.hpp>
#include <campello_widgets/widgets/single_child_render_object_widget.hpp>
#include <campello_widgets/ui/edge_insets.hpp>

namespace systems::leal::campello_widgets
{

    /**
     * @brief Insets its child by the given EdgeInsets.
     */
    class Padding : public SingleChildRenderObjectWidget
    {
    public:
        EdgeInsets padding;

        Padding() = default;
        explicit Padding(EdgeInsets p, WidgetRef c = nullptr)
        {
            padding = std::move(p);
            child   = std::move(c);
        }

        std::shared_ptr<RenderObject> createRenderObject() const override;
        void updateRenderObject(RenderObject& ro) const override;

        // Factory method
        static std::shared_ptr<Padding> create(EdgeInsets insets, WidgetRef child = nullptr) {
            auto p = std::make_shared<Padding>();
            p->padding = std::move(insets);
            p->child = std::move(child);
            return p;
        }

        void debugValidate() const override
        {
            CW_ASSERT_MSG(padding.left >= 0.0f, "Padding.padding.left must be non-negative");
            CW_ASSERT_MSG(padding.top >= 0.0f, "Padding.padding.top must be non-negative");
            CW_ASSERT_MSG(padding.right >= 0.0f, "Padding.padding.right must be non-negative");
            CW_ASSERT_MSG(padding.bottom >= 0.0f, "Padding.padding.bottom must be non-negative");
        }

        void debugFillProperties(DiagnosticsPropertyBuilder& properties) const override;

    };

} // namespace systems::leal::campello_widgets
