#pragma once

#include "campello_widgets_config.h"

// Key system
#include <campello_widgets/ui/key.hpp>

// Core
#include <campello_widgets/widgets/widget.hpp>
#include <campello_widgets/widgets/build_context.hpp>
#include <campello_widgets/widgets/element.hpp>
#include <campello_widgets/widgets/stateless_widget.hpp>
#include <campello_widgets/widgets/stateless_element.hpp>
#include <campello_widgets/widgets/stateful_widget.hpp>
#include <campello_widgets/widgets/stateful_element.hpp>
#include <campello_widgets/widgets/inherited_widget.hpp>
#include <campello_widgets/widgets/inherited_element.hpp>
#include <campello_widgets/widgets/media_query.hpp>

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

// Image loading infrastructure
#include <campello_widgets/ui/image_provider.hpp>
#include <campello_widgets/ui/image_cache.hpp>
#include <campello_widgets/ui/image_loader.hpp>
#include <campello_widgets/ui/http_client.hpp>
#include <campello_widgets/widgets/image_widget.hpp>

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
#include <campello_widgets/widgets/keyboard_listener.hpp>

// Phase 9 — Scrolling
#include <campello_widgets/ui/scroll_physics.hpp>
#include <campello_widgets/ui/scroll_controller.hpp>
#include <campello_widgets/ui/render_single_child_scroll_view.hpp>
#include <campello_widgets/ui/render_list_view.hpp>
#include <campello_widgets/ui/render_grid_view.hpp>
#include <campello_widgets/widgets/single_child_scroll_view.hpp>
#include <campello_widgets/widgets/list_view.hpp>
#include <campello_widgets/widgets/grid_view.hpp>

// Two-dimensional scrollables (TableView, TreeView)
#include <campello_widgets/ui/span.hpp>
#include <campello_widgets/ui/tree_node.hpp>
#include <campello_widgets/ui/render_table_view.hpp>
#include <campello_widgets/ui/render_tree_view.hpp>
#include <campello_widgets/widgets/table_view.hpp>
#include <campello_widgets/widgets/tree_view.hpp>

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

// Transform
#include <campello_widgets/ui/render_transform.hpp>
#include <campello_widgets/widgets/transform.hpp>

// Dialog / Overlay / Modal system
#include <campello_widgets/widgets/overlay.hpp>
#include <campello_widgets/widgets/modal_barrier.hpp>
#include <campello_widgets/widgets/dialog.hpp>

// Platform Menu Bar
#include <campello_widgets/widgets/platform_menu.hpp>
#include <campello_widgets/widgets/platform_menu_delegate.hpp>
#include <campello_widgets/widgets/platform_menu_bar.hpp>

// Phase 13 — Decoration
#include <campello_widgets/ui/box_shadow.hpp>
#include <campello_widgets/ui/box_border.hpp>
#include <campello_widgets/ui/box_decoration.hpp>
#include <campello_widgets/ui/render_decorated_box.hpp>
#include <campello_widgets/widgets/decorated_box.hpp>

// Phase 13 — Layout widgets
#include <campello_widgets/ui/wrap_properties.hpp>
#include <campello_widgets/ui/render_constrained_box.hpp>
#include <campello_widgets/ui/render_aspect_ratio.hpp>
#include <campello_widgets/ui/render_fractionally_sized_box.hpp>
#include <campello_widgets/ui/render_intrinsic_width.hpp>
#include <campello_widgets/ui/render_intrinsic_height.hpp>
#include <campello_widgets/ui/render_wrap.hpp>
#include <campello_widgets/ui/render_clip_rect.hpp>
#include <campello_widgets/ui/render_clip_rrect.hpp>
#include <campello_widgets/ui/render_clip_oval.hpp>
#include <campello_widgets/ui/render_clip_path.hpp>
#include <campello_widgets/widgets/constrained_box.hpp>
#include <campello_widgets/widgets/aspect_ratio.hpp>
#include <campello_widgets/widgets/fractionally_sized_box.hpp>
#include <campello_widgets/widgets/intrinsic_width.hpp>
#include <campello_widgets/widgets/intrinsic_height.hpp>
#include <campello_widgets/widgets/wrap.hpp>
#include <campello_widgets/widgets/clip_rect.hpp>
#include <campello_widgets/widgets/clip_rrect.hpp>
#include <campello_widgets/widgets/clip_oval.hpp>
#include <campello_widgets/widgets/clip_path.hpp>

