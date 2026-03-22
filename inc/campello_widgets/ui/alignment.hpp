#pragma once

#include <campello_widgets/ui/size.hpp>
#include <campello_widgets/ui/offset.hpp>

namespace systems::leal::campello_widgets
{

    /**
     * @brief A point within a rectangle, expressed as fractions of the
     * rectangle's width and height.
     *
     * x and y are in the range [-1, 1]:
     *   -1 = left / top edge
     *    0 = center
     *   +1 = right / bottom edge
     */
    struct Alignment
    {
        float x = 0.0f;
        float y = 0.0f;

        /**
         * @brief Computes the top-left offset to place a child of `child_size`
         * inside a parent of `parent_size` at this alignment.
         */
        Offset inscribe(Size child_size, Size parent_size) const noexcept
        {
            return {
                (parent_size.width  - child_size.width)  * (1.0f + x) * 0.5f,
                (parent_size.height - child_size.height) * (1.0f + y) * 0.5f,
            };
        }

        bool operator==(const Alignment&) const noexcept = default;

        // ------------------------------------------------------------------
        // Named constants
        // ------------------------------------------------------------------

        static constexpr Alignment topLeft()      { return {-1.0f, -1.0f}; }
        static constexpr Alignment topCenter()    { return { 0.0f, -1.0f}; }
        static constexpr Alignment topRight()     { return { 1.0f, -1.0f}; }
        static constexpr Alignment centerLeft()   { return {-1.0f,  0.0f}; }
        static constexpr Alignment center()       { return { 0.0f,  0.0f}; }
        static constexpr Alignment centerRight()  { return { 1.0f,  0.0f}; }
        static constexpr Alignment bottomLeft()   { return {-1.0f,  1.0f}; }
        static constexpr Alignment bottomCenter() { return { 0.0f,  1.0f}; }
        static constexpr Alignment bottomRight()  { return { 1.0f,  1.0f}; }
    };

} // namespace systems::leal::campello_widgets
