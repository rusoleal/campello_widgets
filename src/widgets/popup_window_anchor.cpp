#include <campello_widgets/widgets/popup_window_anchor.hpp>
#include <campello_widgets/widgets/single_child_render_object_widget.hpp>
#include <campello_widgets/widgets/stack.hpp>
#include <campello_widgets/widgets/positioned.hpp>
#include <campello_widgets/widgets/overlay.hpp>
#include <campello_widgets/widgets/focus.hpp>
#include <campello_widgets/widgets/window.hpp>
#include <campello_widgets/widgets/element.hpp>
#include <campello_widgets/widgets/stateful_element.hpp>
#include <campello_widgets/ui/render_box.hpp>
#include <campello_widgets/ui/pointer_dispatcher.hpp>
#include <campello_widgets/ui/focus_node.hpp>
#include <campello_widgets/ui/key_event.hpp>
#include <campello_widgets/ui/view.hpp>
#include <campello_widgets/ui/platform_windowing.hpp>

#include <cstdio>

namespace systems::leal::campello_widgets
{
    namespace
    {
        // Where the anchor was last painted, in its window's coordinates.
        struct AnchorGeometry
        {
            Offset offset;
            Size   size{0.0f, 0.0f};
            bool   painted = false;
        };

        // Lays out and paints its child unchanged, recording where.
        class RenderPopupAnchor : public RenderBox
        {
        public:
            std::shared_ptr<AnchorGeometry> geometry;

            void performLayout() override
            {
                if (child_)
                {
                    layoutChild(*child_, constraints_);
                    size_ = child_->size();
                }
                else
                {
                    size_ = Size{constraints_.min_width, constraints_.min_height};
                }
            }

            void performPaint(PaintContext& context, const Offset& offset) override
            {
                if (geometry)
                {
                    geometry->offset  = offset;
                    geometry->size    = size_;
                    geometry->painted = true;
                }
                if (child_) paintChild(context, offset);
            }
        };

        class PopupAnchorLeaf : public SingleChildRenderObjectWidget
        {
        public:
            std::shared_ptr<AnchorGeometry> geometry;

            std::shared_ptr<RenderObject> createRenderObject() const override
            {
                auto r = std::make_shared<RenderPopupAnchor>();
                r->geometry = geometry;
                return r;
            }

            void updateRenderObject(RenderObject& render_object) const override
            {
                static_cast<RenderPopupAnchor&>(render_object).geometry = geometry;
            }
        };

        bool contains(Offset origin, Size size, Offset p)
        {
            return p.x >= origin.x && p.y >= origin.y &&
                   p.x < origin.x + size.width && p.y < origin.y + size.height;
        }
    } // namespace

    class PopupWindowAnchorState : public State<PopupWindowAnchor>
    {
    public:
        void initState() override
        {
            geometry_ = std::make_shared<AnchorGeometry>();
            escape_   = std::make_shared<FocusNode>();
            escape_->on_key = [this](const KeyEvent& e) {
                if (e.kind == KeyEventKind::down && e.key_code == KeyCode::escape)
                {
                    dismiss();
                    return true;
                }
                return false;
            };
            if (widget().open) requestOpen();
        }

        void didUpdateWidget(const Widget&) override
        {
            if (widget().open && !isOpen()) requestOpen();
            else if (!widget().open && isOpen()) close();
            else if (entry_)
            {
                // The in-window popup shows the current content.
                entry_->child = positionedPopup();
                if (auto* s = entry_->entryState()) s->markNeedsBuild();
            }
        }

        void dispose() override { close(/*rebuild=*/false); }

        WidgetRef build(BuildContext&) override
        {
            auto leaf      = std::make_shared<PopupAnchorLeaf>();
            leaf->geometry = geometry_;
            leaf->child    = widget().child;
            if (!window_) return leaf;

            // The popup window's content lives here; the Window widget is an
            // empty leaf in this window.
            return Stack::create(StackFit::passthrough, {
                leaf, std::make_shared<Window>(window_, focusedPopup())});
        }

    private:
        bool isOpen() const noexcept { return window_ != nullptr || entry_ != nullptr; }

        WidgetRef focusedPopup() const
        {
            auto focus        = std::make_shared<Focus>();
            focus->focus_node = escape_;
            focus->auto_focus = true;
            focus->child      = widget().popup;
            return focus;
        }

        WidgetRef positionedPopup() const
        {
            const Offset at = popupOrigin();
            return std::make_shared<Positioned>(at.x, at.y, std::nullopt, std::nullopt,
                                                widget().popup_size.width, widget().popup_size.height,
                                                focusedPopup());
        }

