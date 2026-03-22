#include <campello_widgets/widgets/image.hpp>
#include <campello_widgets/widgets/raw_image.hpp>

namespace systems::leal::campello_widgets
{

    WidgetRef Image::build(BuildContext&) const
    {
        return RawImage::create(texture, size, fit, alignment, opacity);
    }

} // namespace systems::leal::campello_widgets
