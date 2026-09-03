#include <cmath>
#include <campello_widgets/ui/scale_gesture_recognizer.hpp>
#include <campello_widgets/ui/render_gesture_detector.hpp>
#include <campello_widgets/ui/pointer_dispatcher.hpp>
#include <campello_widgets/ui/gesture_constants.hpp>

namespace systems::leal::campello_widgets
{

namespace
{
    // Avoided <cmath>'s M_PI -- not guaranteed available under strict
    // standard modes on every compiler this codebase targets (notably
    // MSVC without _USE_MATH_DEFINES).
    constexpr float kPi = 3.14159265358979323846f;

    float distance(const Offset& a, const Offset& b) noexcept
    {
        const float dx = a.x - b.x;
        const float dy = a.y - b.y;
        return std::sqrt(dx * dx + dy * dy);
    }
}

    ScaleGestureRecognizer::ScaleGestureRecognizer(RenderGestureDetector& owner)
        : owner_(owner)
    {
    }

    bool ScaleGestureRecognizer::hasHandlers() const noexcept
    {
        return static_cast<bool>(owner_.on_scale_start) || static_cast<bool>(owner_.on_scale_update) ||
               static_cast<bool>(owner_.on_scale_end);
    }

    Offset ScaleGestureRecognizer::centroid() const noexcept
    {
        if (pointers_.empty()) return {};
        Offset sum{};
        for (const auto& [id, p] : pointers_) { sum.x += p.position.x; sum.y += p.position.y; }
        const float n = static_cast<float>(pointers_.size());
        return {sum.x / n, sum.y / n};
    }

    Offset ScaleGestureRecognizer::localCentroid() const noexcept
    {
        if (pointers_.empty()) return {};
        Offset sum{};
        for (const auto& [id, p] : pointers_) { sum.x += p.local_position.x; sum.y += p.local_position.y; }
        const float n = static_cast<float>(pointers_.size());
        return {sum.x / n, sum.y / n};
    }

    void ScaleGestureRecognizer::addPointer(const PointerEvent& down)
    {
        device_kind_ = down.device_kind;
        last_timestamp_ms_ = down.timestamp_ms;
        pointers_[down.pointer_id] = {down.position, down.local_position};

        if (auto* d = PointerDispatcher::activeDispatcher())
            arena_entries_.emplace(down.pointer_id, d->arena().add(down.pointer_id, this));

        if (pointers_.size() == 1)
        {
            down_pos_single_ = down.position;
            return;
        }

        // 2nd+ concurrent pointer: unambiguous multi-touch.
        if (started_)
        {
            // Already running (from a single-finger or smaller multi-finger
            // state) -- just rebase so scale/rotation don't jump once this
            // pointer's span/angle contribution is folded in.
            recomputeBaseline();
            return;
        }

        resolveIfPending(down.pointer_id);
        // A pointer that's been down since before this one may still be
        // waiting on its own solo slop gate -- two simultaneous touches is
        // unambiguous evidence, so stop waiting on it too.
        for (const auto& [id, entry] : arena_entries_)
            resolveIfPending(id);
    }

    void ScaleGestureRecognizer::resolveIfPending(int32_t pointer_id)
    {
        if (resolved_ids_.count(pointer_id)) return;
        auto it = arena_entries_.find(pointer_id);
        if (it == arena_entries_.end()) return;
        resolved_ids_.insert(pointer_id);
        it->second.resolve(hasHandlers() ? GestureDisposition::accepted : GestureDisposition::rejected);
    }

    void ScaleGestureRecognizer::acceptGesture(int32_t pointer_id)
    {
        won_ids_.insert(pointer_id);
        if (started_) return;

        if (pointers_.size() >= 2)
        {
            maybeStart();
            return;
        }

        auto it = pointers_.find(pointer_id);
        if (it != pointers_.end() &&
            distance(it->second.position, down_pos_single_) > computePanSlop(device_kind_))
            maybeStart();
    }

    void ScaleGestureRecognizer::rejectGesture(int32_t pointer_id)
    {
        removePointer(pointer_id);
    }

    void ScaleGestureRecognizer::maybeStart()
    {
        if (started_ || !hasHandlers()) return;
        started_ = true;
        owner_.setPressed(false);

        scale_ = 1.0f; h_scale_ = 1.0f; v_scale_ = 1.0f; rotation_ = 0.0f;
        recomputeBaseline();

        const Offset focal       = centroid();
        const Offset local_focal = localCentroid();

        vx_.reset();
        vy_.reset();
        vx_.addPosition(last_timestamp_ms_, focal.x);
        vy_.addPosition(last_timestamp_ms_, focal.y);

        if (owner_.on_scale_start)
            owner_.on_scale_start(ScaleStartDetails{focal, local_focal, static_cast<int>(pointers_.size())});
    }

