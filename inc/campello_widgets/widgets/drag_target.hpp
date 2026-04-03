#pragma once

#include <campello_widgets/widgets/stateful_widget.hpp>
#include <campello_widgets/widgets/single_child_render_object_widget.hpp>
#include <campello_widgets/ui/drag_details.hpp>
#include <campello_widgets/ui/render_box.hpp>
#include <campello_widgets/ui/paint_context.hpp>
#include <campello_widgets/ui/offset.hpp>
#include <campello_widgets/ui/rect.hpp>

#include <functional>
#include <memory>
#include <typeindex>

namespace systems::leal::campello_widgets
{

    // Forward declarations
    template<typename T> class DragTarget;

    // -------------------------------------------------------------------------
    // RenderDragTarget — registers bounds with DragManager each paint cycle
    // -------------------------------------------------------------------------

    template<typename T>
    class RenderDragTarget : public RenderBox
    {
    public:
        std::function<bool(const T&)>  will_accept;
        std::function<void(const T&)>  on_accept;
        std::function<void()>          on_enter;
        std::function<void()>          on_exit;

        RenderDragTarget()
        {
            if (auto* mgr = DragManager::active())
                registerWithManager(mgr);
        }

        ~RenderDragTarget() override
        {
            if (auto* mgr = DragManager::active())
                mgr->unregisterTarget(this);
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

            // Update bounds with DragManager
            if (auto* mgr = DragManager::active())
            {
                if (!registered_)
                    registerWithManager(mgr);

                Rect bounds = Rect::fromLTWH(
                    global_offset_.x, global_offset_.y, size_.width, size_.height);
                mgr->updateTargetBounds(this, bounds);
            }

            if (child_) paintChild(ctx, offset);
        }

    private:
        void registerWithManager(DragManager* mgr)
        {
            registered_ = true;
            mgr->registerTarget(
                this,
                [this]() -> bool {
                    if (!DragManager::active()) return false;
                    if (DragManager::active()->sessionType() != typeid(T)) return false;
                    if (!will_accept) return true;
                    const T& data = *static_cast<const T*>(DragManager::active()->sessionData());
                    return will_accept(data);
                },
                [this]() { if (on_enter) on_enter(); },
                [this]() { if (on_exit)  on_exit();  },
                [this]() {
                    if (!on_accept || !DragManager::active()) return;
                    const T& data = *static_cast<const T*>(DragManager::active()->sessionData());
                    on_accept(data);
                });
        }

        bool   registered_   = false;
        Offset global_offset_;
    };

    // -------------------------------------------------------------------------
    // DragTargetProxy widget → RenderDragTarget
    // -------------------------------------------------------------------------

    template<typename T>
    class DragTargetProxy : public SingleChildRenderObjectWidget
    {
    public:
        std::function<bool(const T&)>  will_accept;
        std::function<void(const T&)>  on_accept;
        std::function<void()>          on_enter;
        std::function<void()>          on_exit;

        std::shared_ptr<RenderObject> createRenderObject() const override
        {
            auto r = std::make_shared<RenderDragTarget<T>>();
            applyTo(*r);
            return r;
        }

        void updateRenderObject(RenderObject& ro) const override
        {
            applyTo(static_cast<RenderDragTarget<T>&>(ro));
        }

    private:
        void applyTo(RenderDragTarget<T>& r) const
        {
            r.will_accept = will_accept;
            r.on_accept   = on_accept;
            r.on_enter    = on_enter;
            r.on_exit     = on_exit;
        }
    };

    // -------------------------------------------------------------------------
    // DragTarget State
    // -------------------------------------------------------------------------

    template<typename T>
    class DragTargetState : public State<DragTarget<T>>
    {
    public:
        void initState() override { hovering_ = false; }

        WidgetRef build(BuildContext& ctx) override
        {
            const auto& w = this->widget();

            auto proxy = std::make_shared<DragTargetProxy<T>>();
            proxy->will_accept = w.on_will_accept;
            proxy->on_accept   = w.on_accept;
            proxy->on_enter    = [this]() {
                this->setState([this]() { hovering_ = true; });
            };
            proxy->on_exit     = [this]() {
                this->setState([this]() { hovering_ = false; });
            };
            proxy->child = w.builder(ctx, hovering_);
            return proxy;
        }

    private:
        bool hovering_ = false;
    };

    // -------------------------------------------------------------------------
    // DragTarget<T>
    // -------------------------------------------------------------------------

    /**
     * @brief A widget that accepts drops from a compatible Draggable<T>.
     *
     * The `builder` function is called each frame with the current hover state.
     * If `on_will_accept` is null, all drops of type T are accepted.
     *
     * @tparam T  The type of data this target accepts.
     *
     * @code
     * auto target = std::make_shared<DragTarget<int>>();
     * target->builder = [](BuildContext&, bool hovering) {
     *     return make<ColoredBox>(hovering ? Color::blue() : Color::grey());
     * };
     * target->on_will_accept = [](const int& v) { return v > 0; };
     * target->on_accept      = [](const int& v) { handleDrop(v); };
     * @endcode
     */
    template<typename T>
    class DragTarget : public StatefulWidget
    {
    public:
        /** @brief Builds the target's UI. `is_hovering` is true while a compatible
         *         drag is over this target. */
        std::function<WidgetRef(BuildContext&, bool is_hovering)> builder;

        /** @brief Returns true if the drag data is acceptable. Null → accept all. */
        std::function<bool(const T&)> on_will_accept;

        /** @brief Called when a compatible drag is dropped here. */
        std::function<void(const T&)> on_accept;

        DragTarget() = default;
        explicit DragTarget(std::function<WidgetRef(BuildContext&, bool)> b)
            : builder(std::move(b))
        {}
        explicit DragTarget(
            std::function<WidgetRef(BuildContext&, bool)> b,
            std::function<void(const T&)> on_acc)
            : builder(std::move(b)), on_accept(std::move(on_acc))
        {}

        std::unique_ptr<StateBase> createState() const override
        {
            return std::make_unique<DragTargetState<T>>();
        }
    };

} // namespace systems::leal::campello_widgets
