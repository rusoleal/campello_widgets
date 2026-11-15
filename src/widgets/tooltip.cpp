#include <campello_widgets/widgets/tooltip.hpp>
#include <campello_widgets/widgets/theme.hpp>
#include <campello_widgets/widgets/stateful_element.hpp>
#include <campello_widgets/ui/animation_controller.hpp>
#include <campello_widgets/widgets/overlay.hpp>
#include <campello_widgets/widgets/gesture_detector.hpp>
#include <campello_widgets/widgets/padding.hpp>
#include <campello_widgets/widgets/decorated_box.hpp>
#include <campello_widgets/widgets/backdrop_filter.hpp>
#include <campello_widgets/widgets/align.hpp>
#include <campello_widgets/widgets/positioned.hpp>
#include <campello_widgets/widgets/stack.hpp>
#include <campello_widgets/widgets/text.hpp>
#include <campello_widgets/widgets/opacity.hpp>
#include <campello_widgets/ui/text_style.hpp>
#include <campello_widgets/ui/box_decoration.hpp>
#include <campello_widgets/ui/alignment.hpp>
#include <campello_widgets/ui/key.hpp>
#include <campello_widgets/ui/render_dropdown_menu_positioner.hpp>
#include <campello_widgets/ui/render_gesture_detector.hpp>
#include <campello_widgets/ui/stack_fit.hpp>
#include <campello_widgets/widgets/ignore_pointer.hpp>
#include <campello_widgets/widgets/mouse_region.hpp>
#include <campello_widgets/widgets/single_child_render_object_widget.hpp>

namespace systems::leal::campello_widgets
{

    namespace
    {
        // The bubble anchored to the child: centred under it, or over it
        // when there's no room below (RenderDropdownMenuPositioner).
        class TooltipPositioner : public SingleChildRenderObjectWidget
        {
        public:
            Offset anchor_pos;
            Size   anchor_size = Size::zero();

            std::shared_ptr<RenderObject> createRenderObject() const override
            {
                auto r = std::make_shared<RenderDropdownMenuPositioner>();
                r->anchor_pos          = anchor_pos;
                r->anchor_size         = anchor_size;
                r->center_horizontally = true;
                return r;
            }

            void updateRenderObject(RenderObject& ro) const override
            {
                auto& r = static_cast<RenderDropdownMenuPositioner&>(ro);
                r.anchor_pos          = anchor_pos;
                r.anchor_size         = anchor_size;
                r.center_horizontally = true;
                r.markNeedsLayout();
            }
        };
    }

    class TooltipState : public State<Tooltip>
    {
    public:
        void initState() override
        {
            anchor_key_ = std::make_shared<GlobalKey>();
        }

        void dispose() override
        {
            dismissTooltip();
            if (dismiss_ctrl_ && dismiss_listener_ != 0)
                dismiss_ctrl_->removeListener(dismiss_listener_);
            cancelWait();
        }

        WidgetRef build(BuildContext&) override
        {
            const auto& w = widget();

            auto detector              = std::make_shared<GestureDetector>();
            detector->key              = anchor_key_;
            detector->on_long_press    = [this]() { showTooltip(false); };
            detector->child            = w.child;
            if (!w.show_on_hover) return detector;

            auto region      = std::make_shared<MouseRegion>();
            region->on_enter = [this]() { startWait(); };
            region->on_exit  = [this]() {
                cancelWait();
                if (hovered_) dismissTooltip();
                hovered_ = false;
            };
            region->child = detector;
            return region;
        }

    private:
        std::shared_ptr<OverlayEntry>            entry_;
        std::unique_ptr<AnimationController>     dismiss_ctrl_;
        uint64_t                                 dismiss_listener_ = 0;
        std::shared_ptr<GlobalKey>               anchor_key_;
        std::unique_ptr<AnimationController>     wait_ctrl_;
        uint64_t                                 wait_listener_ = 0;
        bool                                     hovered_ = false;

        // Hover: show after the wait, unless the pointer leaves first.
        void startWait()
        {
            cancelWait();
            wait_ctrl_     = std::make_unique<AnimationController>(widget().wait_duration_ms);
            wait_listener_ = wait_ctrl_->addListener([this]() {
                if (wait_ctrl_ && wait_ctrl_->normalizedValue() >= 1.0) {
                    hovered_ = true;
                    showTooltip(true);
                }
            });
            wait_ctrl_->forward(0.0);
        }

