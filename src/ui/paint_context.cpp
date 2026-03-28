#include <campello_widgets/ui/paint_context.hpp>

namespace systems::leal::campello_widgets
{

    PaintContext::PaintContext(
        campello_gpu::RenderPassEncoder& encoder,
        float viewport_width,
        float viewport_height)
        : encoder_(&encoder)
        , viewport_width_(viewport_width)
        , viewport_height_(viewport_height)
        , canvas_(viewport_width, viewport_height)
    {
    }

    PaintContext::PaintContext(
        float viewport_width,
        float viewport_height)
        : encoder_(nullptr)
        , viewport_width_(viewport_width)
        , viewport_height_(viewport_height)
        , canvas_(viewport_width, viewport_height)
    {
    }

} // namespace systems::leal::campello_widgets
