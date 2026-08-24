#include <algorithm>
#include <cmath>
#include <campello_widgets/ui/focus_manager.hpp>
#include <campello_widgets/ui/focus_node.hpp>
#include <campello_widgets/ui/thread_checker.hpp>

namespace systems::leal::campello_widgets
{

    namespace
    {
        bool directionForKeyCode(KeyCode key_code, FocusDirection& out) noexcept
        {
            switch (key_code)
            {
                case KeyCode::left:  out = FocusDirection::left;  return true;
                case KeyCode::right: out = FocusDirection::right; return true;
                case KeyCode::up:    out = FocusDirection::up;    return true;
                case KeyCode::down:  out = FocusDirection::down;  return true;
                default: return false;
            }
        }
    }

    std::atomic<FocusManager*> FocusManager::s_active_manager_{nullptr};
    std::atomic<FocusHighlightMode> FocusManager::s_highlight_mode_{FocusHighlightMode::touch};
    std::mutex                           FocusManager::s_global_handler_mutex_;
    std::function<bool(const KeyEvent&)> FocusManager::s_global_key_handler_;

    void FocusManager::setActiveManager(FocusManager* manager) noexcept
    {
        s_active_manager_.store(manager, std::memory_order_release);
    }

    FocusManager* FocusManager::activeManager() noexcept
    {
        return s_active_manager_.load(std::memory_order_acquire);
    }

    void FocusManager::noteKeyboardInteraction() noexcept
    {
        s_highlight_mode_.store(FocusHighlightMode::keyboard, std::memory_order_relaxed);
    }

    void FocusManager::notePointerInteraction() noexcept
    {
        s_highlight_mode_.store(FocusHighlightMode::touch, std::memory_order_relaxed);
    }

    FocusHighlightMode FocusManager::highlightMode() noexcept
    {
        return s_highlight_mode_.load(std::memory_order_relaxed);
    }

    void FocusManager::setGlobalKeyHandler(std::function<bool(const KeyEvent&)> handler)
    {
        std::lock_guard<std::mutex> lock(s_global_handler_mutex_);
        s_global_key_handler_ = std::move(handler);
    }

    std::function<bool(const KeyEvent&)> FocusManager::globalKeyHandler()
    {
        std::lock_guard<std::mutex> lock(s_global_handler_mutex_);
        return s_global_key_handler_;
    }

    // -------------------------------------------------------------------------

    void FocusManager::registerNode(FocusNode* node)
    {
        if (!node) return;
        focus_order_.push_back(node);
    }

    void FocusManager::unregisterNode(FocusNode* node)
    {
        if (!node) return;

        if (current_focus_ == node)
        {
            node->focused_ = false;
            if (node->on_focus_changed) node->on_focus_changed(false);
            current_focus_ = nullptr;
        }

        focus_order_.erase(
            std::remove(focus_order_.begin(), focus_order_.end(), node),
            focus_order_.end());
    }

    void FocusManager::requestFocus(FocusNode* node)
    {
        if (!node || node == current_focus_) return;

        // Unfocus current.
        if (current_focus_)
        {
            current_focus_->focused_ = false;
            if (current_focus_->on_focus_changed)
                current_focus_->on_focus_changed(false);
        }

        current_focus_ = node;
        node->focused_ = true;
        if (node->on_focus_changed) node->on_focus_changed(true);
    }

    void FocusManager::unfocus(FocusNode* node)
    {
        if (!node || node != current_focus_) return;

        node->focused_ = false;
        if (node->on_focus_changed) node->on_focus_changed(false);
        current_focus_ = nullptr;
    }

    // -------------------------------------------------------------------------

