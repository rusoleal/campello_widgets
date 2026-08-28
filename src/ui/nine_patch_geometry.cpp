#include "ui/nine_patch_geometry.hpp"
#include <algorithm>

namespace systems::leal::campello_widgets
{
    std::vector<NinePatchPatch> computeNinePatchGeometry(
        float img_w, float img_h,
        const Rect& center,
        const Rect& dst_rect)
    {
        std::vector<NinePatchPatch> patches;
        if (img_w <= 0.0f || img_h <= 0.0f) return patches;

        // Clamp `center` to the texture's own bounds -- a caller-supplied
        // rect outside [0,img_w]x[0,img_h] would otherwise produce inverted
        // or nonsensical patches below.
        const float cx0 = std::clamp(center.left(),  0.0f, img_w);
        const float cx1 = std::clamp(center.right(), cx0,  img_w);
        const float cy0 = std::clamp(center.top(),    0.0f, img_h);
        const float cy1 = std::clamp(center.bottom(), cy0,  img_h);

        // Source-space column/row splits, normalised to [0,1] --
        // drawImage()'s own src_rect convention.
        const float sx[4] = {0.0f, cx0 / img_w, cx1 / img_w, 1.0f};
        const float sy[4] = {0.0f, cy0 / img_h, cy1 / img_h, 1.0f};

        // Destination-space column/row splits: the 4 corners keep their
        // *unscaled* source pixel size, clamped to half of dst_rect's size
        // on that axis so two opposing corners never overlap when dst is
        // smaller than the source's unstretched regions. The middle
        // column/row absorbs whatever space is left.
        const float left_w   = std::min(cx0,           dst_rect.width  * 0.5f);
        const float right_w  = std::min(img_w - cx1,   dst_rect.width  * 0.5f);
        const float top_h    = std::min(cy0,           dst_rect.height * 0.5f);
        const float bottom_h = std::min(img_h - cy1,   dst_rect.height * 0.5f);

        const float dx[4] = {
            dst_rect.x, dst_rect.x + left_w,
            dst_rect.x + dst_rect.width - right_w, dst_rect.x + dst_rect.width };
        const float dy[4] = {
            dst_rect.y, dst_rect.y + top_h,
            dst_rect.y + dst_rect.height - bottom_h, dst_rect.y + dst_rect.height };

        patches.reserve(9);
        for (int row = 0; row < 3; ++row)
        {
            for (int col = 0; col < 3; ++col)
            {
                const Rect src = Rect::fromLTRB(sx[col], sy[row], sx[col + 1], sy[row + 1]);
                const Rect dst = Rect::fromLTRB(dx[col], dy[row], dx[col + 1], dy[row + 1]);
                if (src.isEmpty() || dst.isEmpty())
                    continue; // degenerate patch (e.g. center touches an edge) -- skip cleanly
                patches.push_back({src, dst});
            }
        }
        return patches;
    }
}
