#include <campello_widgets/ui/render_gesture_detector.hpp>
#include <campello_widgets/ui/pointer_dispatcher.hpp>
#include <campello_widgets/ui/dirty_region.hpp>
#include <campello_widgets/ui/focus_manager.hpp>

#include <cassert>

namespace systems::leal::campello_widgets
{

    RenderGestureDetector::RenderGestureDetector()
        : tap_recognizer_(std::make_unique<TapGestureRecognizer>(
              *this, PointerButton::primary,
              &RenderGestureDetector::on_tap, &RenderGestureDetector::on_tap_down,
              &RenderGestureDetector::on_tap_up, &RenderGestureDetector::on_tap_cancel,
              &RenderGestureDetector::on_double_tap))
        , secondary_tap_recognizer_(std::make_unique<TapGestureRecognizer>(
              *this, PointerButton::secondary,
              &RenderGestureDetector::on_secondary_tap, &RenderGestureDetector::on_secondary_tap_down,
              &RenderGestureDetector::on_secondary_tap_up, &RenderGestureDetector::on_secondary_tap_cancel,
              nullptr))
        , tertiary_tap_recognizer_(std::make_unique<TapGestureRecognizer>(
              *this, PointerButton::tertiary,
              nullptr, &RenderGestureDetector::on_tertiary_tap_down,
              &RenderGestureDetector::on_tertiary_tap_up, &RenderGestureDetector::on_tertiary_tap_cancel,
              nullptr))
        , long_press_recognizer_(std::make_unique<LongPressGestureRecognizer>(*this))
        , pan_recognizer_(std::make_unique<PanGestureRecognizer>(*this))
        , horizontal_drag_recognizer_(std::make_unique<HorizontalDragGestureRecognizer>(*this))
        , vertical_drag_recognizer_(std::make_unique<VerticalDragGestureRecognizer>(*this))
        , scale_recognizer_(std::make_unique<ScaleGestureRecognizer>(*this))
        , force_press_recognizer_(std::make_unique<ForcePressGestureRecognizer>(*this))
    {
    }

    RenderGestureDetector::~RenderGestureDetector()
    {
        tap_recognizer_->dispose();
        secondary_tap_recognizer_->dispose();
        tertiary_tap_recognizer_->dispose();
        long_press_recognizer_->dispose();
        pan_recognizer_->dispose();
        horizontal_drag_recognizer_->dispose();
        vertical_drag_recognizer_->dispose();
        scale_recognizer_->dispose();
        force_press_recognizer_->dispose();
        if (auto* d = PointerDispatcher::activeDispatcher())
        {
            d->removeHandler(this);
            d->removeTickHandler(this);
        }
        if (registered_node_)
        {
            registered_node_->on_key           = nullptr;
            registered_node_->on_focus_changed = nullptr;
            if (auto* fm = FocusManager::activeManager())
                fm->unregisterNode(registered_node_.get());
        }
    }

    void RenderGestureDetector::attach()
    {
        if (auto* d = PointerDispatcher::activeDispatcher())
        {
            d->addHandler(this, [this](const PointerEvent& e) { onPointerEvent(e); });
            d->addTickHandler(this, [this](uint64_t now) { onTick(now); });
        }
        attached_ = true;
        if (registered_node_)
        {
            if (auto* fm = FocusManager::activeManager())
            {
                fm->registerNode(registered_node_.get());
                if (autofocus_) fm->requestFocus(registered_node_.get());
            }

            // See FocusNode::parent()'s doc comment -- the ancestor chain
            // is resolved lazily from this owner pointer, not walked here.
            registered_node_->setOwner(this);
        }
    }

    void RenderGestureDetector::detach()
    {
        tap_recognizer_->dispose();
        secondary_tap_recognizer_->dispose();
        tertiary_tap_recognizer_->dispose();
        long_press_recognizer_->dispose();
        pan_recognizer_->dispose();
        horizontal_drag_recognizer_->dispose();
        vertical_drag_recognizer_->dispose();
        scale_recognizer_->dispose();
        force_press_recognizer_->dispose();
        if (auto* d = PointerDispatcher::activeDispatcher())
        {
            d->removeHandler(this);
            d->removeTickHandler(this);
        }
        if (registered_node_)
        {
            if (auto* fm = FocusManager::activeManager())
                fm->unregisterNode(registered_node_.get());
            registered_node_->setOwner(nullptr);
        }
        attached_ = false;
    }

    // -------------------------------------------------------------------------
    // Focus
    // -------------------------------------------------------------------------

    void RenderGestureDetector::setFocusConfig(std::shared_ptr<FocusNode> node, bool autofocus, bool focusable)
    {
        ext_focus_node_ = std::move(node);
        autofocus_      = autofocus;
        focusable_      = focusable;
        syncFocusRegistration();
    }

