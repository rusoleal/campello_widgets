#pragma once

#include "campello_widgets_config.h"

// Core
#include <campello_widgets/widgets/widget.hpp>
#include <campello_widgets/widgets/build_context.hpp>
#include <campello_widgets/widgets/element.hpp>
#include <campello_widgets/widgets/stateless_widget.hpp>
#include <campello_widgets/widgets/stateless_element.hpp>
#include <campello_widgets/widgets/stateful_widget.hpp>
#include <campello_widgets/widgets/stateful_element.hpp>

// Layout
#include <campello_widgets/ui/size.hpp>
#include <campello_widgets/ui/offset.hpp>
#include <campello_widgets/ui/rect.hpp>
#include <campello_widgets/ui/edge_insets.hpp>
#include <campello_widgets/ui/box_constraints.hpp>
#include <campello_widgets/ui/render_object.hpp>
#include <campello_widgets/ui/render_box.hpp>
#include <campello_widgets/widgets/render_object_widget.hpp>
#include <campello_widgets/widgets/render_object_element.hpp>

// Rendering
#include <campello_widgets/ui/color.hpp>
#include <campello_widgets/ui/paint.hpp>
#include <campello_widgets/ui/draw_command.hpp>
#include <campello_widgets/ui/canvas.hpp>
#include <campello_widgets/ui/paint_context.hpp>
#include <campello_widgets/ui/renderer.hpp>

// Render primitives
#include <campello_widgets/ui/text_style.hpp>
#include <campello_widgets/ui/text_span.hpp>
#include <campello_widgets/ui/custom_painter.hpp>
#include <campello_widgets/ui/draw_backend.hpp>
#include <campello_widgets/widgets/raw_rectangle.hpp>
#include <campello_widgets/ui/render_rectangle.hpp>
#include <campello_widgets/widgets/raw_text.hpp>
#include <campello_widgets/ui/render_text.hpp>
#include <campello_widgets/widgets/raw_image.hpp>
#include <campello_widgets/ui/render_image.hpp>
#include <campello_widgets/widgets/raw_custom_paint.hpp>
#include <campello_widgets/ui/render_custom_paint.hpp>

// Phase 6 infrastructure
#include <campello_widgets/widgets/single_child_render_object_widget.hpp>
#include <campello_widgets/widgets/single_child_render_object_element.hpp>
#include <campello_widgets/widgets/multi_child_render_object_widget.hpp>
#include <campello_widgets/widgets/multi_child_render_object_element.hpp>

// Phase 6 layout types
#include <campello_widgets/ui/alignment.hpp>
#include <campello_widgets/ui/axis.hpp>
#include <campello_widgets/ui/flex_properties.hpp>
#include <campello_widgets/ui/stack_fit.hpp>
#include <campello_widgets/ui/box_fit.hpp>

// Phase 6 render objects
#include <campello_widgets/ui/render_sized_box.hpp>
#include <campello_widgets/ui/render_padding.hpp>
#include <campello_widgets/ui/render_align.hpp>
#include <campello_widgets/ui/render_colored_box.hpp>
#include <campello_widgets/ui/render_flex.hpp>
#include <campello_widgets/ui/render_stack.hpp>

// Phase 6 elements
#include <campello_widgets/widgets/flex_element.hpp>
#include <campello_widgets/widgets/stack_element.hpp>

// Phase 6 marker widgets
#include <campello_widgets/widgets/flexible.hpp>
#include <campello_widgets/widgets/expanded.hpp>
#include <campello_widgets/widgets/positioned.hpp>

// Phase 6 composited widgets
#include <campello_widgets/widgets/sized_box.hpp>
#include <campello_widgets/widgets/padding.hpp>
#include <campello_widgets/widgets/align.hpp>
#include <campello_widgets/widgets/center.hpp>
#include <campello_widgets/widgets/colored_box.hpp>
#include <campello_widgets/widgets/flex.hpp>
#include <campello_widgets/widgets/row.hpp>
#include <campello_widgets/widgets/column.hpp>
#include <campello_widgets/widgets/stack.hpp>
#include <campello_widgets/widgets/container.hpp>
#include <campello_widgets/widgets/text.hpp>
#include <campello_widgets/widgets/rich_text.hpp>
#include <campello_widgets/widgets/image.hpp>
#include <campello_widgets/widgets/scaffold.hpp>
#include <campello_widgets/widgets/safe_area.hpp>

// Rich text / inline spans
#include <campello_widgets/ui/inline_span.hpp>
#include <campello_widgets/ui/render_paragraph.hpp>

// Phase 7 — Input
#include <campello_widgets/ui/gesture_recognizer.hpp>
#include <campello_widgets/ui/hit_test.hpp>
#include <campello_widgets/ui/pointer_event.hpp>
#include <campello_widgets/ui/pointer_dispatcher.hpp>
#include <campello_widgets/ui/key_event.hpp>
#include <campello_widgets/ui/focus_node.hpp>
#include <campello_widgets/ui/focus_manager.hpp>
#include <campello_widgets/ui/render_gesture_detector.hpp>
#include <campello_widgets/ui/render_focus.hpp>
#include <campello_widgets/widgets/gesture_detector.hpp>
#include <campello_widgets/widgets/focus.hpp>

// Phase 9 — Scrolling
#include <campello_widgets/ui/scroll_physics.hpp>
#include <campello_widgets/ui/scroll_controller.hpp>
#include <campello_widgets/ui/render_single_child_scroll_view.hpp>
#include <campello_widgets/ui/render_list_view.hpp>
#include <campello_widgets/ui/render_grid_view.hpp>
#include <campello_widgets/widgets/single_child_scroll_view.hpp>
#include <campello_widgets/widgets/list_view.hpp>
#include <campello_widgets/widgets/grid_view.hpp>

// Phase 8 — Animation
#include <campello_widgets/ui/ticker.hpp>
#include <campello_widgets/ui/animation_controller.hpp>
#include <campello_widgets/ui/tween.hpp>
#include <campello_widgets/ui/curves.hpp>
#include <campello_widgets/ui/curved_animation.hpp>
#include <campello_widgets/widgets/animated_builder.hpp>
#include <campello_widgets/widgets/animated_container.hpp>

// Phase 4 — Layer compositing
#include <campello_widgets/ui/render_opacity.hpp>
#include <campello_widgets/widgets/opacity.hpp>
#include <campello_widgets/widgets/animated_opacity.hpp>

// Dialog / Overlay / Modal system
#include <campello_widgets/widgets/overlay.hpp>
#include <campello_widgets/widgets/modal_barrier.hpp>
#include <campello_widgets/widgets/dialog.hpp>

// Debug overlays
#include <campello_widgets/ui/debug_flags.hpp>

// Platform-specific entry points
#ifdef CAMPHELLO_PLATFORM_WINDOWS
#include <campello_widgets/windows/run_app.hpp>
#endif

// ---------------------------------------------------------------------------
// Composition utility
// ---------------------------------------------------------------------------
namespace systems::leal::campello_widgets
{
    /**
     * @brief Short alias for std::make_shared<T> — reduces noise in widget trees.
     *
     * @code
     *   make<Padding>(EdgeInsets::all(8), make<Text>("hello"))
     * @endcode
     */
    template<typename T, typename... Args>
    std::shared_ptr<T> make(Args&&... args)
    {
        return std::make_shared<T>(std::forward<Args>(args)...);
    }
} // namespace systems::leal::campello_widgets
