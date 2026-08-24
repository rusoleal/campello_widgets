#include "cupertino_focus_ring.hpp"

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
#include <campello_widgets/ui/system_mouse_cursor.hpp>
#include <campello_widgets/ui/focus_manager.hpp>

#include <algorithm>

namespace systems::leal::campello_widgets
{
    namespace
    {
        // Matches CupertinoButton's real focus ring: a solid 3.5pt stroke,
        // snapped on/off (no fade-in animation -- unlike Material/Fluent's
        // hover/press feedback, Cupertino has none of that here yet, so
        // there is no shared AnimationController to piggyback on).
        constexpr float kFocusStrokeWidth = 3.5f;

        class FocusRingPainter : public CustomPainter
        {
        public:
            bool  focused        = false; // already gated on FocusHighlightMode::keyboard by the caller
            Color focus_color;
            float border_radius  = 0.0f;

            void paint(Canvas& canvas, Size size) override
            {
                if (!focused) return;

                const Rect bounds = Rect::fromLTWH(0.0f, 0.0f, size.width, size.height);
                // Inset half the stroke width so it stays within this
                // overlay's clipped bounds -- a true outside-aligned stroke
                // (Flutter's BorderSide.strokeAlignOutside) would need to
                // paint beyond the control's own rect, which this shared
                // clipped-overlay layout does not support. See this class's
                // header doc comment.
                const Rect  focus_bounds = bounds.inflate(-kFocusStrokeWidth * 0.5f, -kFocusStrokeWidth * 0.5f);
                const RRect focus_rrect  = RRect::fromRectAndRadius(
                    focus_bounds, std::max(0.0f, border_radius - kFocusStrokeWidth * 0.5f));
                canvas.drawRRect(focus_rrect, Paint::stroked(focus_color, kFocusStrokeWidth));
            }

            bool shouldRepaint(const CustomPainter&) const override { return true; }
        };

    } // namespace

    class CupertinoFocusRingState : public State<CupertinoFocusRing>
    {
    public:
        WidgetRef build(BuildContext&) override
        {
            const auto& w           = widget();
            const bool  interactive = static_cast<bool>(w.on_tap);

            auto painter           = std::make_shared<FocusRingPainter>();
            painter->focused       = interactive && focused_ &&
                FocusManager::highlightMode() == FocusHighlightMode::keyboard;
            painter->focus_color   = w.focus_color;
            painter->border_radius = w.border_radius;

            auto overlay          = RawCustomPaint::create(painter);
            auto clipped_overlay  = std::make_shared<ClipRRect>(w.border_radius, overlay);
            auto positioned_overlay = std::make_shared<Positioned>(
                0.0f, 0.0f, 0.0f, 0.0f, std::optional<float>{}, std::optional<float>{}, clipped_overlay);

            auto stack = std::make_shared<Stack>();
            stack->children = {w.child, positioned_overlay};

            if (!interactive)
            {
                // No callbacks wired at all -- matches a disabled control:
                // no focus, no ring, just the content.
                return stack;
            }

            auto gesture          = std::make_shared<GestureDetector>();
            gesture->child        = stack;
            gesture->on_tap       = w.on_tap;
            gesture->focus_node   = w.focus_node;
            gesture->autofocus    = w.autofocus;
            gesture->focusable    = true;
            gesture->on_focus_change = [this](bool has_focus) {
                focused_ = has_focus;
                setState([](){});
            };

            auto hoverable      = std::make_shared<MouseRegion>();
            hoverable->cursor   = SystemMouseCursor::pointer;
            hoverable->child    = gesture;

            return hoverable;
        }

    private:
        bool focused_ = false;
    };

    std::unique_ptr<StateBase> CupertinoFocusRing::createState() const
    {
        return std::make_unique<CupertinoFocusRingState>();
    }

} // namespace systems::leal::campello_widgets