    void ScaleGestureRecognizer::recomputeBaseline() noexcept
    {
        if (pointers_.size() < 2)
        {
            prev_span_ = prev_h_span_ = prev_v_span_ = prev_angle_ = 0.0f;
            return;
        }

        const Offset c = centroid();
        float sum_span = 0.0f, sum_h = 0.0f, sum_v = 0.0f, sum_sin2 = 0.0f, sum_cos2 = 0.0f;
        for (const auto& [id, p] : pointers_)
        {
            const float dx = p.position.x - c.x;
            const float dy = p.position.y - c.y;
            sum_span += std::sqrt(dx * dx + dy * dy);
            sum_h    += std::abs(dx);
            sum_v    += std::abs(dy);
            const float theta = std::atan2(dy, dx);
            sum_sin2 += std::sin(2.0f * theta);
            sum_cos2 += std::cos(2.0f * theta);
        }
        const float n = static_cast<float>(pointers_.size());
        prev_span_   = sum_span / n;
        prev_h_span_ = sum_h / n;
        prev_v_span_ = sum_v / n;
        prev_angle_  = 0.5f * std::atan2(sum_sin2, sum_cos2);
    }

    void ScaleGestureRecognizer::updateScale()
    {
        const Offset c  = centroid();
        const Offset lc = localCentroid();

        if (pointers_.size() >= 2)
        {
            float sum_span = 0.0f, sum_h = 0.0f, sum_v = 0.0f, sum_sin2 = 0.0f, sum_cos2 = 0.0f;
            for (const auto& [id, p] : pointers_)
            {
                const float dx = p.position.x - c.x;
                const float dy = p.position.y - c.y;
                sum_span += std::sqrt(dx * dx + dy * dy);
                sum_h    += std::abs(dx);
                sum_v    += std::abs(dy);
                const float theta = std::atan2(dy, dx);
                sum_sin2 += std::sin(2.0f * theta);
                sum_cos2 += std::cos(2.0f * theta);
            }
            const float n         = static_cast<float>(pointers_.size());
            const float cur_span  = sum_span / n;
            const float cur_h     = sum_h / n;
            const float cur_v     = sum_v / n;
            const float cur_angle = 0.5f * std::atan2(sum_sin2, sum_cos2);

            // Guards divide-by-~0 when pointers briefly coincide.
            constexpr float kEpsilon = 0.5f;
            if (prev_span_ > kEpsilon)   scale_   *= cur_span / prev_span_;
            if (prev_h_span_ > kEpsilon) h_scale_ *= cur_h / prev_h_span_;
            if (prev_v_span_ > kEpsilon) v_scale_ *= cur_v / prev_v_span_;

            // rotation_ has period pi, not 2*pi -- see this class's doc
            // comment on the doubling trick -- so unwrap against pi/2.
            float d = cur_angle - prev_angle_;
            while (d >  kPi / 2.0f) d -= kPi;
            while (d <= -kPi / 2.0f) d += kPi;
            rotation_ += d;

            prev_span_ = cur_span; prev_h_span_ = cur_h; prev_v_span_ = cur_v; prev_angle_ = cur_angle;
        }

        vx_.addPosition(last_timestamp_ms_, c.x);
        vy_.addPosition(last_timestamp_ms_, c.y);

        if (owner_.on_scale_update)
            owner_.on_scale_update(
                ScaleUpdateDetails{c, lc, static_cast<int>(pointers_.size()), scale_, h_scale_, v_scale_, rotation_});
    }

    void ScaleGestureRecognizer::handlePointerEvent(const PointerEvent& event)
    {
        switch (event.kind)
        {
        case PointerEventKind::move:
        {
            auto it = pointers_.find(event.pointer_id);
            if (it == pointers_.end()) return;
            it->second.position       = event.position;
            it->second.local_position = event.local_position;
            last_timestamp_ms_        = event.timestamp_ms;

            if (!started_)
            {
                if (pointers_.size() == 1 &&
                    distance(it->second.position, down_pos_single_) > computePanSlop(device_kind_))
                {
                    if (won_ids_.count(event.pointer_id))
                        maybeStart();
                    else
                        resolveIfPending(event.pointer_id);
                }
                // maybeStart() above may have just started the gesture --
                // fall through to report this same move's new position
                // instead of waiting for the next one.
                if (!started_) return;
            }

            updateScale();
            break;
        }
        case PointerEventKind::up:
        case PointerEventKind::cancel:
            removePointer(event.pointer_id);
            break;
        default:
            break;
        }
    }

    void ScaleGestureRecognizer::removePointer(int32_t pointer_id)
    {
        pointers_.erase(pointer_id);
        arena_entries_.erase(pointer_id);
        resolved_ids_.erase(pointer_id);
        won_ids_.erase(pointer_id);

        if (!started_) return;

        if (pointers_.empty())
        {
            const Offset velocity{vx_.getVelocity(), vy_.getVelocity()};
            if (owner_.on_scale_end) owner_.on_scale_end(ScaleEndDetails{velocity, 0});
            started_ = false;
        }
        else
        {
            recomputeBaseline();
        }
    }

} // namespace systems::leal::campello_widgets
