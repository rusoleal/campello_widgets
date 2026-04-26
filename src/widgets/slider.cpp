#include <campello_widgets/widgets/slider.hpp>
#include <campello_widgets/widgets/theme.hpp>
#include <campello_widgets/ui/render_slider.hpp>
#include <campello_widgets/widgets/single_child_render_object_widget.hpp>
#include <campello_widgets/widgets/sized_box.hpp>

#include <algorithm>

namespace systems::leal::campello_widgets
{

    // ------------------------------------------------------------------
    // Internal proxy widget → RenderSlider
    // ------------------------------------------------------------------

    class SliderProxy : public SingleChildRenderObjectWidget
    {
    public:
        float  normalized_value = 0.0f; // 0..1
        Color  active_color;
        Color  inactive_color;
        float  track_height  = 4.0f;
        float  thumb_radius  = 10.0f;
        std::function<void(float)> on_normalized_changed; // 0..1

        std::shared_ptr<RenderObject> createRenderObject() const override
        {
            auto r = std::make_shared<RenderSlider>();
            applyTo(*r);
            return r;
        }

        void updateRenderObject(RenderObject& ro) const override
        {
            applyTo(static_cast<RenderSlider&>(ro));
            static_cast<RenderSlider&>(ro).markNeedsPaint();
        }

    private:
        void applyTo(RenderSlider& r) const
        {
            r.value              = normalized_value;
            r.active_color       = active_color;
            r.inactive_color     = inactive_color;
            r.track_height       = track_height;
            r.thumb_radius       = thumb_radius;
            r.on_value_changed   = on_normalized_changed;
        }
    };

    // ------------------------------------------------------------------
    // State
    // ------------------------------------------------------------------

    class SliderState : public State<Slider>
    {
    public:
        void initState() override
        {
            const auto& w = widget();
            current_      = normalize(w.value);
        }

        void didUpdateWidget(const Widget& old_base) override
        {
            const auto& old_w = static_cast<const Slider&>(old_base);
            if (old_w.value != widget().value)
                current_ = normalize(widget().value);
        }

        WidgetRef build(BuildContext& ctx) override
        {
            const auto& w = widget();
            const auto* tokens = Theme::tokensOf(ctx);

            auto proxy                    = std::make_shared<SliderProxy>();
            proxy->normalized_value       = current_;
            proxy->active_color           = w.active_color.value_or(tokens->colors.primary);
            proxy->inactive_color         = w.inactive_color.value_or(tokens->colors.outline);
            proxy->track_height           = w.track_height;
            proxy->thumb_radius           = w.thumb_radius;
            proxy->on_normalized_changed  = [this](float norm) {
                setState([this, norm]() {
                    current_ = norm;
                    const auto& ww = widget();
                    if (ww.on_changed)
                        ww.on_changed(ww.min + norm * (ww.max - ww.min));
                });
            };

            auto box    = std::make_shared<SizedBox>();
            box->height = w.height;
            box->child  = proxy;
            return box;
        }

    private:
        float current_ = 0.0f; // normalised [0, 1]

        float normalize(float v) const
        {
            const auto& w = widget();
            if (w.max <= w.min) return 0.0f;
            return std::clamp((v - w.min) / (w.max - w.min), 0.0f, 1.0f);
        }
    };

    // ------------------------------------------------------------------

    std::unique_ptr<StateBase> Slider::createState() const
    {
        return std::make_unique<SliderState>();
    }

} // namespace systems::leal::campello_widgets
