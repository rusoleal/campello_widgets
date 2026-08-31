#include "material_ink_response.hpp"

#include <campello_widgets/widgets/gesture_detector.hpp>
#include <campello_widgets/widgets/mouse_region.hpp>
#include <campello_widgets/widgets/clip_rrect.hpp>
#include <campello_widgets/widgets/stack.hpp>
#include <campello_widgets/widgets/positioned.hpp>
#include <campello_widgets/widgets/raw_custom_paint.hpp>
#include <campello_widgets/ui/custom_painter.hpp>
#include <campello_widgets/ui/canvas.hpp>
#include <campello_widgets/ui/paint.hpp>
#include <campello_widgets/ui/rect.hpp>
#include <campello_widgets/ui/rrect.hpp>
#include <campello_widgets/ui/animation_controller.hpp>
#include <campello_widgets/ui/system_mouse_cursor.hpp>
#include <campello_widgets/ui/focus_manager.hpp>

#include <algorithm>
#include <cmath>

namespace systems::leal::campello_widgets
{
    namespace
    {
        // Material 3 state-layer opacities (hover/focus/pressed on-color overlay).
        constexpr float kHoverAlpha   = 0.08f;
        constexpr float kFocusAlpha   = 0.10f;
        constexpr float kPressedAlpha = 0.12f;

        class InkResponsePainter : public CustomPainter
        {
        public:
            bool   hovered         = false;
            bool   focused         = false; // already gated on FocusHighlightMode::keyboard by the caller
            bool   ripple_active   = false;
            float  ripple_progress = 0.0f; // 0..1, driven by the ripple AnimationController
            Offset ripple_origin;
            Color  overlay_color;
            float  border_radius   = 0.0f;

            void paint(Canvas& canvas, Size size) override
            {
                const Rect  bounds = Rect::fromLTWH(0.0f, 0.0f, size.width, size.height);
                const RRect rrect  = RRect::fromRectAndRadius(bounds, border_radius);

                // Focus and hover are the same tinted-fill state layer, just
                // a different alpha tier -- real Material doesn't stack them,
                // it takes the strongest applicable one.
                const float base_alpha = std::max(focused ? kFocusAlpha : 0.0f,
                                                   hovered ? kHoverAlpha : 0.0f);
                if (base_alpha > 0.0f)
                {
                    canvas.drawRRect(rrect, Paint::filled(Color::fromRGBA(
                        overlay_color.r, overlay_color.g, overlay_color.b, base_alpha)));
                }

                if (ripple_active && ripple_progress > 0.0f)
                {
                    // Distance from the actual tap-down point to the
                    // farthest corner -- not a fixed diagonal-based
                    // constant, which only covers the shape correctly for
                    // a centered tap. An off-center tap needs a *larger*
                    // radius to reach the far corner than a centered one
                    // does (being close to one corner means being far from
                    // the opposite one), so a fixed radius undershoots and
                    // the ripple visibly stops short of full coverage on
                    // off-center presses. 1.05x for a small margin so the
                    // corner is cleanly covered, not just barely grazed.
                    float max_radius = 0.0f;
                    for (float cx : {0.0f, size.width})
                    {
                        for (float cy : {0.0f, size.height})
                        {
                            const float dx = cx - ripple_origin.x;
                            const float dy = cy - ripple_origin.y;
                            max_radius = std::max(max_radius, std::sqrt(dx * dx + dy * dy));
                        }
                    }
                    max_radius *= 1.05f;
                    // Simplification vs. real Material: our single
                    // AnimationController drives both growth AND (via
                    // reverse() on release) shrink-back, whereas Flutter's
                    // ink splash keeps radius pinned at max and only fades
                    // alpha out on release. Visually still reads clearly as
                    // "a press happened here", just not pixel-identical.
                    const float radius = max_radius * ripple_progress;
                    // Fade the circle in over the first third of growth so it
                    // doesn't pop in at full alpha from a zero-radius point.
                    const float alpha = kPressedAlpha * std::min(ripple_progress * 3.0f, 1.0f);
                    canvas.drawCircle(ripple_origin, radius, Paint::filled(Color::fromRGBA(
                        overlay_color.r, overlay_color.g, overlay_color.b, alpha)));
                }
            }