    void RenderGestureDetector::syncFocusRegistration()
    {
        std::shared_ptr<FocusNode> desired;
        if (focusable_)
        {
            if (ext_focus_node_)
            {
                desired = ext_focus_node_;
            }
            else
            {
                if (!own_focus_node_) own_focus_node_ = std::make_shared<FocusNode>();
                desired = own_focus_node_;
            }
        }

        if (desired.get() == registered_node_.get())
            return;

        auto* fm = FocusManager::activeManager();

        if (registered_node_)
        {
            registered_node_->on_key           = nullptr;
            registered_node_->on_focus_changed = nullptr;
            if (attached_)
            {
                if (fm) fm->unregisterNode(registered_node_.get());
                registered_node_->setOwner(nullptr);
            }
        }

        registered_node_ = desired;

        if (registered_node_)
        {
            registered_node_->on_focus_changed = [this](bool has_focus) {
                if (on_focus_change) on_focus_change(has_focus);
            };
            registered_node_->on_key = [this](const KeyEvent& event) -> bool {
                return handleFocusKey(event);
            };
            if (attached_)
            {
                if (fm)
                {
                    fm->registerNode(registered_node_.get());
                    if (autofocus_) fm->requestFocus(registered_node_.get());
                }
                registered_node_->setOwner(this);
            }
        }
    }

    bool RenderGestureDetector::handleFocusKey(const KeyEvent& event)
    {
        // Space/Enter activates a focused control on key-down only — not
        // on every auto-repeat, which would otherwise fire on_tap
        // repeatedly for as long as the key is held.
        if (event.kind != KeyEventKind::down) return false;
        if (event.key_code != KeyCode::space && event.key_code != KeyCode::enter) return false;
        if (!on_tap) return false;
        on_tap();
        return true;
    }

    // -------------------------------------------------------------------------
    // Shared owner-level state, driven by this detector's own recognizers.
    // -------------------------------------------------------------------------

    void RenderGestureDetector::setPressed(bool pressed)
    {
        if (pressed_ == pressed) return;
        pressed_ = pressed;
        if (on_press_change) on_press_change(pressed_);
    }

    void RenderGestureDetector::requestFocusOnTap()
    {
        // A completed tap grabs keyboard focus for this control, mirroring
        // clicking a Flutter button focusing it -- but marked as
        // pointer-driven so a theme's focus-ring painter doesn't draw a
        // ring just because a click happened to also focus this control
        // (see FocusHighlightMode's doc comment).
        if (registered_node_)
        {
            FocusManager::notePointerInteraction();
            registered_node_->requestFocus();
        }
    }

    // -------------------------------------------------------------------------

    void RenderGestureDetector::onPointerEvent(const PointerEvent& event)
    {
        switch (event.kind)
        {
        case PointerEventKind::down:
        {
            // Pan, horizontal-drag, vertical-drag, and scale are mutually
            // exclusive families -- mirrors Flutter's own GestureDetector
            // assertion (onScale* cannot coexist with onPan*/
            // onHorizontalDrag*/onVerticalDrag*).
            const bool has_pan =
                static_cast<bool>(on_pan_update) || static_cast<bool>(on_pan_end) || static_cast<bool>(on_pan_start);
            const bool has_horizontal = static_cast<bool>(on_horizontal_drag_update) ||
                                         static_cast<bool>(on_horizontal_drag_end) ||
                                         static_cast<bool>(on_horizontal_drag_start);
            const bool has_vertical = static_cast<bool>(on_vertical_drag_update) ||
                                       static_cast<bool>(on_vertical_drag_end) ||
                                       static_cast<bool>(on_vertical_drag_start);
            const bool has_scale = static_cast<bool>(on_scale_update) || static_cast<bool>(on_scale_end) ||
                                    static_cast<bool>(on_scale_start);
            assert((static_cast<int>(has_pan) + static_cast<int>(has_horizontal) + static_cast<int>(has_vertical) +
                    static_cast<int>(has_scale)) <= 1 &&
                   "GestureDetector: on_pan_*, on_horizontal_drag_*, on_vertical_drag_*, and on_scale_* are "
                   "mutually exclusive gesture families -- set at most one of the four on a single detector.");

            tap_recognizer_->addPointer(event);
            secondary_tap_recognizer_->addPointer(event);
            tertiary_tap_recognizer_->addPointer(event);
            long_press_recognizer_->addPointer(event);
            pan_recognizer_->addPointer(event);
            horizontal_drag_recognizer_->addPointer(event);
            vertical_drag_recognizer_->addPointer(event);
            scale_recognizer_->addPointer(event);
            force_press_recognizer_->addPointer(event);
            setPressed(true);
            break;
        }

        case PointerEventKind::move:
            tap_recognizer_->handlePointerEvent(event);
            secondary_tap_recognizer_->handlePointerEvent(event);
            tertiary_tap_recognizer_->handlePointerEvent(event);
            long_press_recognizer_->handlePointerEvent(event);
            pan_recognizer_->handlePointerEvent(event);
            horizontal_drag_recognizer_->handlePointerEvent(event);
            vertical_drag_recognizer_->handlePointerEvent(event);
            scale_recognizer_->handlePointerEvent(event);
            force_press_recognizer_->handlePointerEvent(event);
            break;

        case PointerEventKind::up:
            tap_recognizer_->handlePointerEvent(event);
            secondary_tap_recognizer_->handlePointerEvent(event);
            tertiary_tap_recognizer_->handlePointerEvent(event);
            long_press_recognizer_->handlePointerEvent(event);
            pan_recognizer_->handlePointerEvent(event);
            horizontal_drag_recognizer_->handlePointerEvent(event);
            vertical_drag_recognizer_->handlePointerEvent(event);
            scale_recognizer_->handlePointerEvent(event);
            force_press_recognizer_->handlePointerEvent(event);
            setPressed(false);
            break;

        case PointerEventKind::cancel:
            tap_recognizer_->handlePointerEvent(event);
            secondary_tap_recognizer_->handlePointerEvent(event);
            tertiary_tap_recognizer_->handlePointerEvent(event);
            long_press_recognizer_->handlePointerEvent(event);
            pan_recognizer_->handlePointerEvent(event);
            horizontal_drag_recognizer_->handlePointerEvent(event);
            vertical_drag_recognizer_->handlePointerEvent(event);
            scale_recognizer_->handlePointerEvent(event);
            force_press_recognizer_->handlePointerEvent(event);
            setPressed(false);
            break;

        case PointerEventKind::scroll:
            if (on_scroll)
                on_scroll({event.scroll_delta_x, event.scroll_delta_y});
            break;
        }
    }

