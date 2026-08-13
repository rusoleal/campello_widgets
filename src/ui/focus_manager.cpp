#include <algorithm>
#include <campello_widgets/ui/focus_manager.hpp>
#include <campello_widgets/ui/focus_node.hpp>
#include <campello_widgets/ui/thread_checker.hpp>

namespace systems::leal::campello_widgets
{

    std::atomic<FocusManager*> FocusManager::s_active_manager_{nullptr};
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

} // namespace systems::leal::campello_widgets
