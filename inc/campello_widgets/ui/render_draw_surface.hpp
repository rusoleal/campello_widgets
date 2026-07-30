#pragma once

#include <optional>
#include <campello_widgets/ui/render_image.hpp>
#include <campello_widgets/ui/color.hpp>
#include <campello_widgets/ui/offset.hpp>
#include <campello_widgets/ui/pointer_event.hpp>
#include <campello_widgets/ui/gesture_arena_manager.hpp>
#include <campello_widgets/ui/draw_command.hpp>

namespace systems::leal::campello_widgets
{

    /**
     * @brief A freehand-drawing canvas backed by a persistent, incrementally
     * updated GPU texture.
     *
     * Rather than re-recording every stroke ever drawn on each repaint (the
     * cost of which would grow without bound over a drawing session),
     * RenderDrawSurface keeps its own dedicated offscreen texture alive for
     * its whole lifetime and, each frame a new pointer segment was drawn,
     * submits only that segment's stamped circles into it via
     * `DrawSurfaceUpdateBeginCmd(clear_first=false)` — see that command's
     * doc comment. The texture is then displayed exactly like any other
     * image (inherited from `RenderImage`), so an idle frame (no new
     * strokes) costs nothing beyond the ordinary image draw.
     *
     * Strokes are built from stamped `DrawCircleCmd`s rather than
     * `drawLine`, since not every backend implements line drawing (Vulkan
     * currently does not) while `drawCircle` is implemented everywhere.
     */
    class RenderDrawSurface : public RenderImage, public GestureArenaMember
    {
    public:
        Color stroke_color      = Color::black();
        Color background_color  = Color::white();
        float stroke_width      = 4.0f;

        RenderDrawSurface();
        ~RenderDrawSurface() override;

        void attach() override;
        void detach() override;

        void performLayout() override;
        void performPaint(PaintContext& ctx, const Offset& offset) override;

        // A draw surface is a leaf (no child RenderBox), so it must claim
        // its own bounds to receive pointer events — see
        // RenderBox::hitTestSelf().
        bool hitTestSelf(const Offset&) const override { return true; }

        // ------------------------------------------------------------------
        // GestureArenaMember
        // ------------------------------------------------------------------

        void acceptGesture(int32_t pointer_id) override;
        void rejectGesture(int32_t pointer_id) override;

        /** @brief Erases all strokes, filling the surface with `background_color`. */
        void clear();

    private:
        void onPointerEvent(const PointerEvent& event);
        void ensureSurface();
        void stampSegment(const Offset& from, const Offset& to, float pressure);

        bool   pressed_    = false;
        bool   lost_arena_ = false;
        std::optional<GestureArenaEntry> arena_entry_;
        Offset last_point_;

        std::shared_ptr<campello_gpu::Texture> surface_texture_;
        Size     surface_logical_size_;        ///< Logical size the texture was allocated at.
        bool     pending_clear_first_ = false; ///< True after first-ever allocation — see ensureSurface().
        std::shared_ptr<campello_gpu::Texture> pending_blit_source_; ///< Previous texture to preserve into surface_texture_ on resize — see ensureSurface().
        DrawList pending_cmds_;                ///< New stamped circles not yet baked into surface_texture_.
    };

} // namespace systems::leal::campello_widgets
