#pragma once

#include <campello_widgets/widgets/stateless_widget.hpp>
#include <campello_widgets/widgets/offstage.hpp>
#include <campello_widgets/widgets/sized_box.hpp>

namespace systems::leal::campello_widgets
{

    /**
     * @brief Shows or hides a child without necessarily unmounting it.
     *
     * When `maintain_state` is true (the default), the child's Element/State
     * stays mounted while hidden -- built on top of `Offstage`, which is
     * exactly this widget's `maintainState:true` behavior in Flutter. When
     * `maintain_state` is false, the child (and any state it owns) is
     * unmounted entirely while hidden, replaced by `replacement` (default:
     * a zero-size box).
     *
     * @code
     * auto w = std::make_shared<Visibility>();
     * w->visible = isLoggedIn;
     * w->child   = accountPanel;
     * @endcode
     */
    class Visibility : public StatelessWidget
    {
    public:
        bool      visible        = true;
        bool      maintain_state = true;
        WidgetRef child;
        WidgetRef replacement;

        Visibility() = default;
        explicit Visibility(bool vis, WidgetRef c = nullptr)
            : visible(vis), child(std::move(c))
        {
        }

        WidgetRef build(BuildContext&) const override
        {
            if (visible) return child;
            if (maintain_state)
            {
                auto w = std::make_shared<Offstage>();
                w->offstage = true;
                w->child    = child;
                return w;
            }
            return replacement ? replacement : SizedBox::shrink();
        }
    };

} // namespace systems::leal::campello_widgets
