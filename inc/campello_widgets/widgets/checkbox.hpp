#pragma once

#include <campello_widgets/widgets/stateless_widget.hpp>
#include <campello_widgets/ui/color.hpp>

#include <functional>
#include <optional>

namespace systems::leal::campello_widgets
{

    /**
     * @brief A square checkbox that toggles between checked and unchecked.
     *
     * Checkbox is controlled: the current value is supplied via `value` and
     * changes are reported via `on_changed`. Set `on_changed` to nullptr to
     * disable interaction (renders at reduced opacity).
     *
     * @code
     * auto cb = std::make_shared<Checkbox>();
     * cb->value      = is_checked;
     * cb->on_changed = [this](bool v) { setState([&]{ is_checked = v; }); };
     * @endcode
     */
    class Checkbox : public StatelessWidget
    {
    public:
        bool                           value      = false;
        std::function<void(bool)>      on_changed;

        std::optional<Color> active_color;
        std::optional<Color> check_color;
        std::optional<Color> border_color;
        float size          = 18.0f;
        float border_radius = 2.0f;

        Checkbox() = default;
        explicit Checkbox(bool val, std::function<void(bool)> on_change = nullptr)
            : value(val), on_changed(std::move(on_change))
        {}

        WidgetRef build(BuildContext& ctx) const override;
    };

} // namespace systems::leal::campello_widgets