        Offset popupOrigin() const
        {
            return Offset{ geometry_->offset.x + widget().offset.x,
                           geometry_->offset.y + geometry_->size.height + widget().offset.y };
        }

        // Opens once the anchor has been painted (its position is known).
        void requestOpen()
        {
            if (geometry_->painted) { open(); open_retries_ = 0; return; }
            // Not painted yet: try again on the next UI turn (a few times;
            // a platform without a run loop runs posted tasks immediately).
            if (open_retry_pending_ || ++open_retries_ > 8) return;
            open_retry_pending_ = true;
            std::weak_ptr<int> alive = alive_;
            auto retry = [this, alive] {
                if (alive.expired()) return;
                open_retry_pending_ = false;
                if (widget().open && !isOpen()) requestOpen();
            };
            if (auto* windowing = PlatformWindowing::instance()) windowing->postTask(retry);
        }

        void open()
        {
            View* view = element() ? View::of(*element()) : View::current();
            const Offset at = popupOrigin();

            if (widget().native && PlatformWindowing::instance())
            {
                WindowController* owner = WindowController::forView(view);
                WindowOptions options;
                options.title    = "";
                options.size     = widget().popup_size;
                options.position = at;
                options.kind     = WindowKind::popup;
                options.parent   = owner ? owner->platformWindow() : nullptr;
                window_ = WindowController::create(options);
                if (window_)
                {
                    std::weak_ptr<int> alive = alive_;
                    // Lost focus (or the platform closed it): dismissed.
                    window_->on_close_requested = [this, alive](WindowController&) {
                        if (!alive.expired()) dismiss();
                    };
                    window_->on_destroyed = [this, alive] {
                        if (alive.expired() || !window_) return;
                        setState([this] { window_.reset(); });
                        stopObserving();
                    };
                }
            }

            if (!window_)
            {
                // In the window's own Overlay, at the same place.
                entry_ = std::make_shared<OverlayEntry>(positionedPopup());
                if (auto* overlay = Overlay::forView(view)) overlay->insert(entry_);
                else if (Overlay::globalState()) Overlay::insert(entry_);
                else std::fprintf(stderr, "[campello_widgets] PopupWindowAnchor: no native popup windows "
                                          "here and no Overlay in this window to show the popup in\n");
            }

            observeOutsidePresses(view);
            setState([] {});
        }

        void observeOutsidePresses(View* view)
        {
            stopObserving();
            if (!view) return;
            observed_ = &view->pointerDispatcher();
            std::weak_ptr<int> alive = alive_;
            observer_ = observed_->addPointerDownObserver([this, alive](const PointerEvent& e) {
                if (alive.expired()) return;
                // The anchor itself toggles the popup on its own (e.g. a
                // button): leave that press to it.
                if (contains(geometry_->offset, geometry_->size, e.position)) return;
                if (entry_ && contains(popupOrigin(), widget().popup_size, e.position)) return;
                dismiss();
            });
        }

        void stopObserving()
        {
            if (observed_ && PointerDispatcher::isAlive(observed_)) observed_->removePointerDownObserver(observer_);
            observed_ = nullptr;
            observer_ = 0;
        }

        void dismiss()
        {
            if (!isOpen()) return;
            if (widget().on_dismiss) widget().on_dismiss();  // normally sets open = false
            else close();
        }

        void close(bool rebuild = true)
        {
            stopObserving();
            if (window_)
            {
                auto window = std::move(window_);
                window_.reset();
                window->on_close_requested = nullptr;
                window->on_destroyed       = nullptr;
                window->destroy();
            }
            if (entry_)
            {
                Overlay::remove(entry_);
                entry_.reset();
            }
            if (rebuild && element()) setState([] {});
        }

        std::shared_ptr<AnchorGeometry>     geometry_;
        std::shared_ptr<FocusNode>          escape_;
        std::shared_ptr<WindowController>   window_;  // native popup
        std::shared_ptr<OverlayEntry>       entry_;   // in-window fallback
        PointerDispatcher*                  observed_ = nullptr;
        PointerDispatcher::DownObserverId   observer_ = 0;
        bool                                open_retry_pending_ = false;
        int                                 open_retries_ = 0;
        std::shared_ptr<int>                alive_ = std::make_shared<int>(0);
    };

    std::unique_ptr<StateBase> PopupWindowAnchor::createState() const
    {
        return std::make_unique<PopupWindowAnchorState>();
    }

} // namespace systems::leal::campello_widgets