    bool RenderGestureDetector::hitTest(HitTestResult& result, const Offset& position)
    {
        if (position.x < 0.0f || position.x >= size_.width ||
            position.y < 0.0f || position.y >= size_.height)
            return false;

        const bool child_hit = hitTestChildren(result, position);

        if (behavior == HitTestBehavior::deferToChild)
        {
            // A child already claimed this point -- stay out of the hit
            // path (and therefore out of the pointer dispatch / gesture
            // arena for this event) entirely, rather than merely refusing
            // to add our own entry while still having been "consulted".
            if (child_hit) return true;
            result.add({this, position, /*opaque=*/true});
            return true;
        }

        // opaque and translucent both always claim the point regardless of
        // whether a child also did; they differ only in whether the entry
        // they add blocks an ancestor multi-child hit-test loop (e.g.
        // RenderStack) from continuing to lower-painted siblings.
        result.add({this, position, /*opaque=*/behavior != HitTestBehavior::translucent});
        return true;
    }

    void RenderGestureDetector::performLayout()
    {
        // Transparent: pass constraints through unchanged (same as RenderMouseRegion).
        // Using RenderBox::performLayout's loosen+center default would give children
        // loose constraints, causing Stacks and other fill-to-max widgets to claim
        // infinite height when placed in an unbounded main axis (e.g. Column).
        if (child_)
        {
            layoutChild(*child_, constraints_);
            size_ = child_->size();
        }
        else
        {
            size_ = constraints_.constrain({0.0f, 0.0f});
        }
    }

    void RenderGestureDetector::performPaint(PaintContext& ctx, const Offset& offset)
    {
        // Convert to tree-local space (subtract the safe-area inset baked
        // into `offset` — see RenderObject::setActivePaintOriginOffset's doc
        // comment). globalOffset() feeds anchor positioning for overlays
        // (e.g. DropdownButton's/PopupMenuButton's menu, via Positioned)
        // that live in the Overlay's own top-level coordinate space, not
        // this node's tree-local one — leaving this un-projected would
        // offset anchored overlays by the safe-area inset whenever it's
        // non-zero (any iPhone), AND — see projectedBounds()'s doc — by
        // any ambient Canvas transform currently active (critically, a
        // scrolled ancestor's `canvas.translate()`, which is independent of
        // `offset` and only applied at paint time): an anchor button inside
        // a SingleChildScrollView reports its pre-scroll logical position
        // without this, so its menu opens shifted by however far the list
        // has scrolled.
        const Offset paint_origin = RenderObject::activePaintOriginOffset();
        const Rect   local_bounds = Rect::fromLTWH(
            offset.x - paint_origin.x, offset.y - paint_origin.y,
            size_.width, size_.height);
        const Rect projected = projectedBounds(ctx.canvas().currentTransform(), local_bounds);
        global_offset_ = { projected.x, projected.y };
        // Feeds FocusManager::moveFocusDirectional()'s D-pad/TV navigation
        // — see FocusNode::bounds()'s doc comment.
        if (registered_node_) registered_node_->bounds_ = projected;
        if (child_) paintChild(ctx, offset);
    }

    void RenderGestureDetector::onTick(uint64_t now_ms)
    {
        long_press_recognizer_->handleTick(now_ms);
    }

} // namespace systems::leal::campello_widgets
