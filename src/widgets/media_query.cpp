#include <campello_widgets/widgets/media_query.hpp>
#include <campello_widgets/widgets/build_context.hpp>

namespace systems::leal::campello_widgets
{

    const MediaQueryData* MediaQuery::of(BuildContext& context)
    {
        const auto* widget = context.dependOnInheritedWidgetOfExactType<MediaQuery>();
        if (!widget) return nullptr;
        return &widget->data;
    }

    bool MediaQuery::updateShouldNotify(const InheritedWidget& old_widget) const
    {
        const auto& old_media = static_cast<const MediaQuery&>(old_widget);
        return data != old_media.data;
    }

} // namespace systems::leal::campello_widgets
