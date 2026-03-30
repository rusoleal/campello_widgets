#include <campello_widgets/widgets/animated_align.hpp>
#include <campello_widgets/ui/animation_controller.hpp>
#include <campello_widgets/ui/tween.hpp>
#include <campello_widgets/widgets/align.hpp>

namespace systems::leal::campello_widgets
{

    class AnimatedAlignState : public State<AnimatedAlign>
    {
    public:
        void initState() override
        {
            const auto& w = widget();
            current_ = w.alignment;

            ctrl_ = std::make_unique<AnimationController>(w.duration_ms);
            listener_id_ = ctrl_->addListener([this]() {
                setState([this]() {
                    const double t = curve_(ctrl_->normalizedValue());
                    current_ = lerp<Alignment>(from_, to_, t);
                });
            });
        }

        void dispose() override
        {
            if (ctrl_ && listener_id_ != 0)
                ctrl_->removeListener(listener_id_);
        }

        void didUpdateWidget(const Widget& old_base) override
        {
            const auto& old_w = static_cast<const AnimatedAlign&>(old_base);
            const auto& new_w = widget();

            if (old_w.alignment == new_w.alignment) return;

            from_ = current_;
            to_   = new_w.alignment;

            if (old_w.duration_ms != new_w.duration_ms) {
                if (ctrl_ && listener_id_ != 0)
                    ctrl_->removeListener(listener_id_);
                ctrl_ = std::make_unique<AnimationController>(new_w.duration_ms);
                listener_id_ = ctrl_->addListener([this]() {
                    setState([this]() {
                        const double t = curve_(ctrl_->normalizedValue());
                        current_ = lerp<Alignment>(from_, to_, t);
                    });
                });
            }

            curve_ = new_w.curve ? new_w.curve : Curves::easeInOut;
            ctrl_->forward(0.0);
        }

        WidgetRef build(BuildContext&) override
        {
            const auto& w = widget();
            auto a = std::make_shared<Align>();
            a->alignment    = current_;
            a->width_factor  = w.width_factor;
            a->height_factor = w.height_factor;
            a->child        = w.child;
            return a;
        }

    private:
        Alignment current_{};
        Alignment from_{};
        Alignment to_{};

        std::unique_ptr<AnimationController> ctrl_;
        uint64_t                             listener_id_ = 0;
        std::function<double(double)>        curve_       = Curves::easeInOut;
    };

    std::unique_ptr<StateBase> AnimatedAlign::createState() const
    {
        return std::make_unique<AnimatedAlignState>();
    }

} // namespace systems::leal::campello_widgets
