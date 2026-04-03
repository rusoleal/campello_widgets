#pragma once

#include <campello_widgets/widgets/stateless_widget.hpp>
#include <campello_widgets/ui/color.hpp>

namespace systems::leal::campello_widgets
{

    /**
     * @brief A thin horizontal line with optional leading/trailing indentation.
     *
     * Consumes `height` pixels of vertical space; the dividing line itself is
     * `thickness` pixels tall and is centred within that space.
     *
     * @code
     * auto d = std::make_shared<Divider>();
     * d->thickness  = 1.0f;
     * d->indent     = 16.0f;
     * d->end_indent = 16.0f;
     * @endcode
     */
    class Divider : public StatelessWidget
    {
    public:
        /** Total vertical space consumed (line is centred within this). */
        float height      = 16.0f;
        /** Thickness of the rendered line. */
        float thickness   = 1.0f;
        /** Leading (left) indent in logical pixels. */
        float indent      = 0.0f;
        /** Trailing (right) indent in logical pixels. */
        float end_indent  = 0.0f;
        /** Line colour. Defaults to a subtle 12 % black. */
        Color color       = Color::fromRGBA(0.0f, 0.0f, 0.0f, 0.12f);

        Divider() = default;
        explicit Divider(float thick)
            : thickness(thick)
        {}
        explicit Divider(float thick, float ind, float end_ind = 0.0f)
            : thickness(thick), indent(ind), end_indent(end_ind)
        {}
        explicit Divider(float thick, float ind, float end_ind, Color col)
            : thickness(thick), indent(ind), end_indent(end_ind), color(col)
        {}

        WidgetRef build(BuildContext& ctx) const override;
    };

} // namespace systems::leal::campello_widgets
