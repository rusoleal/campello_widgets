#pragma once

#include <campello_widgets/widgets/stateful_widget.hpp>
#include <campello_widgets/widgets/single_child_render_object_widget.hpp>
#include <campello_widgets/widgets/gesture_detector.hpp>
#include <campello_widgets/widgets/overlay.hpp>
#include <campello_widgets/widgets/positioned.hpp>
#include <campello_widgets/ui/drag_details.hpp>
#include <campello_widgets/ui/render_box.hpp>
#include <campello_widgets/ui/pointer_event.hpp>
#include <campello_widgets/ui/pointer_dispatcher.hpp>
#include <campello_widgets/ui/offset.hpp>
#include <campello_widgets/ui/rect.hpp>
#include <campello_widgets/ui/value_notifier.hpp>
#include <campello_widgets/widgets/value_listenable_builder.hpp>

#include <functional>
#include <memory>
#include <typeindex>

namespace systems::leal::campello_widgets
{

    // Forward declarations
    template<typename T> class Draggable;

    // -------------------------------------------------------------------------
    // Internal RenderObject — registers with PointerDispatcher directly so
    // that we get raw PointerEvents with absolute global positions.
    // -------------------------------------------------------------------------

    template<typename T>
    class RenderDraggable : public RenderBox
    {
    public:
        std::function<void(Offset)> on_drag_start;
        std::function<void(Offset)> on_drag_update;
        std::function<void()>       on_drag_end;

        RenderDraggable()
        {
            if (auto* d = PointerDispatcher::activeDispatcher())
                d->addHandler(this, [this](const PointerEvent& e) { onPointerEvent(e); });
        }

        ~RenderDraggable() override
        {
            if (auto* d = PointerDispatcher::activeDispatcher())
                d->removeHandler(this);
        }

        void performLayout() override
        {
            if (child_)
            {
                layoutChild(*child_, constraints_);
                positionChild(*child_, Offset::zero());
                size_ = child_->size();
            }
            else
            {
                size_ = constraints_.constrain({0.0f, 0.0f});
            }
        }

        void performPaint(PaintContext& ctx, const Offset& offset) override
        {
            global_offset_ = offset;
            if (child_) paintChild(ctx, offset);
        }

    private:
        static constexpr float kDragSlop = 10.0f;

        void onPointerEvent(const PointerEvent& event)
        {
            switch (event.kind)
            {
            case PointerEventKind::down:
                pressed_  = true;
                dragging_ = false;
                down_pos_ = event.position;
                last_pos_ = event.position;
                break;

            case PointerEventKind::move:
                if (pressed_)
                {
                    float dx = event.position.x - down_pos_.x;
                    float dy = event.position.y - down_pos_.y;
                    if (!dragging_ && (dx * dx + dy * dy) >= kDragSlop * kDragSlop)
                    {
                        dragging_ = true;
                        if (on_drag_start) on_drag_start(event.position);
                    }
                    if (dragging_)
                    {
                        last_pos_ = event.position;
                        if (on_drag_update) on_drag_update(event.position);
                    }
                }
                break;

            case PointerEventKind::up:
            case PointerEventKind::cancel:
                if (dragging_)
                {
                    dragging_ = false;
                    if (on_drag_end) on_drag_end();
                }
                pressed_  = false;
                break;

            default:
                break;
            }
        }

        bool   pressed_       = false;
        bool   dragging_      = false;
        Offset down_pos_;
        Offset last_pos_;
        Offset global_offset_;
    };

    // -------------------------------------------------------------------------
    // Internal proxy widget → RenderDraggable
    // -------------------------------------------------------------------------

    template<typename T>
    class DraggableProxy : public SingleChildRenderObjectWidget
    {
    public:
        std::function<void(Offset)> on_drag_start;
        std::function<void(Offset)> on_drag_update;
        std::function<void()>       on_drag_end;

        std::shared_ptr<RenderObject> createRenderObject() const override
        {
            auto r = std::make_shared<RenderDraggable<T>>();
            r->on_drag_start  = on_drag_start;
            r->on_drag_update = on_drag_update;
            r->on_drag_end    = on_drag_end;
            return r;
        }

        void updateRenderObject(RenderObject& ro) const override
        {
            auto& r = static_cast<RenderDraggable<T>&>(ro);
            r.on_drag_start  = on_drag_start;
            r.on_drag_update = on_drag_update;
            r.on_drag_end    = on_drag_end;
        }
    };

    // -------------------------------------------------------------------------
    // Draggable State
    // -------------------------------------------------------------------------

