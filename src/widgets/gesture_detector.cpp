#include <campello_widgets/diagnostics/diagnostic_property.hpp>
#include <campello_widgets/widgets/gesture_detector.hpp>
#include <campello_widgets/ui/render_gesture_detector.hpp>

namespace systems::leal::campello_widgets
{

    std::shared_ptr<RenderObject> GestureDetector::createRenderObject() const
    {
        auto ro           = std::make_shared<RenderGestureDetector>();
        ro->on_tap         = on_tap;
        ro->on_double_tap  = on_double_tap;
        ro->on_long_press  = on_long_press;
        ro->on_pan_update  = on_pan_update;
        ro->on_pan_end     = on_pan_end;
        ro->on_scroll      = on_scroll;
        return ro;
    }

    void GestureDetector::updateRenderObject(RenderObject& render_object) const
    {
        auto& ro           = static_cast<RenderGestureDetector&>(render_object);
        ro.on_tap          = on_tap;
        ro.on_double_tap   = on_double_tap;
        ro.on_long_press   = on_long_press;
        ro.on_pan_update   = on_pan_update;
        ro.on_pan_end      = on_pan_end;
        ro.on_scroll       = on_scroll;
    }


    void GestureDetector::debugFillProperties(DiagnosticsPropertyBuilder& properties) const
    {
        properties.add(std::make_unique<FlagProperty>("onTap", on_tap != nullptr, "tap enabled", "tap disabled"));
    }
} // namespace systems::leal::campello_widgets
