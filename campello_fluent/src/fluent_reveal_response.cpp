#include "fluent_reveal_response.hpp"

#include <campello_widgets/widgets/gesture_detector.hpp>
#include <campello_widgets/widgets/mouse_region.hpp>
#include <campello_widgets/widgets/clip_rrect.hpp>
#include <campello_widgets/widgets/stack.hpp>
#include <campello_widgets/widgets/positioned.hpp>
#include <campello_widgets/widgets/raw_custom_paint.hpp>
#include <campello_widgets/ui/custom_painter.hpp>
#include <campello_widgets/ui/canvas.hpp>
#include <campello_widgets/ui/paint.hpp>
#include <campello_widgets/ui/rect.hpp>
#include <campello_widgets/ui/rrect.hpp>
#include <campello_widgets/ui/animation_controller.hpp>
#include <campello_widgets/ui/system_mouse_cursor.hpp>
#include <campello_widgets/ui/focus_manager.hpp>

#include <algorithm>

namespace systems::leal::campello_widgets
{
    namespace
    {
        // Fluent Reveal state-layer approximations. Press reads clearly
        // stronger than hover (both fill and border), matching WinUI's
        // default ButtonRevealStyle where the pressed state is
        // meaningfully more saturated, not just marginally so.
        constexpr float kHoverFillAlpha     = 0.06f;
        constexpr float kPressedFillAlpha   = 0.14f;
        constexpr float kHoverBorderAlpha   = 0.35f;
        constexpr float kPressedBorderAlpha = 0.55f;
        constexpr float kBorderWidth        = 1.0f;

        // WinUI's FocusVisual: a crisp, fully-opaque 2px rectangle, snapped
        // on/off (not animated/faded like the hover/press tint+glow above)
        // -- a genuinely different visual language from Reveal, not just a
        // stronger version of it.
        constexpr float kFocusStrokeWidth = 2.0f;

        class RevealPainter : public CustomPainter
        {
        public:
            float intensity     = 0.0f; // 0 = idle, 1 = fully faded in (hover or pressed)
            bool  pressed        = false;
            bool  focused        = false; // already gated on FocusHighlightMode::keyboard by the caller
            Color reveal_color;
            float border_radius = 0.0f;

            void paint(Canvas& canvas, Size size) override
            {
                const Rect  bounds = Rect::fromLTWH(0.0f, 0.0f, size.width, size.height);

                if (intensity > 0.0f)
                {
                    const RRect rrect = RRect::fromRectAndRadius(bounds, border_radius);
                    const float fill_target   = pressed ? kPressedFillAlpha   : kHoverFillAlpha;
                    const float border_target = pressed ? kPressedBorderAlpha : kHoverBorderAlpha;

                    // Fill first, border glow on top -- the border is what
                    // actually reads as "Reveal" (a light outline), the fill
                    // is a subtle supporting tint underneath it.
                    canvas.drawRRect(rrect, Paint::filled(Color::fromRGBA(
                        reveal_color.r, reveal_color.g, reveal_color.b, fill_target * intensity)));
                    canvas.drawRRect(rrect, Paint::stroked(Color::fromRGBA(
                        reveal_color.r, reveal_color.g, reveal_color.b, border_target * intensity),
                        kBorderWidth));
                }

                if (focused)
                {
                    // Inset half the stroke width so it stays within this
                    // overlay's clipped bounds -- a true "outside" ring,
                    // like WinUI's real default, would need to paint beyond
                    // the control's own rect, which this shared clipped-
                    // overlay layout (see the header doc comment) does not
                    // support.
                    const Rect  focus_bounds = bounds.inflate(-kFocusStrokeWidth * 0.5f, -kFocusStrokeWidth * 0.5f);
                    const RRect focus_rrect  = RRect::fromRectAndRadius(
                        focus_bounds, std::max(0.0f, border_radius - kFocusStrokeWidth * 0.5f));
                    canvas.drawRRect(focus_rrect, Paint::stroked(reveal_color, kFocusStrokeWidth));
                }
            }

            bool shouldRepaint(const CustomPainter&) const override { return true; }
        };

    } // namespace

    class FluentRevealResponseState : public State<FluentRevealResponse>
    {
    public:
        void initState() override
        {
            ctrl_        = std::make_unique<AnimationController>(150.0);
            listener_id_ = ctrl_->addListener([this]() { setState([](){}); });
        }

        void dispose() override
        {
            if (ctrl_ && listener_id_ != 0)
                ctrl_->removeListener(listener_id_);
        }

        WidgetRef build(BuildContext&) override
        {
            const auto& w           = widget();
            const bool  interactive = static_cast<bool>(w.on_tap);

            auto painter           = std::make_shared<RevealPainter>();
            painter->intensity     = ctrl_ ? static_cast<float>(ctrl_->value()) : 0.0f;
            painter->pressed       = interactive && pressed_;
            painter->focused       = interactive && focused_ &&
                FocusManager::highlightMode() == FocusHighlightMode::keyboard;
            painter->reveal_color  = w.reveal_color;
            painter->border_radius = w.border_radius;

            // Only the overlay is clipped, never w.child -- see this
            // class's header doc comment for why clipping the content
            // itself would cut off a stroked border's outward bleed.
            auto overlay          = RawCustomPaint::create(painter);
            auto clipped_overlay  = std::make_shared<ClipRRect>(w.border_radius, overlay);
            auto positioned_overlay = std::make_shared<Positioned>(
                0.0f, 0.0f, 0.0f, 0.0f, std::optional<float>{}, std::optional<float>{}, clipped_overlay);

            auto stack = std::make_shared<Stack>();
            stack->children = {w.child, positioned_overlay};

            if (!interactive)
            {
                // No callbacks wired at all -- matches a disabled control:
                // no hover cursor, no reveal, just the content.
                return stack;
            }

            auto gesture             = std::make_shared<GestureDetector>();
            gesture->child           = stack;
            gesture->on_tap          = w.on_tap;
            gesture->focus_node      = w.focus_node;
            gesture->autofocus       = w.autofocus;
            gesture->focusable       = true;
            gesture->on_press_change = [this](bool pressed) {
                pressed_ = pressed;
                updateTarget();
            };
            gesture->on_focus_change = [this](bool has_focus) {
                focused_ = has_focus;
                setState([](){});
            };

            auto hoverable      = std::make_shared<MouseRegion>();
            hoverable->cursor   = SystemMouseCursor::pointer;
            hoverable->on_enter = [this]() { hovered_ = true;  updateTarget(); };
            hoverable->on_exit  = [this]() { hovered_ = false; updateTarget(); };
            hoverable->child    = gesture;

            return hoverable;
        }

    private:
        void updateTarget()
        {
            // Pressed and hovered both fade the same overlay in -- the
            // painter distinguishes intensity (how faded-in) from pressed
            // (which alpha targets to use), so releasing a press while
            // still hovered smoothly settles at the hover level instead of
            // fading all the way out and back in.
            const bool active = hovered_ || pressed_;
            if (ctrl_) active ? ctrl_->forward() : ctrl_->reverse();
            setState([](){});
        }

        std::unique_ptr<AnimationController> ctrl_;
        uint64_t listener_id_ = 0;
        bool     hovered_ = false;
        bool     pressed_ = false;
        bool     focused_ = false;
    };

    std::unique_ptr<StateBase> FluentRevealResponse::createState() const
    {
        return std::make_unique<FluentRevealResponseState>();
    }

} // namespace systems::leal::campello_widgets