// Animation transitions (explicit)
#include <campello_widgets/widgets/fade_transition.hpp>
#include <campello_widgets/widgets/scale_transition.hpp>
#include <campello_widgets/widgets/slide_transition.hpp>
#include <campello_widgets/widgets/rotation_transition.hpp>
#include <campello_widgets/widgets/fractional_translation.hpp>

// Animation transitions (implicit)
#include <campello_widgets/ui/render_animated_size.hpp>
#include <campello_widgets/widgets/animated_align.hpp>
#include <campello_widgets/widgets/animated_positioned.hpp>
#include <campello_widgets/widgets/animated_switcher.hpp>
#include <campello_widgets/widgets/animated_size.hpp>

// State Management
#include <campello_widgets/ui/value_notifier.hpp>
#include <campello_widgets/ui/async_snapshot.hpp>
#include <campello_widgets/ui/stream.hpp>
#include <campello_widgets/widgets/layout_builder.hpp>
#include <campello_widgets/widgets/value_listenable_builder.hpp>
#include <campello_widgets/widgets/future_builder.hpp>
#include <campello_widgets/widgets/stream_builder.hpp>

// Input / Forms
#include <campello_widgets/ui/system_mouse_cursor.hpp>
#include <campello_widgets/ui/render_mouse_region.hpp>
#include <campello_widgets/ui/render_slider.hpp>
#include <campello_widgets/ui/render_text_field.hpp>
#include <campello_widgets/ui/text_editing_controller.hpp>
#include <campello_widgets/widgets/mouse_region.hpp>
#include <campello_widgets/widgets/checkbox.hpp>
#include <campello_widgets/widgets/switch.hpp>
#include <campello_widgets/widgets/slider.hpp>
#include <campello_widgets/widgets/radio_group.hpp>
#include <campello_widgets/widgets/radio.hpp>
#include <campello_widgets/widgets/text_field.hpp>

// Drag and drop
#include <campello_widgets/ui/drag_details.hpp>
#include <campello_widgets/widgets/draggable.hpp>
#include <campello_widgets/widgets/drag_target.hpp>

// Navigation
#include <campello_widgets/widgets/navigator.hpp>

// Utility widgets
#include <campello_widgets/widgets/button.hpp>
#include <campello_widgets/widgets/divider.hpp>
#include <campello_widgets/widgets/circular_progress_indicator.hpp>
#include <campello_widgets/widgets/linear_progress_indicator.hpp>
#include <campello_widgets/widgets/tooltip.hpp>

// Decoration widgets
#include <campello_widgets/ui/image_filter.hpp>
#include <campello_widgets/ui/shader.hpp>
#include <campello_widgets/ui/render_backdrop_filter.hpp>
#include <campello_widgets/ui/render_shader_mask.hpp>
#include <campello_widgets/widgets/backdrop_filter.hpp>
#include <campello_widgets/widgets/shader_mask.hpp>
#include <campello_widgets/ui/debug_flags.hpp>

// High-priority composited widgets
#include <campello_widgets/widgets/card.hpp>
#include <campello_widgets/widgets/list_tile.hpp>
#include <campello_widgets/widgets/floating_action_button.hpp>
#include <campello_widgets/widgets/snack_bar.hpp>
#include <campello_widgets/widgets/popup_menu_button.hpp>
#include <campello_widgets/widgets/dropdown_button.hpp>
#include <campello_widgets/widgets/tab_bar.hpp>
#include <campello_widgets/ui/render_page_view.hpp>
#include <campello_widgets/widgets/page_view.hpp>

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
     *   mw<Padding>(EdgeInsets::all(8), mw<Text>("hello"))
     * @endcode
     */
    template<typename T, typename... Args>
    std::shared_ptr<T> mw(Args&&... args)
    {
        auto w = std::make_shared<T>(std::forward<Args>(args)...);
        w->captureLocation();
        return w;
    }
} // namespace systems::leal::campello_widgets
