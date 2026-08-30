#pragma once

#include <campello_widgets/widgets/single_child_render_object_widget.hpp>
#include <campello_widgets/ui/pointer_event.hpp>

#include <functional>

namespace systems::leal::campello_widgets
{

    /**
     * @brief Exposes raw pointer events over its child with no gesture
     * recognition/disambiguation.
     *
     * Use `GestureDetector` for tap/pan/long-press semantics; use `Listener`
     * when you need the unprocessed down/move/up/cancel/scroll stream
     * directly (matching Flutter's `Listener` widget).
     *
     * @code
     * auto w = std::make_shared<Listener>();
     * w->on_pointer_down = [](const PointerEvent& e) { log(e.position); };
     * w->child = someChild;
     * @endcode
     */
    class Listener : public SingleChildRenderObjectWidget
    {
    public:
        std::function<void(const PointerEvent&)> on_pointer_down;
        std::function<void(const PointerEvent&)> on_pointer_move;
        std::function<void(const PointerEvent&)> on_pointer_up;
        std::function<void(const PointerEvent&)> on_pointer_cancel;
        std::function<void(const PointerEvent&)> on_pointer_signal;

        Listener() = default;
        explicit Listener(WidgetRef c) { child = std::move(c); }

        std::shared_ptr<RenderObject> createRenderObject() const override;
        void updateRenderObject(RenderObject& ro) const override;
    };

} // namespace systems::leal::campello_widgets
