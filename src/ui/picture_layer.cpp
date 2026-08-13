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
                // PushClipRRectCmd/PushClipOvalCmd/DrawShaderMaskBeginCmd/
                // SaveLayerCmd all store an absolute bounds rect too, but
                // OffsetLayer::maybeReplay()'s cheap-reposition path doesn't
                // need to shift it: their *only* consumer is an offscreen
                // composite draw (drawClipShapeComposite/
                // drawShaderMaskComposite/saveLayerComposite) that already
                // multiplies that stored rect by the ambient transform —
                // the same wrapping canvas.translate() that repositions
                // ordinary draw geometry, so it's already correct with no
                // special handling. PushClipRectCmd is the odd one out: its
                // only consumer is `current_clip = c.rect` — a direct
                // reassignment, never multiplied by the transform stack
                // (see this class's doc comment for why that's correct for
                // a *fresh* recording) — so a replayed copy needs its own
                // rect shifted by hand, which maybeReplay() does. Only
                // PushClipPathCmd remains genuinely unsafe: it's also a
                // direct-assignment consumer, but Path has no translate()
                // yet to shift it with.
                if constexpr (std::is_same_v<T, PushClipPathCmd> ||
                              std::is_same_v<T, DrawBackdropFilterBeginCmd>)
                    has_unsafe_geometry_ = true;
            }, c);
        }
    }

} // namespace systems::leal::campello_widgets
