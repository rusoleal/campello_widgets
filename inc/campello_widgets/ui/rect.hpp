#pragma once

#include <campello_widgets/ui/size.hpp>
#include <campello_widgets/ui/offset.hpp>

namespace systems::leal::campello_widgets
{

    /**
     * @brief An axis-aligned rectangle defined by an origin and a size.
     *
     * All values are in logical pixels. The origin is the top-left corner.
     */
    struct Rect
    {
        float x      = 0.0f;
        float y      = 0.0f;
        float width  = 0.0f;
        float height = 0.0f;

        // ------------------------------------------------------------------
        // Named constructors
        // ------------------------------------------------------------------

        static constexpr Rect zero() noexcept { return {}; }

        static constexpr Rect fromLTWH(float left, float top,
                                       float width, float height) noexcept
        {
            return {left, top, width, height};
        }

        static constexpr Rect fromLTRB(float left, float top,
                                       float right, float bottom) noexcept
        {
            return {left, top, right - left, bottom - top};
        }

        static Rect fromOffsetAndSize(const Offset& origin, const Size& size) noexcept
        {
            return {origin.x, origin.y, size.width, size.height};
        }

        // ------------------------------------------------------------------
        // Accessors
        // ------------------------------------------------------------------

        float left()   const noexcept { return x; }
        float top()    const noexcept { return y; }
        float right()  const noexcept { return x + width; }
        float bottom() const noexcept { return y + height; }

        Offset origin() const noexcept { return {x, y}; }
        Size   size()   const noexcept { return {width, height}; }

        // ------------------------------------------------------------------
        // Queries
        // ------------------------------------------------------------------

        bool isEmpty() const noexcept { return width <= 0.0f || height <= 0.0f; }

        bool contains(float px, float py) const noexcept
        {
            return px >= x && px < right() && py >= y && py < bottom();
        }

        bool intersects(const Rect& other) const noexcept
        {
            return right() > other.x && x < other.right() &&
                   bottom() > other.y && y < other.bottom();
        }

        Rect intersection(const Rect& other) const noexcept
        {
            const float l = x      > other.x      ? x      : other.x;
            const float t = y      > other.y      ? y      : other.y;
            const float r = right()  < other.right()  ? right()  : other.right();
            const float b = bottom() < other.bottom() ? bottom() : other.bottom();
            if (r <= l || b <= t) return Rect::zero();
            return Rect::fromLTRB(l, t, r, b);
        }

        bool operator==(const Rect&) const noexcept = default;
    };

} // namespace systems::leal::campello_widgets
