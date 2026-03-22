#pragma once

#include <campello_widgets/ui/animation_controller.hpp>
#include <campello_widgets/ui/color.hpp>
#include <campello_widgets/ui/offset.hpp>
#include <campello_widgets/ui/size.hpp>

namespace systems::leal::campello_widgets
{

    // ------------------------------------------------------------------
    // lerp free functions — specialize for each interpolatable type
    // ------------------------------------------------------------------

    template<typename T>
    T lerp(const T& a, const T& b, double t);

    template<>
    inline float lerp(const float& a, const float& b, double t)
    {
        return a + static_cast<float>((b - a) * t);
    }

    template<>
    inline double lerp(const double& a, const double& b, double t)
    {
        return a + (b - a) * t;
    }

    template<>
    inline Color lerp(const Color& a, const Color& b, double t)
    {
        return Color::fromRGBA(
            a.r + static_cast<float>((b.r - a.r) * t),
            a.g + static_cast<float>((b.g - a.g) * t),
            a.b + static_cast<float>((b.b - a.b) * t),
            a.a + static_cast<float>((b.a - a.a) * t));
    }

    template<>
    inline Offset lerp(const Offset& a, const Offset& b, double t)
    {
        return {a.x + static_cast<float>((b.x - a.x) * t),
                a.y + static_cast<float>((b.y - a.y) * t)};
    }

    template<>
    inline Size lerp(const Size& a, const Size& b, double t)
    {
        return {a.width  + static_cast<float>((b.width  - a.width)  * t),
                a.height + static_cast<float>((b.height - a.height) * t)};
    }

    // ------------------------------------------------------------------
    // Tween<T>
    // ------------------------------------------------------------------

    /**
     * @brief Interpolates between two values of type T.
     *
     * Uses `lerp<T>` under the hood. Specializations exist for float, double,
     * Color, Offset, and Size.
     *
     * @code
     * Tween<Color> t{Color::blue(), Color::red()};
     * Color c = t.evaluate(controller.normalizedValue()); // or t.evaluate(controller)
     * @endcode
     */
    template<typename T>
    struct Tween
    {
        T begin;
        T end;

        /** @brief Evaluate the tween at normalised position `t` in [0, 1]. */
        T evaluate(double t) const
        {
            return lerp<T>(begin, end, t);
        }

        /** @brief Convenience overload — evaluates at the controller's normalised value. */
        T evaluate(const AnimationController& ctrl) const
        {
            return evaluate(ctrl.normalizedValue());
        }
    };

} // namespace systems::leal::campello_widgets
