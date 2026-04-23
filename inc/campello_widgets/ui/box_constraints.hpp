#pragma once

#include <algorithm>
#include <limits>
#include <campello_widgets/ui/size.hpp>
#include <campello_widgets/ui/edge_insets.hpp>

namespace systems::leal::campello_widgets
{

    /**
     * @brief Immutable constraints for the box layout model.
     *
     * Encodes the minimum and maximum width/height that a RenderBox is allowed
     * to occupy. The layout protocol is constraints-down, sizes-up:
     *  1. A parent passes BoxConstraints down to each child.
     *  2. Each child picks a Size within those constraints.
     *  3. The parent uses the reported Size to position the child.
     */
    struct BoxConstraints
    {
        float min_width  = 0.0f;
        float max_width  = std::numeric_limits<float>::infinity();
        float min_height = 0.0f;
        float max_height = std::numeric_limits<float>::infinity();

        // ------------------------------------------------------------------
        // Named constructors
        // ------------------------------------------------------------------

        /**
         * @brief Forces the child to be exactly the given size.
         */
        static BoxConstraints tight(float width, float height) noexcept
        {
            return {width, width, height, height};
        }

        static BoxConstraints tight(Size size) noexcept
        {
            return tight(size.width, size.height);
        }

        /**
         * @brief Allows the child to be at most the given size, with no minimum.
         */
        static BoxConstraints loose(float width, float height) noexcept
        {
            return {0.0f, width, 0.0f, height};
        }

        static BoxConstraints loose(Size size) noexcept
        {
            return loose(size.width, size.height);
        }

        /**
         * @brief Forces the child to fill all available space.
         */
        static BoxConstraints expand(
            float width  = std::numeric_limits<float>::infinity(),
            float height = std::numeric_limits<float>::infinity()) noexcept
        {
            return {width, width, height, height};
        }

        /** @brief Unconstrained in both axes. */
        static BoxConstraints unconstrained() noexcept
        {
            const float inf = std::numeric_limits<float>::infinity();
            return {0.0f, inf, 0.0f, inf};
        }

        /**
         * @brief Creates constraints that tighten only the specified dimensions.
         *
         * If a dimension is specified, both min and max are set to that value.
         * If a dimension is null, it remains unconstrained (0 to infinity).
         */
        static BoxConstraints tightFor(std::optional<float> width,
                                       std::optional<float> height) noexcept
        {
            const float inf = std::numeric_limits<float>::infinity();
            return {
                width  ? *width  : 0.0f,
                width  ? *width  : inf,
                height ? *height : 0.0f,
                height ? *height : inf,
            };
        }

        // ------------------------------------------------------------------
        // Constraint operations
        // ------------------------------------------------------------------

        /**
         * @brief Returns constraints shrunk by the given insets.
         *
         * Useful for a parent that wants to reserve space for padding before
         * laying out its child.
         */
        BoxConstraints deflate(const EdgeInsets& insets) const noexcept
        {
            const float dw = insets.horizontal();
            const float dh = insets.vertical();
            return {
                std::max(0.0f, min_width  - dw),
                std::max(0.0f, max_width  - dw),
                std::max(0.0f, min_height - dh),
                std::max(0.0f, max_height - dh),
            };
        }

        /**
         * @brief Returns a copy with minimum constraints removed (min = 0).
         *
         * Useful for passing to children that should not be forced to fill space.
         */
        BoxConstraints loosen() const noexcept
        {
            return {0.0f, max_width, 0.0f, max_height};
        }

        /**
         * @brief Returns the intersection of these constraints with another.
         *
         * Each bound is clamped to the range of the corresponding bound in
         * `other`.  This is how SizedBox merges its own bounds with the
         * parent's constraints.
         */
        BoxConstraints enforce(const BoxConstraints& other) const noexcept
        {
            return {
                std::clamp(min_width,  other.min_width,  other.max_width),
                std::clamp(max_width,  other.min_width,  other.max_width),
                std::clamp(min_height, other.min_height, other.max_height),
                std::clamp(max_height, other.min_height, other.max_height),
            };
        }

        /**
         * @brief Clamps a Size to fit within these constraints.
         */
        Size constrain(Size size) const noexcept
        {
            return {
                std::clamp(size.width,  min_width,  max_width),
                std::clamp(size.height, min_height, max_height),
            };
        }

        // ------------------------------------------------------------------
        // Queries
        // ------------------------------------------------------------------

        /** @brief True when min == max on both axes. */
        bool isTight() const noexcept
        {
            return min_width == max_width && min_height == max_height;
        }

        /** @brief True when any maximum constraint is infinite. */
        bool isInfinite() const noexcept
        {
            const float inf = std::numeric_limits<float>::infinity();
            return max_width == inf || max_height == inf;
        }

        bool operator==(const BoxConstraints&) const noexcept = default;
    };

} // namespace systems::leal::campello_widgets
