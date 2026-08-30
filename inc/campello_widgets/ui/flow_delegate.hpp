#pragma once

#include <campello_widgets/ui/size.hpp>
#include <campello_widgets/ui/box_constraints.hpp>
#include <vector_math/matrix4.hpp>
#include <cstddef>

namespace systems::leal::campello_widgets
{

    /**
     * @brief Read/paint access to a Flow's children, passed to
     * `FlowDelegate::paintChildren()`.
     *
     * Matches Flutter's `FlowPaintingContext`.
     */
    class FlowPaintingContext
    {
    public:
        virtual ~FlowPaintingContext() = default;

        /** @brief This Flow's own size, as determined by `FlowDelegate::getSize()`. */
        virtual Size size() const = 0;

        virtual size_t childCount() const = 0;

        /** @brief The size child `index` was laid out at. */
        virtual Size childSize(size_t index) const = 0;

        /**
         * @brief Paints child `index` through `transform` (applied around
         * the Flow's own top-left origin), at `opacity`.
         */
        virtual void paintChild(
            size_t index,
            const systems::leal::vector_math::Matrix4<float>& transform,
            float opacity = 1.0f) = 0;
    };

    /**
     * @brief Delegate that positions/paints a Flow's children via arbitrary
     * per-child transforms, computed at paint time rather than layout time.
     *
     * Matches Flutter's `FlowDelegate`.
     */
    class FlowDelegate
    {
    public:
        virtual ~FlowDelegate() = default;

        /** @brief This Flow's own size. Default: fills the incoming constraints. */
        virtual Size getSize(const BoxConstraints& constraints) const
        {
            return {constraints.max_width, constraints.max_height};
        }

        /** @brief Constraints for child `index`. Default: unchanged. */
        virtual BoxConstraints getConstraintsForChild(size_t index, const BoxConstraints& constraints) const
        {
            (void)index;
            return constraints;
        }

        /** @brief Positions/paints every child via `context.paintChild()`. */
        virtual void paintChildren(FlowPaintingContext& context) = 0;

        /** @brief Whether a change from `old_delegate` requires repainting. */
        virtual bool shouldRepaint(const FlowDelegate& old_delegate) const = 0;
    };

} // namespace systems::leal::campello_widgets
