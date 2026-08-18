#include <campello_widgets/widgets/icon.hpp>
#include <campello_widgets/widgets/raw_image.hpp>
#include <campello_widgets/ui/size.hpp>
#include <campello_widgets/ui/box_fit.hpp>
#include <campello_widgets/ui/alignment.hpp>

namespace systems::leal::campello_widgets
{

    WidgetRef Icon::build(BuildContext&) const
    {
        return RawImage::create(
            texture,
            Size{size, size},
            BoxFit::contain,
            Alignment::center(),
            1.0f,
            color);
    }

} // namespace systems::leal::campello_widgets