    void FocusManager::handleKeyEvent(const KeyEvent& event)
    {
        ThreadChecker::instance().assertOnBoundThread("FocusManager::handleKeyEvent");

        // Any key event reaching here implies the user is currently
        // interacting via keyboard -- switches focus-ring visibility on
        // for whatever has (or is about to gain) focus. See
        // FocusHighlightMode's doc comment.
        noteKeyboardInteraction();

        // App-level shortcuts run first and may consume the event outright
        // regardless of what currently has focus. Copy the handler out
        // under the lock rather than holding it while invoking — the
        // handler itself may synchronously do widget-tree work
        // (setState, etc.) that shouldn't run under this mutex.
        std::function<bool(const KeyEvent&)> global_handler;
        {
            std::lock_guard<std::mutex> lock(s_global_handler_mutex_);
            global_handler = s_global_key_handler_;
        }
        if (global_handler && global_handler(event)) return;

        // Intercept Tab / Shift+Tab for traversal on key-down only.
        if (event.kind != KeyEventKind::up && event.key_code == KeyCode::tab)
        {
            if (event.modifiers & KeyModifiers::shift)
                moveFocusBackward();
            else
                moveFocusForward();
            return;
        }

        // Arrow keys: unlike Tab, these are routinely meaningful to the
        // focused control itself (TextField cursor movement, Slider value
        // adjustment, ...), so give it first chance to consume the event.
        // Only an unconsumed key-down/repeat falls back to directional
        // (D-pad/TV) focus movement -- a key-up never triggers a focus
        // move (nothing to reasonably repeat/undo), it's just passed
        // through in case a consumer cares about it.
        FocusDirection direction;
        if (directionForKeyCode(event.key_code, direction))
        {
            if (current_focus_ && current_focus_->on_key && current_focus_->on_key(event))
                return;

            if (event.kind != KeyEventKind::up)
                moveFocusDirectional(direction);
            return;
        }

        // Route to focused node.
        if (current_focus_ && current_focus_->on_key)
            current_focus_->on_key(event);
    }

    void FocusManager::moveFocusForward()
    {
        if (focus_order_.empty()) return;

        if (!current_focus_)
        {
            requestFocus(focus_order_.front());
            return;
        }

        auto it = std::find(focus_order_.begin(), focus_order_.end(), current_focus_);
        if (it == focus_order_.end())
        {
            requestFocus(focus_order_.front());
            return;
        }

        ++it;
        if (it == focus_order_.end())
            it = focus_order_.begin();

        requestFocus(*it);
    }

    void FocusManager::moveFocusBackward()
    {
        if (focus_order_.empty()) return;

        if (!current_focus_)
        {
            requestFocus(focus_order_.back());
            return;
        }

        auto it = std::find(focus_order_.begin(), focus_order_.end(), current_focus_);
        if (it == focus_order_.end() || it == focus_order_.begin())
        {
            requestFocus(focus_order_.back());
            return;
        }

        requestFocus(*std::prev(it));
    }

    void FocusManager::moveFocusDirectional(FocusDirection direction)
    {
        if (focus_order_.empty()) return;

        if (!current_focus_)
        {
            requestFocus(focus_order_.front());
            return;
        }

        const Rect src = current_focus_->bounds();
        const float src_cx = src.x + src.width  * 0.5f;
        const float src_cy = src.y + src.height * 0.5f;

        // Cross-axis misalignment is penalized more heavily than distance
        // along the travel axis, so e.g. moving right prefers a same-row
        // candidate a bit further away over a closer one that's off-row
        // (mirrors how Android's/Compose's directional focus search
        // weights candidates).
        constexpr float kCrossAxisWeight = 3.0f;

        FocusNode* best = nullptr;
        float      best_score = 0.0f;

        for (FocusNode* candidate : focus_order_)
        {
            if (candidate == current_focus_) continue;

            const Rect r = candidate->bounds();
            if (r.width <= 0.0f || r.height <= 0.0f) continue; // unpainted

            const float cx = r.x + r.width  * 0.5f;
            const float cy = r.y + r.height * 0.5f;

            float along  = 0.0f; // distance along the travel axis (must be > 0)
            float cross  = 0.0f; // misalignment on the perpendicular axis
            switch (direction)
            {
                case FocusDirection::right: along = cx - src_cx; cross = cy - src_cy; break;
                case FocusDirection::left:  along = src_cx - cx; cross = cy - src_cy; break;
                case FocusDirection::down:  along = cy - src_cy; cross = cx - src_cx; break;
                case FocusDirection::up:    along = src_cy - cy; cross = cx - src_cx; break;
            }
            if (along <= 0.0f) continue; // not in the requested direction

            const float score = along + kCrossAxisWeight * std::abs(cross);
            if (!best || score < best_score)
            {
                best = candidate;
                best_score = score;
            }
        }

        if (best) requestFocus(best);
    }

} // namespace systems::leal::campello_widgets
