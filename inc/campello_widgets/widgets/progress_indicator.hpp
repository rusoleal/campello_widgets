#pragma once

#include <campello_widgets/widgets/stateless_widget.hpp>
#include <campello_widgets/ui/design_system.hpp>

#include <optional>

namespace systems::leal::campello_widgets
{

    /**
     * @brief An adaptive progress indicator that delegates to the active design system.
     *
     * Displays either a circular spinner or a horizontal bar depending on `type`.
     * When `value` is unset the indicator animates continuously (indeterminate).
     *
     * @code
     * auto spinner = std::make_shared<ProgressIndicator>();
     *
     * auto bar = std::make_shared<ProgressIndicator>();
     * bar->type  = ProgressType::linear;
     * bar->value = 0.6f;
     * @endcode
     */
    class ProgressIndicator : public StatelessWidget
    {
    public:
        ProgressType type = ProgressType::circular;
        std::optional<float> value; ///< null = indeterminate

        ProgressIndicator() = default;
        explicit ProgressIndicator(ProgressType t)
            : type(t)
        {}
        explicit ProgressIndicator(ProgressType t, float val)
            : type(t), value(val)
        {}

        WidgetRef build(BuildContext& ctx) const override;
    };

} // namespace systems::leal::campello_widgets
