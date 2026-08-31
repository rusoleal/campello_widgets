#pragma once

#include <optional>
#include <campello_widgets/ui/gesture_recognizer.hpp>
#include <campello_widgets/ui/gesture_details.hpp>
#include <campello_widgets/ui/velocity_tracker.hpp>

namespace systems::leal::campello_widgets
{

    class RenderGestureDetector;

    /**
     * @brief Shared down/move/up state machine, arena participation, and
     * velocity tracking for the omnidirectional/horizontal/vertical drag
     * families (Flutter's DragGestureRecognizer base).
     *
     * A subclass decides which axis (if any) gates slop-exceeded
     * recognition via axisDistance() and reads its own owner callback
     * triple via the fire*() hooks -- e.g. HorizontalDragGestureRecognizer
     * only counts x-axis movement toward exceeding pan slop, so a mostly-
     * vertical drag never resolves it even if the diagonal distance is
     * large.
     *
     * Velocity is sampled continuously from pointer-down (not just once
     * dragging actually starts) into two independent VelocityTracker
     * instances (x and y), matching Flutter's DragGestureRecognizer.
     *
     * RenderGestureDetector asserts that at most one of {pan, horizontal,
     * vertical} has any callback set (mirrors Flutter's own GestureDetector
     * assertion) -- an instance with none of its family's callbacks set
     * still joins the arena on every pointer-down like the others (so
     * sweep()'s first-added-wins fallback keeps working the same way
     * regardless of which families exist), it just always concedes on slop
     * exceeded instead of claiming the gesture (see exceedsSlopThreshold()).
     */
    class DragGestureRecognizer : public GestureRecognizer
    {
    public:
        explicit DragGestureRecognizer(RenderGestureDetector& owner);

        void addPointer(const PointerEvent& down) override;
        void handlePointerEvent(const PointerEvent& event) override;

        void acceptGesture(int32_t pointer_id) override;
        void rejectGesture(int32_t pointer_id) override;

    protected:
        /** @brief True if any of this family's update/start/end callbacks are set (down alone doesn't count -- see PanGestureRecognizer::hasHandlers()'s original doc). */
        virtual bool hasHandlers() const noexcept = 0;

        /** @brief Distance (px), along whichever axis this family cares about, of `delta_from_down`. */
        virtual float axisDistance(const Offset& delta_from_down) const noexcept = 0;

        /** @brief The component of `velocity` this family reports as DragEndDetails::primary_velocity. Default 0 -- pan is omnidirectional, so it has no single primary axis (matches Flutter, where PanGestureRecognizer's DragEndDetails.primaryVelocity is null). */
        virtual float primaryVelocityOf(const Offset& /*velocity*/) const noexcept { return 0.0f; }

        virtual void fireDown(const DragDownDetails&)     = 0;
        virtual void fireStart(const DragStartDetails&)   = 0;
        virtual void fireUpdate(const DragUpdateDetails&) = 0;
        virtual void fireEnd(const DragEndDetails&)       = 0;

        RenderGestureDetector& owner_;

    private:
        bool exceedsSlopThreshold() const noexcept;
        void beginDrag();

        std::optional<GestureArenaEntry> arena_entry_;
        bool   has_down_ = false;
        bool   dragging_ = false;
        bool   won_      = false;
        bool   rejected_ = false;
        Offset down_pos_;
        Offset last_pos_;
        Offset down_local_pos_;
        Offset last_local_pos_;
        PointerDeviceKind device_kind_ = PointerDeviceKind::touch;

        VelocityTracker vx_;
        VelocityTracker vy_;
    };

    /** @brief Omnidirectional drag -- any movement past pan slop counts. */
    class PanGestureRecognizer : public DragGestureRecognizer
    {
    public:
        explicit PanGestureRecognizer(RenderGestureDetector& owner) : DragGestureRecognizer(owner) {}

    protected:
        bool  hasHandlers() const noexcept override;
        float axisDistance(const Offset& delta_from_down) const noexcept override;
        void  fireDown(const DragDownDetails& details) override;
        void  fireStart(const DragStartDetails& details) override;
        void  fireUpdate(const DragUpdateDetails& details) override;
        void  fireEnd(const DragEndDetails& details) override;
    };

    /** @brief Drag locked to the x axis -- only horizontal movement counts toward slop. */
    class HorizontalDragGestureRecognizer : public DragGestureRecognizer
    {
    public:
        explicit HorizontalDragGestureRecognizer(RenderGestureDetector& owner) : DragGestureRecognizer(owner) {}

    protected:
        bool  hasHandlers() const noexcept override;
        float axisDistance(const Offset& delta_from_down) const noexcept override;
        float primaryVelocityOf(const Offset& velocity) const noexcept override { return velocity.x; }
        void  fireDown(const DragDownDetails& details) override;
        void  fireStart(const DragStartDetails& details) override;
        void  fireUpdate(const DragUpdateDetails& details) override;
        void  fireEnd(const DragEndDetails& details) override;
    };

    /** @brief Drag locked to the y axis -- only vertical movement counts toward slop. */
    class VerticalDragGestureRecognizer : public DragGestureRecognizer
    {
    public:
        explicit VerticalDragGestureRecognizer(RenderGestureDetector& owner) : DragGestureRecognizer(owner) {}

    protected:
        bool  hasHandlers() const noexcept override;
        float axisDistance(const Offset& delta_from_down) const noexcept override;
        float primaryVelocityOf(const Offset& velocity) const noexcept override { return velocity.y; }
        void  fireDown(const DragDownDetails& details) override;
        void  fireStart(const DragStartDetails& details) override;
        void  fireUpdate(const DragUpdateDetails& details) override;
        void  fireEnd(const DragEndDetails& details) override;
    };

} // namespace systems::leal::campello_widgets
