#pragma once

#include <vector>
#include <campello_widgets/ui/rect.hpp>

namespace systems::leal::campello_widgets
{
    /**
     * @brief One of the 9 (src, dst) rect pairs a nine-patch splits into.
     *
     * `src` is normalised to [0,1]x[0,1] within the source texture, matching
     * `Canvas::drawImage()`'s own `src_rect` convention. `dst` is in the same
     * local coordinate space as the `dst_rect` passed to `drawImageNine()`.
     */
    struct NinePatchPatch
    {
        Rect src;
        Rect dst;
    };

    /**
     * @brief Pure CPU geometry for `Canvas::drawImageNine()` -- split into its
     * own header so it's unit-testable without a GPU-backed `Texture`
     * (mirrors `gpu/stroke_geometry.hpp` / `gpu/path_fill_aa.hpp`'s split).
     *
     * `center` is in *source pixel* coordinates, clamped to the texture's own
     * `[0,img_w]x[0,img_h]` bounds. Corner patches keep their unscaled source
     * pixel size, independently clamped to half of `dst_rect`'s size on that
     * axis so two opposing corners never overlap -- a simple independent
     * clamp rather than a full proportional shrink; see `drawImageNine()`'s
     * doc comment for the limitation this has under extreme squeezing.
     *
     * Degenerate patches (zero-width/height, e.g. `center` touching an edge)
     * are omitted from the result rather than returned as empty rects.
     */
    std::vector<NinePatchPatch> computeNinePatchGeometry(
        float img_w, float img_h,
        const Rect& center,
        const Rect& dst_rect);
}