        void cancelWait()
        {
            if (wait_ctrl_ && wait_listener_ != 0) wait_ctrl_->removeListener(wait_listener_);
            wait_listener_ = 0;
            wait_ctrl_.reset();
        }

        void showTooltip(bool fromHover)
        {
            // Already visible — reset the timer
            if (entry_) {
                if (dismiss_ctrl_)
                    dismiss_ctrl_->forward(0.0);
                return;
            }

            const auto& w = widget();
            const auto* tokens = Theme::tokensOf(*element());
            const Color bg = w.background_color.value_or(tokens->colors.inverse_surface);
            const Color fg = w.text_color.value_or(tokens->colors.inverse_on_surface);

            // Build the tooltip bubble
            TextStyle ts;
            ts.color     = fg;
            ts.font_size = w.font_size;

            auto label = std::make_shared<Text>(w.message, ts);

            auto padded        = std::make_shared<Padding>();
            padded->padding    = w.padding;
            padded->child      = label;

            WidgetRef bubble;
            if (w.backdrop_filter.has_value()) {
                // See PopupMenuButton::open()'s identical composition for
                // why the shadow needs its own DecoratedBox wrapping the
                // BackdropFilter rather than a flat fill.
                auto bf    = std::make_shared<BackdropFilter>();
                bf->filter = *w.backdrop_filter;
                bf->child  = padded;

                BoxDecoration shadow_deco;
                shadow_deco.border_radius = w.border_radius;
                auto shadowed        = std::make_shared<DecoratedBox>();
                shadowed->decoration = shadow_deco;
                shadowed->child      = bf;
                bubble = shadowed;
            } else {
                BoxDecoration deco;
                deco.color         = bg;
                deco.border_radius = w.border_radius;

                auto decorated_bubble  = std::make_shared<DecoratedBox>();
                decorated_bubble->decoration = deco;
                decorated_bubble->child      = padded;
                bubble = decorated_bubble;
            }

            // Position: anchored to the child when it's on screen (centred
            // under it, or over it); else near the top of the screen.
            Offset anchor_pos;
            Size   anchor_size = Size::zero();
            bool   anchored    = false;
            if (auto* elem = anchor_key_ ? anchor_key_->currentElement() : nullptr) {
                if (auto* roe = elem->findDescendantRenderObjectElement()) {
                    if (auto box = std::dynamic_pointer_cast<RenderGestureDetector>(roe->sharedRenderObject())) {
                        anchor_pos  = box->globalOffset();
                        anchor_size = box->size();
                        anchored    = true;
                    }
                }
            }
            WidgetRef placed;
            if (anchored) {
                auto positioner         = std::make_shared<TooltipPositioner>();
                positioner->anchor_pos  = anchor_pos;
                positioner->anchor_size = anchor_size;
                positioner->child       = bubble;
                auto stack = Stack::create({Positioned::fill(positioner)});
                stack->fit = StackFit::expand;
                placed = stack;
            } else {
                auto aligned       = std::make_shared<Align>();
                aligned->alignment = Alignment::topCenter();
                aligned->child     = bubble;
                placed = aligned;
            }
            // Never in the pointer's way (it would leave the child and hide it).
            auto ignoring = std::make_shared<IgnorePointer>(true, placed);

            entry_ = std::make_shared<OverlayEntry>(ignoring);
            Overlay::insert(entry_);

            // Shown by hovering: stays until the pointer leaves.
            if (fromHover) return;

            // Auto-dismiss timer using AnimationController
            if (dismiss_ctrl_ && dismiss_listener_ != 0)
                dismiss_ctrl_->removeListener(dismiss_listener_);

            dismiss_ctrl_     = std::make_unique<AnimationController>(w.display_duration_ms);
            dismiss_listener_ = dismiss_ctrl_->addListener([this]() {
                if (dismiss_ctrl_->normalizedValue() >= 1.0)
                    dismissTooltip();
            });
            dismiss_ctrl_->forward(0.0);
        }

        void dismissTooltip()
        {
            if (!entry_) return;
            Overlay::remove(entry_);
            entry_.reset();
        }
    };

    // ------------------------------------------------------------------

    std::unique_ptr<StateBase> Tooltip::createState() const
    {
        return std::make_unique<TooltipState>();
    }

} // namespace systems::leal::campello_widgets
