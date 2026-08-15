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

namespace systems::leal::campello_widgets
{

    class TooltipState : public State<Tooltip>
    {
    public:
        void dispose() override
        {
            dismissTooltip();
            if (dismiss_ctrl_ && dismiss_listener_ != 0)
                dismiss_ctrl_->removeListener(dismiss_listener_);
        }

        WidgetRef build(BuildContext&) override
        {
            const auto& w = widget();

            auto detector              = std::make_shared<GestureDetector>();
            detector->on_long_press    = [this]() { showTooltip(); };
            detector->child            = w.child;
            return detector;
        }

    private:
        std::shared_ptr<OverlayEntry>            entry_;
        std::unique_ptr<AnimationController>     dismiss_ctrl_;
        uint64_t                                 dismiss_listener_ = 0;

        void showTooltip()
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

            // Position: centred near the top of the screen
            auto aligned      = std::make_shared<Align>();
            aligned->alignment = Alignment::topCenter();
            aligned->child    = bubble;

            entry_ = std::make_shared<OverlayEntry>(aligned);
            Overlay::insert(entry_);

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
