#include <campello_widgets/widgets/switch.hpp>
#include <campello_widgets/ui/animation_controller.hpp>
#include <campello_widgets/ui/custom_painter.hpp>
#include <campello_widgets/ui/canvas.hpp>
#include <campello_widgets/ui/paint.hpp>
#include <campello_widgets/ui/rect.hpp>
#include <campello_widgets/ui/rrect.hpp>
#include <campello_widgets/widgets/sized_box.hpp>
#include <campello_widgets/widgets/raw_custom_paint.hpp>
#include <campello_widgets/widgets/gesture_detector.hpp>
#include <campello_widgets/widgets/opacity.hpp>

#include <algorithm>

namespace systems::leal::campello_widgets
{

    // ------------------------------------------------------------------
    // Painter
    // ------------------------------------------------------------------

    class SwitchPainter : public CustomPainter
    {
    public:
        double t = 0.0; // 0 = off, 1 = on
        Color  active_track_color;
        Color  inactive_track_color;
        Color  active_thumb_color;
        Color  inactive_thumb_color;

        void paint(Canvas& canvas, Size size) override
        {
            const float w  = size.width;
            const float h  = size.height;
            const float ft = static_cast<float>(t);

            // Interpolated track color
            const Color track{
                inactive_track_color.r + (active_track_color.r - inactive_track_color.r) * ft,
                inactive_track_color.g + (active_track_color.g - inactive_track_color.g) * ft,
                inactive_track_color.b + (active_track_color.b - inactive_track_color.b) * ft,
                inactive_track_color.a + (active_track_color.a - inactive_track_color.a) * ft,
            };

            // Track (rounded rect)
            const float  track_r = h * 0.5f;
            const RRect  track_rr = RRect::fromRectAndRadius(
                Rect::fromLTWH(0.0f, 0.0f, w, h), track_r);
            canvas.drawRRect(track_rr, Paint::filled(track));

            // Thumb position
            const float thumb_r    = h * 0.5f - 2.0f;
            const float thumb_off  = h * 0.5f;
            const float thumb_x    = thumb_off + ft * (w - h);

            // Interpolated thumb color
            const Color thumb{
                inactive_thumb_color.r + (active_thumb_color.r - inactive_thumb_color.r) * ft,
                inactive_thumb_color.g + (active_thumb_color.g - inactive_thumb_color.g) * ft,
                inactive_thumb_color.b + (active_thumb_color.b - inactive_thumb_color.b) * ft,
                inactive_thumb_color.a + (active_thumb_color.a - inactive_thumb_color.a) * ft,
            };

            canvas.drawCircle({thumb_x, h * 0.5f}, thumb_r, Paint::filled(thumb));
        }

        bool shouldRepaint(const CustomPainter&) const override { return true; }
    };

    // ------------------------------------------------------------------
    // State
    // ------------------------------------------------------------------

    class SwitchState : public State<Switch>
    {
    public:
        void initState() override
        {
            ctrl_ = std::make_unique<AnimationController>(180.0);
            listener_id_ = ctrl_->addListener([this]() {
                setState([](){});
            });
            // Snap to initial position without animation
            if (widget().value)
                ctrl_->forward(0.0), ctrl_->forward(1.0);
        }

        void dispose() override
        {
            if (ctrl_ && listener_id_ != 0)
                ctrl_->removeListener(listener_id_);
        }

        void didUpdateWidget(const Widget& old_base) override
        {
            const auto& old_w = static_cast<const Switch&>(old_base);
            if (old_w.value != widget().value)
                widget().value ? ctrl_->forward(0.0) : ctrl_->reverse(0.0);
        }

        WidgetRef build(BuildContext&) override
        {
            const auto& w = widget();

            auto painter                    = std::make_shared<SwitchPainter>();
            painter->t                      = ctrl_ ? ctrl_->normalizedValue() : (w.value ? 1.0 : 0.0);
            painter->active_track_color     = w.active_track_color;
            painter->inactive_track_color   = w.inactive_track_color;
            painter->active_thumb_color     = w.active_thumb_color;
            painter->inactive_thumb_color   = w.inactive_thumb_color;

            auto box    = std::make_shared<SizedBox>();
            box->width  = w.width;
            box->height = w.height;
            box->child  = RawCustomPaint::create(painter);

            auto detector    = std::make_shared<GestureDetector>();
            const auto fn    = w.on_changed;
            const bool cur   = w.value;
            detector->on_tap = fn
                ? std::function<void()>([fn, cur]() { fn(!cur); })
                : std::function<void()>{};
            detector->child  = box;

            if (!fn)
            {
                auto faded     = std::make_shared<Opacity>();
                faded->opacity = 0.38f;
                faded->child   = detector;
                return faded;
            }
            return detector;
        }

    private:
        std::unique_ptr<AnimationController> ctrl_;
        uint64_t                             listener_id_ = 0;
    };

    // ------------------------------------------------------------------

    std::unique_ptr<StateBase> Switch::createState() const
    {
        return std::make_unique<SwitchState>();
    }

} // namespace systems::leal::campello_widgets
