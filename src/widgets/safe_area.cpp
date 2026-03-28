#include <campello_widgets/widgets/safe_area.hpp>
#include <campello_widgets/widgets/padding.hpp>
#include <campello_widgets/ui/renderer.hpp>

namespace systems::leal::campello_widgets
{

    WidgetRef SafeArea::build(BuildContext& context) const
    {
        (void)context;

        // Get view insets from the current renderer if available.
        // This is a temporary solution until MediaQuery is implemented.
        EdgeInsets view_insets;
        Renderer* renderer = detail::currentRenderer();
        if (renderer != nullptr) {
            view_insets = renderer->viewInsets();
        }

        // Build the effective padding based on enabled edges.
        EdgeInsets effective = minimum;
        if (left)   effective.left   += view_insets.left;
        if (top)    effective.top    += view_insets.top;
        if (right)  effective.right  += view_insets.right;
        if (bottom) effective.bottom += view_insets.bottom;

        // If no padding needed, just return the child directly.
        if (effective.left == 0.0f && effective.top == 0.0f &&
            effective.right == 0.0f && effective.bottom == 0.0f) {
            return child;
        }

        // Wrap child in Padding with the effective insets.
        auto padding = std::make_shared<Padding>();
        padding->padding = effective;
        padding->child = child;
        return padding;
    }

} // namespace systems::leal::campello_widgets