    template<typename T>
    class DraggableState : public State<Draggable<T>>
    {
    public:
        void initState() override
        {
            // Ensure a global DragManager exists
            if (!DragManager::active())
            {
                own_manager_ = std::make_unique<DragManager>();
                DragManager::setActive(own_manager_.get());
            }
        }

        void dispose() override
        {
            endDrag(false);
        }

        WidgetRef build(BuildContext&) override
        {
            const auto& w = this->widget();

            auto proxy = std::make_shared<DraggableProxy<T>>();
            proxy->child = dragging_ ? (w.child_when_dragging ? w.child_when_dragging : w.child)
                                     : w.child;

            proxy->on_drag_start  = [this](Offset pos) { onDragStart(pos); };
            proxy->on_drag_update = [this](Offset pos) { onDragUpdate(pos); };
            proxy->on_drag_end    = [this]()             { onDragEnd();      };
            return proxy;
        }

    private:
        void onDragStart(Offset pos)
        {
            drag_data_    = this->widget().data;
            dragging_     = true;
            drag_pos_     = std::make_shared<ValueNotifier<Offset>>(pos);

            // Register with DragManager
            if (DragManager::active())
                DragManager::active()->startSession(typeid(T), &drag_data_, pos);

            // Create feedback overlay using ValueListenableBuilder so the
            // position updates without a full State rebuild
            auto pos_notifier = drag_pos_; // shared_ptr copy
            WidgetRef feedback_widget = this->widget().feedback
                                        ? this->widget().feedback
                                        : this->widget().child;

            auto vlb = std::make_shared<ValueListenableBuilder<Offset>>();
            vlb->valueListenable = pos_notifier;
            vlb->builder = [fw = feedback_widget](BuildContext&, const Offset& p, WidgetRef) -> WidgetRef {
                auto pos_widget = std::make_shared<Positioned>();
                pos_widget->left  = p.x;
                pos_widget->top   = p.y;
                pos_widget->child = fw;
                return pos_widget;
            };

            feedback_entry_ = OverlayEntry::create(vlb);
            Overlay::insert(feedback_entry_);

            if (this->widget().on_drag_started)
                this->widget().on_drag_started();

            this->setState([](){});
        }

        void onDragUpdate(Offset pos)
        {
            if (drag_pos_) drag_pos_->setValue(pos);
            if (DragManager::active()) DragManager::active()->updatePosition(pos);
        }

        void onDragEnd()
        {
            bool accepted = false;
            if (DragManager::active()) accepted = DragManager::active()->endSession();
            endDrag(accepted);
        }

        void endDrag(bool accepted)
        {
            if (!dragging_) return;
            dragging_ = false;

            if (feedback_entry_)
            {
                Overlay::remove(feedback_entry_);
                feedback_entry_.reset();
            }
            drag_pos_.reset();

            if (this->widget().on_drag_ended)
                this->widget().on_drag_ended(accepted);

            this->setState([](){});
        }

        bool                                 dragging_  = false;
        T                                    drag_data_ {};
        std::shared_ptr<ValueNotifier<Offset>> drag_pos_;
        std::shared_ptr<OverlayEntry>        feedback_entry_;
        std::unique_ptr<DragManager>         own_manager_; ///< Created lazily if none exists
    };

    // -------------------------------------------------------------------------
    // Draggable<T>
    // -------------------------------------------------------------------------

    /**
     * @brief Makes its child draggable, showing a feedback widget during the drag.
     *
     * When the user drags the child, `feedback` (or `child` if feedback is null)
     * follows the pointer. When the drag ends on a compatible DragTarget<T>, that
     * target's `on_accept` callback fires with the dragged `data`.
     *
     * @tparam T  The type of data transferred on a successful drop.
     *
     * @code
     * auto d = std::make_shared<Draggable<int>>();
     * d->data     = 42;
     * d->child    = make<Text>("drag me");
     * d->feedback = make<Container>(...); // optional; defaults to child
     * d->on_drag_started = []() { ... };
     * d->on_drag_ended   = [](bool accepted) { ... };
     * @endcode
     */
    template<typename T>
    class Draggable : public StatefulWidget
    {
    public:
        T data {}; ///< The value transferred when the drag is accepted

        WidgetRef child;              ///< Displayed when not dragging
        WidgetRef feedback;           ///< Follows the pointer during drag (null → child)
        WidgetRef child_when_dragging;///< Replaces child during drag (null → child)

        std::function<void()>       on_drag_started;          ///< Called when drag begins
        std::function<void(bool)>   on_drag_ended;            ///< Called when drag ends (accepted?)

        std::unique_ptr<StateBase> createState() const override
        {
            return std::make_unique<DraggableState<T>>();
        }
    };

} // namespace systems::leal::campello_widgets