            bool shouldRepaint(const CustomPainter&) const override { return true; }
        };

    } // namespace

    class MaterialInkResponseState : public State<MaterialInkResponse>
    {
    public:
        void initState() override
        {
            ripple_ctrl_        = std::make_unique<AnimationController>(300.0);
            ripple_listener_id_ = ripple_ctrl_->addListener([this]() { setState([](){}); });
        }

        void dispose() override
        {
            if (ripple_ctrl_ && ripple_listener_id_ != 0)
                ripple_ctrl_->removeListener(ripple_listener_id_);
        }

        WidgetRef build(BuildContext&) override
        {
            const auto& w = widget();
            const bool  interactive = static_cast<bool>(w.on_tap);

            auto painter            = std::make_shared<InkResponsePainter>();
            painter->hovered        = interactive && hovered_;
            painter->focused        = interactive && focused_ &&
                FocusManager::highlightMode() == FocusHighlightMode::keyboard;
            painter->ripple_active  = interactive && ripple_active_;
            painter->ripple_progress = ripple_ctrl_ ? static_cast<float>(ripple_ctrl_->value()) : 0.0f;
            painter->ripple_origin  = ripple_origin_;
            painter->overlay_color  = w.overlay_color;
            painter->border_radius  = w.border_radius;

            // Only the ripple/hover overlay is clipped to border_radius --
            // w.child (the button's own fill+border) must never pass
            // through a clip at all. A stroked border is centered on its
            // shape's boundary and legitimately extends half its width
            // outward; clipping the whole stack at the exact rect (as an
            // earlier version of this did) cut that outward half away,
            // reproducing the same border-vanishing symptom the Metal/
            // DirectX/Vulkan shader quad-inflation fix addressed, but via
            // an explicit clip this time instead of missing quad geometry.
            auto overlay = RawCustomPaint::create(painter);
            auto clipped_overlay = std::make_shared<ClipRRect>(w.border_radius, overlay);

            auto positioned_overlay = std::make_shared<Positioned>(
                0.0f, 0.0f, 0.0f, 0.0f, std::optional<float>{}, std::optional<float>{}, clipped_overlay);

            auto stack = std::make_shared<Stack>();
            stack->children = {w.child, positioned_overlay};

            if (!interactive)
            {
                // No callbacks wired at all -- matches a disabled control:
                // no hover cursor, no overlay, no ripple, just the content.
                return stack;
            }

            auto gesture          = std::make_shared<GestureDetector>();
            gesture->child        = stack;
            gesture->on_tap       = w.on_tap;
            gesture->focus_node   = w.focus_node;
            gesture->autofocus    = w.autofocus;
            gesture->focusable    = true;
            gesture->on_pan_down  = [this](DragDownDetails details) {
                ripple_origin_ = details.local_position;
                ripple_active_ = true;
                if (ripple_ctrl_) ripple_ctrl_->forward(0.0);
            };
            gesture->on_press_change = [this](bool pressed) {
                pressed_ = pressed;
                if (!pressed && ripple_ctrl_) ripple_ctrl_->reverse();
                setState([](){});
            };
            gesture->on_focus_change = [this](bool has_focus) {
                focused_ = has_focus;
                setState([](){});
            };

            auto hoverable      = std::make_shared<MouseRegion>();
            hoverable->cursor   = SystemMouseCursor::pointer;
            hoverable->on_enter = [this]() { hovered_ = true;  setState([](){}); };
            hoverable->on_exit  = [this]() { hovered_ = false; setState([](){}); };
            hoverable->child    = gesture;

            return hoverable;
        }

    private:
        std::unique_ptr<AnimationController> ripple_ctrl_;
        uint64_t ripple_listener_id_ = 0;
        bool     hovered_       = false;
        bool     focused_       = false;
        bool     pressed_       = false;
        bool     ripple_active_ = false;
        Offset   ripple_origin_;
    };

    std::unique_ptr<StateBase> MaterialInkResponse::createState() const
    {
        return std::make_unique<MaterialInkResponseState>();
    }

} // namespace systems::leal::campello_widgets
