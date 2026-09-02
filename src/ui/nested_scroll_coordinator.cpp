#include <campello_widgets/ui/nested_scroll_coordinator.hpp>
#include <cmath>

namespace systems::leal::campello_widgets
{

    namespace
    {
        constexpr float kEpsilon = 1e-4f;
    }

    void NestedScrollCoordinator::applyUserOffset(float delta)
    {
        if (delta > 0.0f)
        {
            // Collapsing direction -- outer absorbs first.
            const float outer_applied = apply_to_outer ? apply_to_outer(delta) : 0.0f;
            const float leftover      = delta - outer_applied;
            if (std::abs(leftover) > kEpsilon && apply_to_inner) apply_to_inner(leftover);
        }
        else if (delta < 0.0f)
        {
            // Expanding direction -- inner absorbs first.
            const float inner_applied = apply_to_inner ? apply_to_inner(delta) : 0.0f;
            const float leftover      = delta - inner_applied;
            if (std::abs(leftover) > kEpsilon && apply_to_outer) apply_to_outer(leftover);
        }
        // delta == 0.0f: intentional no-op, neither callback is invoked.
    }

} // namespace systems::leal::campello_widgets
