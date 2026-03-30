#pragma once

#include <campello_widgets/widgets/single_child_render_object_widget.hpp>
#include <campello_widgets/ui/image_filter.hpp>
#include <campello_widgets/ui/render_backdrop_filter.hpp>

namespace systems::leal::campello_widgets
{

    /**
     * @brief Applies a blur (or other image filter) to the scene behind it.
     *
     * Place a BackdropFilter wherever you want a "frosted glass" effect.
     * The child widget renders on top of the blurred backdrop.
     *
     * @code
     * BackdropFilter{
     *     .filter = ImageFilter::blur(10.0f),
     *     .child  = SomeChildWidget{},
     * }
     * @endcode
     */
    struct BackdropFilter : SingleChildRenderObjectWidget
    {
        ImageFilter filter = ImageFilter::blur(8.0f);


        std::shared_ptr<RenderObject> createRenderObject() const override
        {
            return std::make_shared<RenderBackdropFilter>(filter);
        }

        void updateRenderObject(RenderObject& ro) const override
        {
            static_cast<RenderBackdropFilter&>(ro).setFilter(filter);
        }
    };

} // namespace systems::leal::campello_widgets
