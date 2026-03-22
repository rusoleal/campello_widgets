#include <algorithm>
#include <campello_widgets/ui/focus_manager.hpp>
#include <campello_widgets/ui/focus_node.hpp>

namespace systems::leal::campello_widgets
{

    FocusManager* FocusManager::s_active_manager_ = nullptr;

    void FocusManager::setActiveManager(FocusManager* manager) noexcept
    {
        s_active_manager_ = manager;
    }

    FocusManager* FocusManager::activeManager() noexcept
    {
        return s_active_manager_;
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
