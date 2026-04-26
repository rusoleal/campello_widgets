#pragma once

#include <campello_widgets/widgets/stateless_widget.hpp>

namespace systems::leal::campello_widgets
{

    /**
     * @brief An adaptive horizontal divider that delegates its visual
     *        appearance to the active DesignSystem.
     *
     * The divider's thickness, color, and height are decided by the current
     * theme. You can control the leading and trailing indentation.
     *
     * @code
     * auto d = std::make_shared<Divider>();
     * d->indent = 16.0f;
     * @endcode
     */
    class Divider : public StatelessWidget
    {
    public:
        float indent     = 0.0f;
        float end_indent = 0.0f;

        Divider() = default;
        explicit Divider(float ind, float end_ind = 0.0f)
            : indent(ind), end_indent(end_ind)
        {}

        WidgetRef build(BuildContext& ctx) const override;
    };

} // namespace systems::leal::campello_widgets
