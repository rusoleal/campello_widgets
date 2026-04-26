#include <campello_widgets/widgets/checkbox.hpp>
#include <campello_widgets/widgets/theme.hpp>
#include <campello_widgets/ui/custom_painter.hpp>
#include <campello_widgets/ui/canvas.hpp>
#include <campello_widgets/ui/paint.hpp>
#include <campello_widgets/ui/rect.hpp>
#include <campello_widgets/ui/path.hpp>
#include <campello_widgets/ui/rrect.hpp>
#include <campello_widgets/widgets/sized_box.hpp>
#include <campello_widgets/widgets/raw_custom_paint.hpp>
#include <campello_widgets/widgets/gesture_detector.hpp>
#include <campello_widgets/widgets/opacity.hpp>

namespace systems::leal::campello_widgets
{

    // ------------------------------------------------------------------
    // Painter
    // ------------------------------------------------------------------

    class CheckboxPainter : public CustomPainter
    {
    public:
        bool  checked       = false;
        Color active_color;
        Color check_color;
        Color border_color;
        float border_radius = 2.0f;

        void paint(Canvas& canvas, Size size) override
        {
            const Rect  bounds = Rect::fromLTWH(0.0f, 0.0f, size.width, size.height);
            const RRect rrect  = RRect::fromRectAndRadius(bounds, border_radius);

            if (checked)
            {
                // Filled background
                canvas.drawRRect(rrect, Paint::filled(active_color));

                // Checkmark path  ✓
                const float w = size.width;
                const float h = size.height;
                Path path;
                path.moveTo(w * 0.2f, h * 0.5f);
                path.lineTo(w * 0.42f, h * 0.72f);
                path.lineTo(w * 0.8f, h * 0.28f);

                Paint tick = Paint::stroked(check_color, size.width * 0.12f);
                canvas.drawPath(path, tick);
            }
            else
            {
                // Border only
                Paint border = Paint::stroked(border_color, 2.0f);
                canvas.drawRRect(rrect, border);
            }
        }

        bool shouldRepaint(const CustomPainter&) const override { return true; }
    };

    // ------------------------------------------------------------------

    WidgetRef Checkbox::build(BuildContext& ctx) const
    {
        const bool   checked    = value;
        const auto   changed_fn = on_changed;
        const auto*  tokens     = Theme::tokensOf(ctx);
        const Color  ac         = active_color.value_or(tokens->colors.primary);
        const Color  cc         = check_color.value_or(tokens->colors.on_primary);
        const Color  bc         = border_color.value_or(tokens->colors.outline);
        const float  sz         = size;
        const float  br         = border_radius;

        auto painter            = std::make_shared<CheckboxPainter>();
        painter->checked        = checked;
        painter->active_color   = ac;
        painter->check_color    = cc;
        painter->border_color   = bc;
        painter->border_radius  = br;

        auto box    = std::make_shared<SizedBox>();
        box->width  = sz;
        box->height = sz;
        box->child  = RawCustomPaint::create(painter);

        auto detector        = std::make_shared<GestureDetector>();
        detector->on_tap     = changed_fn
            ? std::function<void()>([changed_fn, checked]() { changed_fn(!checked); })
            : std::function<void()>{};
        detector->child      = box;

        if (!changed_fn)
        {
            auto faded     = std::make_shared<Opacity>();
            faded->opacity = 0.40f;
            faded->child   = detector;
            return faded;
        }
        return detector;
    }

} // namespace systems::leal::campello_widgets
