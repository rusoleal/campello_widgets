#include <campello_widgets/widgets/draw_surface.hpp>
#include <campello_widgets/ui/render_draw_surface.hpp>

namespace systems::leal::campello_widgets
{

    std::shared_ptr<RenderObject> DrawSurface::createRenderObject() const
    {
        auto ro = std::make_shared<RenderDrawSurface>();
        ro->stroke_color     = stroke_color;
        ro->background_color = background_color;
        ro->stroke_width     = stroke_width;
        return ro;
    }

    void DrawSurface::updateRenderObject(RenderObject& render_object) const
    {
        auto& ro = static_cast<RenderDrawSurface&>(render_object);
        ro.stroke_color     = stroke_color;
        ro.background_color = background_color;
        ro.stroke_width     = stroke_width;
    }

} // namespace systems::leal::campello_widgets
