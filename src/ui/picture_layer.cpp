#include <campello_widgets/ui/picture_layer.hpp>

namespace systems::leal::campello_widgets
{

    void PictureLayer::record(PaintContext& context, const std::function<void()>& paintContent)
    {
        const size_t start = context.canvas().commands().size();
        paintContent();
        const DrawList& all = context.canvas().commands();
        commands_.assign(all.begin() + static_cast<std::ptrdiff_t>(start), all.end());
        has_content_ = true;

        has_unsafe_geometry_ = false;
        has_backdrop_filter_ = false;
        for (const auto& c : commands_)
        {
            std::visit([this](auto&& cmd) {
                using T = std::decay_t<decltype(cmd)>;
                if constexpr (std::is_same_v<T, DrawBackdropFilterBeginCmd>)
                    has_backdrop_filter_ = true;
                if constexpr (std::is_same_v<T, PushClipRectCmd>            ||
                              std::is_same_v<T, PushClipRRectCmd>           ||
                              std::is_same_v<T, PushClipOvalCmd>            ||
                              std::is_same_v<T, PushClipPathCmd>            ||
                              std::is_same_v<T, DrawBackdropFilterBeginCmd> ||
                              std::is_same_v<T, DrawShaderMaskBeginCmd>     ||
                              std::is_same_v<T, SaveLayerCmd>)
                    has_unsafe_geometry_ = true;
            }, c);
        }
    }

} // namespace systems::leal::campello_widgets
