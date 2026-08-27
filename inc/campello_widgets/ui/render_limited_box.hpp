#pragma once

#include <limits>
#include <campello_widgets/ui/render_box.hpp>

namespace systems::leal::campello_widgets
{

    /**
     * @brief Limits an otherwise-unbounded incoming constraint axis to
     * `max_width`/`max_height`; passes an already-bounded axis through
     * unchanged.
     *
     * Mirrors Flutter's `RenderLimitedBox`. On a *bounded* axis, the
     * incoming max is used as-is, so a child laid out through this box can
     * still fill it. On an *unbounded* axis (e.g. inside a Row/Column with
     * no Expanded), the effective max becomes `max_width`/`max_height`
     * (default: infinity, i.e. a no-op) instead of infinity — this is what
     * lets a childless box request "as large as possible" without ever
     * reporting an infinite size. See `Container`'s empty-box fallback for
     * the intended use (`LimitedBox{0, 0, ConstrainedBox{expand()}}`,
     * matching Flutter's identical `Container.build()` trick).
     */
    class RenderLimitedBox : public RenderBox
    {
    public:
        float max_width  = std::numeric_limits<float>::infinity();
        float max_height = std::numeric_limits<float>::infinity();

        void performLayout() override;
        void performPaint(PaintContext& context, const Offset& offset) override;

    private:
        BoxConstraints limitConstraints(const BoxConstraints& constraints) const noexcept;
    };

} // namespace systems::leal::campello_widgets
