#include <campello_widgets/ui/curved_animation.hpp>

namespace systems::leal::campello_widgets
{

    CurvedAnimation::CurvedAnimation(AnimationController& parent, CurveFn curve)
        : parent_(parent)
        , curve_(std::move(curve))
    {
    }

    CurvedAnimation::~CurvedAnimation() = default;

    double CurvedAnimation::value() const
    {
        return curve_(parent_.normalizedValue());
    }

    AnimationStatus CurvedAnimation::status() const
    {
        return parent_.status();
    }

    uint64_t CurvedAnimation::addListener(std::function<void()> fn)
    {
        return parent_.addListener(std::move(fn));
    }

    void CurvedAnimation::removeListener(uint64_t id)
    {
        parent_.removeListener(id);
    }

} // namespace systems::leal::campello_widgets
