#pragma once

#include <memory>
#include <campello_widgets/widgets/stateful_widget.hpp>
#include <campello_widgets/ui/scroll_controller.hpp>
#include <campello_widgets/ui/color.hpp>

namespace systems::leal::campello_widgets
{

    /**
     * @brief Draws a draggable, proportional scroll-position thumb over its
     * child, tracking `controller`.
     *
     * Wraps `child` in a `LayoutBuilder` (to learn the viewport's own pixel
     * extent) and a `Stack` with a `Positioned` thumb rebuilt on every
     * `controller` offset change -- see `computeScrollbarThumbGeometry()`
     * (`ui/scrollbar_geometry.hpp`) for the thumb-size math, and
     * `GestureDetector::on_pan_update` for drag-to-scroll, converting thumb
     * drag pixels to scroll offset via the track/content ratio.
     *
     * Matches Flutter's `Scrollbar` widget (always-visible / non-fading
     * variant only).
     *
     * @code
     * auto ctrl = std::make_shared<ScrollController>();
     * auto w = std::make_shared<Scrollbar>();
     * w->controller = ctrl;
     * w->child = std::make_shared<ListView>(...);
     * @endcode
     */
    class Scrollbar : public StatefulWidget
    {
    public:
        std::shared_ptr<ScrollController> controller;
        WidgetRef                         child;
        bool                              is_vertical      = true;
        float                             thickness        = 6.0f;
        float                             min_thumb_length = 24.0f;
        Color                             thumb_color      = Color::fromRGBA(0.0f, 0.0f, 0.0f, 0.35f);

        Scrollbar() = default;
        explicit Scrollbar(std::shared_ptr<ScrollController> c, WidgetRef ch)
            : controller(std::move(c)), child(std::move(ch))
        {
        }

        std::unique_ptr<StateBase> createState() const override;
    };

} // namespace systems::leal::campello_widgets
