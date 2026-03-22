#include <campello_widgets/widgets/container.hpp>
#include <campello_widgets/widgets/sized_box.hpp>
#include <campello_widgets/widgets/padding.hpp>
#include <campello_widgets/widgets/colored_box.hpp>
#include <campello_widgets/widgets/align.hpp>

namespace systems::leal::campello_widgets
{

    WidgetRef Container::build(BuildContext&) const
    {
        WidgetRef result = child;

        if (padding)
        {
            auto w    = std::make_shared<Padding>();
            w->padding = *padding;
            w->child  = std::move(result);
            result    = std::move(w);
        }

        if (color)
        {
            auto w   = std::make_shared<ColoredBox>();
            w->color = *color;
            w->child = std::move(result);
            result   = std::move(w);
        }

        if (alignment)
        {
            auto w         = std::make_shared<Align>();
            w->alignment   = *alignment;
            w->child       = std::move(result);
            result         = std::move(w);
        }

        if (width || height)
        {
            auto w    = std::make_shared<SizedBox>();
            w->width  = width;
            w->height = height;
            w->child  = std::move(result);
            result    = std::move(w);
        }

        if (!result)
        {
            auto w = std::make_shared<SizedBox>();
            result = std::move(w);
        }

        return result;
    }

} // namespace systems::leal::campello_widgets
