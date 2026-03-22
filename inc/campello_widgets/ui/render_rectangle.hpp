#pragma once

#include <campello_widgets/ui/render_box.hpp>
#include <campello_widgets/ui/color.hpp>

namespace systems::leal::campello_widgets
{

    /**
     * @brief RenderBox that paints a solid or stroked rectangle.
     *
     * By default expands to fill the tightest constraint available.
     * Set `fill = false` to collapse to zero size when constraints are loose.
     */
    class RenderRectangle : public RenderBox
    {
    public:
        void setColor(Color color) noexcept;
        void setFill(bool fill) noexcept;
        void setCornerRadius(float radius) noexcept;

        Color color()        const noexcept { return color_;         }
        bool  fill()         const noexcept { return fill_;          }
        float cornerRadius() const noexcept { return corner_radius_; }

        void performLayout() override;
        void performPaint(PaintContext& context, const Offset& offset) override;

    private:
        Color color_         = Color::black();
        bool  fill_          = true;
        float corner_radius_ = 0.0f;
    };

} // namespace systems::leal::campello_widgets
