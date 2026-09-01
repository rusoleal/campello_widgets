#include <campello_widgets/widgets/refresh_indicator.hpp>
#include <campello_widgets/widgets/stack.hpp>
#include <campello_widgets/widgets/positioned.hpp>
#include <campello_widgets/widgets/align.hpp>
#include <campello_widgets/widgets/theme.hpp>
#include <campello_widgets/ui/design_system.hpp>

#include <algorithm>
#include <vector>

namespace systems::leal::campello_widgets
{

    namespace
    {
        // How far above/below its resting position the indicator travels
        // while revealing (0 -> fully hidden above the viewport, 1 -> resting).
        constexpr float kRevealTravel = 40.0f;
    }

    class RefreshIndicatorState : public State<RefreshIndicator>
    {
    public:
        void initState() override { subscribe(); }

        void dispose() override
        {
            alive_ = false;
            unsubscribe();
        }

        void didUpdateWidget(const Widget& old_widget_base) override
        {
            const auto& old_w = static_cast<const RefreshIndicator&>(old_widget_base);
            if (old_w.controller.get() != widget().controller.get())
            {
                if (old_w.controller && listener_id_ != 0)
                    old_w.controller->removeOverscrollListener(listener_id_);
                listener_id_ = 0;
                subscribe();
            }
        }

        WidgetRef build(BuildContext& context) override
        {
            std::vector<WidgetRef> stack_children{widget().child};

            if (refreshing_ || pull_progress_ > 0.0f)
            {
                const DesignSystem* ds = Theme::of(context);

                RefreshIndicatorConfig cfg;
                cfg.pull_progress = pull_progress_;
                cfg.refreshing    = refreshing_;

                // Reveal: parked fully above the viewport at progress 0,
                // slides down to its resting position by progress 1; stays
                // parked at rest while refreshing (cfg.pull_progress is
                // ignored by the design systems once refreshing is true, but
                // the reveal offset here still needs a concrete value).
                const float reveal = refreshing_ ? 1.0f : std::min(pull_progress_, 1.0f);
                const float top    = -kRevealTravel + reveal * kRevealTravel;

                // left+right together force a tight (full-width) constraint on
                // whatever Positioned wraps -- an Align in between loosens that
                // back down so the themed indicator keeps its own natural size
                // and is simply centered within the full-width slot, instead of
                // being stretched to fill it. height_factor=1 keeps Align from
                // also filling (and vertically centering within) the rest of
                // the stack's height, since Positioned only pins `top` here --
                // Align should shrink-wrap to exactly the indicator's own
                // height so it hugs `top`, not the middle of the viewport.
                auto aligned         = std::make_shared<Align>();
                aligned->height_factor = 1.0f;
                aligned->child       = ds->buildRefreshIndicator(cfg);

                auto positioned   = std::make_shared<Positioned>();
                positioned->top   = top;
                positioned->left  = 0.0f;
                positioned->right = 0.0f;
                positioned->child = aligned;

                stack_children.push_back(positioned);
            }

            return std::make_shared<Stack>(std::move(stack_children));
        }

    private:
        void subscribe()
        {
            if (widget().controller)
            {
                listener_id_ = widget().controller->addOverscrollListener(
                    [this](float raw_overscroll) { onOverscroll(raw_overscroll); });
            }
        }

        void unsubscribe()
        {
            if (widget().controller && listener_id_ != 0)
                widget().controller->removeOverscrollListener(listener_id_);
        }

        void onOverscroll(float raw_overscroll)
        {
            // Ignore drag input while a refresh is in flight — a fresh pull
            // during that window doesn't re-arm until the current one settles.
            if (refreshing_) return;

            const float trigger       = std::max(1.0f, widget().trigger_distance);
            const float new_progress  = raw_overscroll / trigger;

            // ScrollController::notifyOverscroll() fires unconditionally on
            // every scroll delta -- not just this widget's own top-boundary
            // pull -- so ordinary in-bounds scrolling of the wrapped list
            // calls this every drag-move tick with raw_overscroll == 0 the
            // whole time. Skipping the no-op case (nothing actually
            // changed since the last call) avoids rebuilding this State --
            // and everything beneath it in the Stack -- on every single
            // scroll tick of ordinary scrolling, which was stealing enough
            // of each frame's budget to make the wrapped list visibly step
            // instead of tracking the drag smoothly.
            const float delta = new_progress - pull_progress_;
            if (delta > -1e-4f && delta < 1e-4f)
                return;

            // Release detection: ScrollController's overscroll channel has no
            // direct "pointer just lifted" signal — a deliberate simplification
            // flagged in the plan. What it does guarantee: a live drag only
            // ever reports an increase or hold of raw_offset_ frame-to-frame
            // (each report is driven by a real pointer move), while release
            // hands off to onTick()'s spring-back easing, which is strictly,
            // monotonically decreasing every tick until it settles at the
            // boundary. So: once pull_progress has reached the trigger
            // threshold, the first decrease from the previous reading is
            // treated as "the user let go." Known edge case this gets wrong:
            // pulling back up slightly *without* releasing, right at/above
            // the threshold, would be misread as a release — acceptable for
            // v1, no clean alternative signal exists without threading a new
            // callback through the render objects' own pointer-up handling.
            const bool was_armed = armed_;
            const bool decreased = new_progress < pull_progress_ - 1e-4f;

            pull_progress_ = new_progress;
            armed_         = pull_progress_ >= 1.0f;

            if (was_armed && decreased)
            {
                triggerRefresh();
                return;
            }

            setState([] {});
        }

        void triggerRefresh()
        {
            refreshing_ = true;
            armed_      = false;
            setState([] {});

            if (widget().on_refresh)
            {
                widget().on_refresh([this] { finishRefresh(); });
            }
            else
            {
                finishRefresh();
            }
        }

        void finishRefresh()
        {
            // Snap back rather than an animated settle — this codebase has
            // no lightweight "ease a scalar back to 0" primitive to reuse
            // here without building a new one, and the plan explicitly
            // called for keeping this simple rather than over-engineering a
            // new transition primitive for one widget.
            if (!alive_) return; // on_refresh completed after this State was disposed
            refreshing_    = false;
            pull_progress_ = 0.0f;
            armed_         = false;
            setState([] {});
        }

        uint64_t listener_id_   = 0;
        float    pull_progress_ = 0.0f;
        bool     armed_         = false;
        bool     refreshing_    = false;
        bool     alive_         = true;
    };

    std::unique_ptr<StateBase> RefreshIndicator::createState() const
    {
        return std::make_unique<RefreshIndicatorState>();
    }

} // namespace systems::leal::campello_widgets
