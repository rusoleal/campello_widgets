#include <campello_widgets/widgets/modal_barrier.hpp>
#include <campello_widgets/widgets/gesture_detector.hpp>
#include <campello_widgets/widgets/colored_box.hpp>
#include <campello_widgets/widgets/positioned.hpp>

namespace systems::leal::campello_widgets
{

    WidgetRef ModalBarrier::build(BuildContext& context) const
    {
        (void)context;

        // Create a full-screen colored box with tap handling
        auto colored_box = std::make_shared<ColoredBox>();
        colored_box->color = color;

        if (dismissible && on_dismiss) {
            // Wrap in GestureDetector for tap handling
            auto gesture = std::make_shared<GestureDetector>();
            gesture->child = colored_box;
            gesture->on_tap = on_dismiss;
            return Positioned::fill(gesture);
        }

        return Positioned::fill(colored_box);
    }

} // namespace systems::leal::campello_widgets
