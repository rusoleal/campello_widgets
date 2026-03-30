#include <campello_widgets/widgets/radio.hpp>
#include <campello_widgets/widgets/radio_group.hpp>
#include <campello_widgets/ui/custom_painter.hpp>
#include <campello_widgets/ui/canvas.hpp>
#include <campello_widgets/ui/paint.hpp>
#include <campello_widgets/widgets/sized_box.hpp>
#include <campello_widgets/widgets/raw_custom_paint.hpp>
#include <campello_widgets/widgets/gesture_detector.hpp>
#include <campello_widgets/widgets/build_context.hpp>

namespace systems::leal::campello_widgets
{

    // ------------------------------------------------------------------
    // Painter
    // ------------------------------------------------------------------

    class RadioPainter : public CustomPainter
    {
    public:
        bool  selected = false;
        Color active_color;
        Color inactive_color;

        void paint(Canvas& canvas, Size size) override
        {
            const float cx = size.width  * 0.5f;
            const float cy = size.height * 0.5f;
            const float r  = std::min(cx, cy) - 1.0f;

            // Outer ring
            const Color ring_color = selected ? active_color : inactive_color;
            canvas.drawCircle({cx, cy}, r, Paint::stroked(ring_color, 2.0f));

            // Inner dot when selected
            if (selected)
                canvas.drawCircle({cx, cy}, r * 0.5f, Paint::filled(active_color));
        }

        bool shouldRepaint(const CustomPainter&) const override { return true; }
    };

    // ------------------------------------------------------------------

    WidgetRef Radio::build(BuildContext& ctx) const
    {
        const auto* scope =
            ctx.dependOnInheritedWidgetOfExactType<RadioGroupScope>();

        const bool selected    = scope && scope->group_value == value;
        const int  this_value  = value;
        const auto on_changed  = scope ? scope->on_changed : std::function<void(int)>{};

        auto painter           = std::make_shared<RadioPainter>();
        painter->selected      = selected;
        painter->active_color  = active_color;
        painter->inactive_color = inactive_color;

        auto box    = std::make_shared<SizedBox>();
        box->width  = size;
        box->height = size;
        box->child  = RawCustomPaint::create(painter);

        auto detector    = std::make_shared<GestureDetector>();
        detector->on_tap = on_changed
            ? std::function<void()>([on_changed, this_value]() { on_changed(this_value); })
            : std::function<void()>{};
        detector->child  = box;
        return detector;
    }

} // namespace systems::leal::campello_widgets
