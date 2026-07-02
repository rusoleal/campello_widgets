#pragma once

#include <campello_widgets/ui/rect.hpp>
#include <campello_widgets/ui/draw_command.hpp>
#include <vector_math/vector4.hpp>
#include <algorithm>
#include <vector>

namespace systems::leal::campello_widgets
{

    /**
     * @brief Projects `local_rect`'s four corners through `transform` and
     *        returns their axis-aligned bounding box.
     *
     * `offset`-based positioning (the `child_offset_`/`positionChild()`
     * mechanism most `RenderObject`s use) and `Canvas::translate()`/
     * `transform()` (used by e.g. `Transform` widgets and, critically,
     * `RenderSingleChildScrollView`/`RenderListView`/`RenderGridView`/
     * `RenderPageView`'s scroll offset — see their `performPaint()`) are
     * independent, additive mechanisms. A rect built purely from `offset`
     * and `size_` is only the *logical* position — it does not reflect
     * the true on-screen position once any ambient canvas transform is
     * active, since that transform is applied on top, separately, at
     * paint time. Every dirty-region report (`Renderer::noteDirtyRegion()`
     * callers) needs the *true* screen position to be comparable against
     * each other in one consistent coordinate space, so each call site
     * projects through `Canvas::currentTransform()` before reporting —
     * see `RenderObject::paint()`, `OffsetLayer::maybeReplay()`, and
     * `RenderBackdropFilter::performPaint()`.
     *
     * A conservative approximation for rotated/perspective content (the
     * bounding box of the projected corners, not the exact quad) — fine
     * for dirty-region purposes, which only need "did anything overlap,"
     * not pixel-exact coverage.
     */
    inline Rect projectedBounds(const Matrix4& transform, const Rect& local_rect) noexcept
    {
        auto project = [&](float x, float y) -> std::pair<float, float>
        {
            const auto v = transform * vector_math::Vector4<float>(x, y, 0.0f, 1.0f);
            const float w = v.w() != 0.0f ? v.w() : 1.0f;
            return {v.x() / w, v.y() / w};
        };
        const auto [x0, y0] = project(local_rect.left(),  local_rect.top());
        const auto [x1, y1] = project(local_rect.right(), local_rect.top());
        const auto [x2, y2] = project(local_rect.left(),  local_rect.bottom());
        const auto [x3, y3] = project(local_rect.right(), local_rect.bottom());

        return Rect::fromLTRB(
            std::min({x0, x1, x2, x3}), std::min({y0, y1, y2, y3}),
            std::max({x0, x1, x2, x3}), std::max({y0, y1, y2, y3}));
    }

    /**
     * @brief True if any `regions` rect (expanded by `margin_x`/`margin_y`)
     *        overlaps any rect in `dirty_rects` — or unconditionally true
     *        if `dirty_overflowed` (the conservative fallback once the
     *        per-frame dirty-rect budget is exceeded).
     *
     * Used by `Renderer::buildFrame()` to decide whether the BackdropFilter
     * capture pass can be skipped this frame: `regions` are the bounds of
     * every `BackdropFilter` painted this frame, `margin_x`/`margin_y`
     * account for blur spread beyond each filter's exact footprint, and
     * `dirty_rects`/`dirty_overflowed` are the frame's accumulated
     * dirty-region state (see `Renderer::noteDirtyRegion()`). Kept as a
     * free function, independent of `Renderer`, so it's unit-testable
     * without a GPU device.
     */
    inline bool anyRegionDirty(
        const std::vector<Rect>& regions,
        float                     margin_x,
        float                     margin_y,
        const std::vector<Rect>& dirty_rects,
        bool                      dirty_overflowed) noexcept
    {
        if (dirty_overflowed)
            return !regions.empty();

        for (const Rect& region : regions)
        {
            const Rect margin = region.inflate(margin_x, margin_y);
            for (const Rect& d : dirty_rects)
                if (margin.intersects(d))
                    return true;
        }
        return false;
    }

} // namespace systems::leal::campello_widgets
