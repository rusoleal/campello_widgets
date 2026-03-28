#pragma once

#include <campello_widgets/ui/rect.hpp>
#include <campello_widgets/ui/offset.hpp>
#include <algorithm>

namespace systems::leal::campello_widgets
{

    /**
     * @brief A rounded rectangle with the same radius on all corners.
     *
     * RRect is used for drawing and clipping rounded rectangles.
     * For per-corner radii, use RRect with individual corner radii.
     */
    struct RRect
    {
        Rect rect;
        float radius_x = 0.0f;  // Horizontal corner radius
        float radius_y = 0.0f;  // Vertical corner radius

        RRect() = default;
        RRect(const Rect& r, float radius)
            : rect(r)
            , radius_x(radius)
            , radius_y(radius)
        {}
        RRect(const Rect& r, float rx, float ry)
            : rect(r)
            , radius_x(rx)
            , radius_y(ry)
        {}

        /** @brief Creates a rounded rect from a rect and single radius. */
        static RRect fromRectAndRadius(const Rect& rect, float radius)
        {
            return RRect(rect, radius);
        }

        /** @brief Creates a rounded rect from a rect and radii. */
        static RRect fromRectAndRadii(const Rect& rect, float radius_x, float radius_y)
        {
            return RRect(rect, radius_x, radius_y);
        }

        /** @brief Returns true if the rounded rect is empty. */
        bool isEmpty() const
        {
            return rect.isEmpty() || (radius_x <= 0.0f && radius_y <= 0.0f);
        }

        /** @brief Returns the safe radii (clamped to half width/height). */
        void getSafeRadii(float& out_rx, float& out_ry) const
        {
            out_rx = std::min(radius_x, rect.width * 0.5f);
            out_ry = std::min(radius_y, rect.height * 0.5f);
        }

        bool operator==(const RRect& other) const noexcept
        {
            return rect == other.rect &&
                   radius_x == other.radius_x &&
                   radius_y == other.radius_y;
        }

        bool operator!=(const RRect& other) const noexcept
        {
            return !(*this == other);
        }
    };

    /**
     * @brief A rounded rectangle with potentially different radii for each corner.
     *
     * This allows for more complex shapes like speech bubbles or asymmetric rounding.
     */
    struct RRectComplex
    {
        Rect rect;
        float top_left_radius = 0.0f;
        float top_right_radius = 0.0f;
        float bottom_left_radius = 0.0f;
        float bottom_right_radius = 0.0f;

        RRectComplex() = default;
        RRectComplex(const Rect& r, float radius)
            : rect(r)
            , top_left_radius(radius)
            , top_right_radius(radius)
            , bottom_left_radius(radius)
            , bottom_right_radius(radius)
        {}

        /** @brief Creates with different radii for each corner. */
        static RRectComplex fromRectAndCorners(
            const Rect& rect,
            float top_left = 0.0f,
            float top_right = 0.0f,
            float bottom_left = 0.0f,
            float bottom_right = 0.0f)
        {
            RRectComplex r;
            r.rect = rect;
            r.top_left_radius = top_left;
            r.top_right_radius = top_right;
            r.bottom_left_radius = bottom_left;
            r.bottom_right_radius = bottom_right;
            return r;
        }

        bool operator==(const RRectComplex& other) const noexcept = default;
    };

} // namespace systems::leal::campello_widgets
