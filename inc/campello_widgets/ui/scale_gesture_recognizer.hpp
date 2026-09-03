#pragma once

#include <map>
#include <optional>
#include <unordered_set>
#include <campello_widgets/ui/gesture_recognizer.hpp>
#include <campello_widgets/ui/gesture_details.hpp>
#include <campello_widgets/ui/velocity_tracker.hpp>

namespace systems::leal::campello_widgets
{

    class RenderGestureDetector;

    /**
     * @brief Recognizes pinch/rotate (and single-finger pan-as-scale) across
     * any number of concurrent pointers on one detector.
     *
     * Unlike the other recognizers, a scale gesture can be simultaneously
     * "made of" several pointer_ids -- each still joins the SAME underlying
     * gesture arena independently (one arena per pointer_id, same as every
     * other recognizer), so this single ScaleGestureRecognizer instance is a
     * GestureArenaMember of several arenas at once and tracks per-pointer
     * win/resolve state in `won_ids_`/`resolved_ids_`.
     *
     * Disambiguation: the first pointer down behaves like a single-finger
     * PanGestureRecognizer -- it must exceed pan slop before the arena entry
     * resolves (see DragGestureRecognizer for the identical pattern). A
     * second (or third, ...) pointer arriving is unambiguous evidence of an
     * intentional multi-touch gesture, so it (and the first pointer, if
     * still pending) resolve as accepted immediately, without waiting for
     * slop -- mirrors Flutter's real ScaleGestureRecognizer not gating
     * additional fingers on movement.
     *
     * Math: focal point is the centroid of all live pointer positions;
     * scale/horizontal_scale/vertical_scale are the ratio of the current
     * average (or per-axis) distance from the centroid to the previous
     * frame's, accumulated multiplicatively frame-to-frame (not against a
     * single fixed "gesture start" reference) -- this makes a mid-gesture
     * change in pointer count (a 2nd finger joining a 1-finger pan, or one
     * of two fingers lifting) continuous rather than jumping, since the
     * baseline is simply rebased to the current geometry at the moment the
     * pointer set changes (see recomputeBaseline()) rather than recomputing
     * from a stale original reference.
     *
     * Rotation uses the doubling trick standard for averaging undirected
     * line orientations (each pointer's angle from the centroid is doubled
     * before being summed as a unit vector, then the result is halved) --
     * needed because two points are always exactly 180 degrees apart around
     * their own centroid, so a naive average-of-angles always cancels to
     * zero for the two-finger case. This makes `rotation` inherently
     * periodic with period pi (a line has no direction, only an
     * orientation), so its frame-to-frame delta is unwrapped against pi/2,
     * not the usual pi.
     *
     * A single-pointer gesture never touches scale/horizontal_scale/
     * vertical_scale/rotation past their identity defaults (1/1/1/0) --
     * span-from-centroid is undefined for one point -- so a GestureDetector
     * with only on_scale_* callbacks set still gets Flutter-parity
     * single-finger drag behavior (a moving focal_point, scale pinned at
     * 1.0) rather than doing nothing until a second finger appears.
     */
    class ScaleGestureRecognizer : public GestureRecognizer
    {
    public:
        explicit ScaleGestureRecognizer(RenderGestureDetector& owner);

        void addPointer(const PointerEvent& down) override;
        void handlePointerEvent(const PointerEvent& event) override;

        void acceptGesture(int32_t pointer_id) override;
        void rejectGesture(int32_t pointer_id) override;

    private:
        struct PointerState
        {
            Offset position;
            Offset local_position;
        };

        bool   hasHandlers() const noexcept;
        Offset centroid() const noexcept;
        Offset localCentroid() const noexcept;

        void resolveIfPending(int32_t pointer_id);
        void maybeStart();
        void recomputeBaseline() noexcept;
        void updateScale();
        void removePointer(int32_t pointer_id);

        RenderGestureDetector& owner_;

        std::map<int32_t, PointerState>        pointers_;
        std::map<int32_t, GestureArenaEntry>   arena_entries_;
        std::unordered_set<int32_t>            won_ids_;
        std::unordered_set<int32_t>            resolved_ids_;

        bool     started_ = false;
        Offset   down_pos_single_;   ///< Down position of the first pointer, for its solo slop gate.
        PointerDeviceKind device_kind_ = PointerDeviceKind::touch;

        /// Timestamp of whichever PointerEvent most recently reached
        /// addPointer()/handlePointerEvent() -- maybeStart()/updateScale()
        /// read this to seed/feed vx_/vy_ instead of calling
        /// steady_clock::now() themselves (see VelocityTracker's doc
        /// comment for why), since neither is itself a PointerEvent handler.
        uint64_t last_timestamp_ms_ = 0;

        // Cumulative gesture output, carried across pointer-count changes.
        float scale_    = 1.0f;
        float h_scale_  = 1.0f;
        float v_scale_  = 1.0f;
        float rotation_ = 0.0f;

        // Previous frame's geometry -- the rolling baseline the next
        // updateScale() call computes a delta against.
        float prev_span_   = 0.0f;
        float prev_h_span_ = 0.0f;
        float prev_v_span_ = 0.0f;
        float prev_angle_  = 0.0f;

        VelocityTracker vx_;
        VelocityTracker vy_;
    };

} // namespace systems::leal::campello_widgets
