#include <algorithm>
#include <cmath>
#include <unordered_map>
#include <campello_widgets/ui/focus_manager.hpp>
#include <campello_widgets/ui/focus_node.hpp>
#include <campello_widgets/ui/focus_traversal_policy.hpp>
#include <campello_widgets/ui/hardware_keyboard.hpp>
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

        // Implementation detail of FocusManager::sortWithGroups() -- see
        // that method's own doc comment (focus_manager.hpp) for what this
        // represents. `group` is nullptr for the scope's own top-level
        // bucket; `items` holds leaves and/or child-group marker
        // FocusNode*s in first-occurrence order, with `children` mapping
        // each such marker to its own nested bucket.
        struct GroupBucket
        {
            FocusNode* group = nullptr;
            std::vector<FocusNode*> items;
            std::unordered_map<FocusNode*, GroupBucket> children;
        };
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
        // Captures whatever held focus right before this scope opened, so
        // unregisterNode() can restore it when the scope later closes --
        // see FocusNode::previouslyFocusedOutside()'s doc comment. Must
        // happen here, before this node's owner can possibly steal focus
        // (e.g. via auto_focus a moment later in the same mount pass).
        if (node->isScope())
            node->setPreviouslyFocusedOutside(current_focus_);
        focus_order_.push_back(node);
    }

    void FocusManager::unregisterNode(FocusNode* node)
    {
        if (!node) return;

        // Captured before the "clear current_focus_ if it was me" block
        // below, since a scope can legitimately BE current_focus_ itself
        // (e.g. auto_focus with no focusable descendant yet) --
        // nearestEnclosingScope() checks `node` itself first, so this
        // covers that case too. Safe to walk node->parent() here (not
        // stale): a scope's own destructor -- and thus this call --
        // always runs before any focused descendant's, since the whole
        // removed subtree's RenderObject graph stays fully linked until a
        // single shared_ptr release at the Stack boundary triggers a
        // normal top-down member-destructor cascade (verified directly
        // against Element::unmount()/RenderObjectElement::unmount()/
        // RenderStack::clearChildren() -- Element-level unmount is
        // bottom-up but never touches the RenderObject parent/child
        // links themselves).
        const bool wasFocusWithinThisScope =
            node->isScope() && (nearestEnclosingScope(current_focus_) == node);

        if (current_focus_ == node)
        {
            node->focused_ = false;
            if (node->on_focus_changed) node->on_focus_changed(false);
            current_focus_ = nullptr;
        }

        if (wasFocusWithinThisScope)
            requestFocus(node->previouslyFocusedOutside());

        // Dangling-ref hygiene: any other scope remembering `node` as its
        // pre-open focus target would otherwise try to restore focus into
        // an already-destroyed node once it later closes.
        for (FocusNode* n : focus_order_)
        {
            if (n->isScope() && n->previouslyFocusedOutside() == node)
                n->setPreviouslyFocusedOutside(nullptr);
        }

        focus_order_.erase(
            std::remove(focus_order_.begin(), focus_order_.end(), node),
            focus_order_.end());
    }

    void FocusManager::requestFocus(FocusNode* node)
    {
        if (!node || !node->canRequestFocus() || node == current_focus_) return;

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

        // Keep HardwareKeyboard::current() live for every branch below
        // (including early returns) -- see its doc comment. Platform
        // adapters may also update it directly for modifier transitions
        // that never reach here as a KeyEvent at all (e.g. macOS's
        // flagsChanged: for a bare Cmd/Shift/Ctrl/Option press).
        HardwareKeyboard::current().updateModifiers(event.modifiers);

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

        // Tab / Shift+Tab: same "give the focused node first chance" shape
        // as the arrow-key case below -- a plain control has no use for
        // Tab and falls through to traversal (the common case, e.g. every
        // TextField), but a control that legitimately wants Tab itself
        // (e.g. RichTextField inserting an indent, matching every real
        // code editor) can consume it and suppress traversal. Key-down
        // only, same as before. Tried against the whole focus chain (see
        // dispatchToFocusChain()), not just the leaf, so an ancestor
        // wrapper gets a chance too if the focused control itself doesn't
        // want it.
        if (event.kind != KeyEventKind::up && event.key_code == KeyCode::tab)
        {
            if (dispatchToFocusChain(current_focus_, event)) return;

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
            if (dispatchToFocusChain(current_focus_, event)) return;

            if (event.kind != KeyEventKind::up)
                moveFocusDirectional(direction);
            return;
        }

        // Route to the focused node, bubbling up through ancestor
        // FocusNodes if unconsumed (see dispatchToFocusChain()) -- e.g. so
        // a KeyboardListener wrapping a focused TextField still sees a key
        // that TextField's own on_key didn't handle.
        dispatchToFocusChain(current_focus_, event);
    }

    bool FocusManager::dispatchToFocusChain(FocusNode* node, const KeyEvent& event)
    {
        for (; node; node = node->parent())
        {
            if (node->on_key && node->on_key(event)) return true;
        }
        return false;
    }

    FocusNode* FocusManager::nearestEnclosingScope(FocusNode* node) noexcept
    {
        for (; node; node = node->parent())
        {
            if (node->isScope()) return node;
        }
        return nullptr;
    }

    std::vector<FocusNode*> FocusManager::traversalCandidates() const
    {
        FocusNode* scope = nearestEnclosingScope(current_focus_);
        std::vector<FocusNode*> result;
        result.reserve(focus_order_.size());
        for (FocusNode* n : focus_order_)
        {
            if (n->isScope()) continue; // a scope is a container, never itself a stop
            if (!n->canRequestFocus()) continue;
            if (n->skipTraversal()) continue;
            if (nearestEnclosingScope(n) != scope) continue;
            result.push_back(n);
        }
        return result;
    }

    // Buckets `candidates` (already scope-filtered leaves) by their nearest
    // enclosing FocusTraversalGroup ancestor between them and `scope`, sorts
    // each bucket with that group's own traversal_policy (or `fallback` when
    // the group has none), then flattens depth-first so each group's members
    // stay contiguous -- ports Flutter's real _findGroups/_sortAllDescendants
    // shape (focus_traversal.dart), simplified to operate on the already-
    // filtered leaf list this codebase's traversalCandidates() produces
    // rather than a raw, unfiltered descendant walk. Group marker nodes
    // themselves never appear in `candidates` (traversalCandidates() already
    // excludes them via the existing canRequestFocus()/skipTraversal()
    // filters, since FocusTraversalGroup sets both false/true on its own
    // node -- see focus_traversal_group.hpp), so every bucket's `items` is
    // built purely from the walk below, never a special-cased self-entry.
    //
    // Collapses to exactly `fallback->order(candidates)` whenever no
    // candidate has a FocusTraversalGroup ancestor: every candidate's chain
    // is empty, so all of them land in the single root bucket, sorted once
    // with `fallback` -- byte-identical to this method's pre-existing
    // behavior for every Tab flow that predates FocusTraversalGroup.
    std::vector<FocusNode*> FocusManager::sortWithGroups(
        const std::vector<FocusNode*>& candidates, FocusNode* scope, FocusTraversalPolicy* fallback) const
    {
        GroupBucket root;
        for (FocusNode* c : candidates)
        {
            std::vector<FocusNode*> chain;
            for (FocusNode* p = c->parent(); p && p != scope; p = p->parent())
                if (p->isTraversalGroup()) chain.push_back(p);
            std::reverse(chain.begin(), chain.end());

            GroupBucket* cur = &root;
            for (FocusNode* g : chain)
            {
                auto it = cur->children.find(g);
                if (it == cur->children.end())
                {
                    cur->items.push_back(g);
                    it = cur->children.emplace(g, GroupBucket{g, {}, {}}).first;
                }
                cur = &it->second;
            }
            cur->items.push_back(c);
        }

        std::function<std::vector<FocusNode*>(GroupBucket&, FocusTraversalPolicy*)> flatten =
            [&](GroupBucket& bucket, FocusTraversalPolicy* inherited) -> std::vector<FocusNode*>
        {
            FocusTraversalPolicy* policy = (bucket.group && bucket.group->traversal_policy)
                ? bucket.group->traversal_policy.get()
                : inherited;
            std::vector<FocusNode*> result;
            for (FocusNode* item : policy->order(bucket.items))
            {
                auto it = bucket.children.find(item);
                if (it != bucket.children.end())
                {
                    std::vector<FocusNode*> sub = flatten(it->second, policy);
                    result.insert(result.end(), sub.begin(), sub.end());
                }
                else
                {
                    result.push_back(item);
                }
            }
            return result;
        };
        return flatten(root, fallback);
    }

    void FocusManager::moveFocusForward()
    {
        std::vector<FocusNode*> candidates = traversalCandidates();
        if (candidates.empty()) return;

        FocusNode* scope = nearestEnclosingScope(current_focus_);
        FocusTraversalPolicy* policy = (scope && scope->traversal_policy)
            ? scope->traversal_policy.get()
            : nullptr;
        if (!policy)
        {
            if (!default_traversal_policy_)
                default_traversal_policy_ = std::make_shared<OrderedTraversalPolicy>();
            policy = default_traversal_policy_.get();
        }
        std::vector<FocusNode*> ordered = sortWithGroups(candidates, scope, policy);

        if (!current_focus_)
        {
            requestFocus(ordered.front());
            return;
        }

        auto it = std::find(ordered.begin(), ordered.end(), current_focus_);
        if (it == ordered.end())
        {
            requestFocus(ordered.front());
            return;
        }

        ++it;
        if (it == ordered.end())
            it = ordered.begin();

        requestFocus(*it);
    }

    void FocusManager::moveFocusBackward()
    {
        std::vector<FocusNode*> candidates = traversalCandidates();
        if (candidates.empty()) return;

        FocusNode* scope = nearestEnclosingScope(current_focus_);
        FocusTraversalPolicy* policy = (scope && scope->traversal_policy)
            ? scope->traversal_policy.get()
            : nullptr;
        if (!policy)
        {
            if (!default_traversal_policy_)
                default_traversal_policy_ = std::make_shared<OrderedTraversalPolicy>();
            policy = default_traversal_policy_.get();
        }
        std::vector<FocusNode*> ordered = sortWithGroups(candidates, scope, policy);

        if (!current_focus_)
        {
            requestFocus(ordered.back());
            return;
        }

        auto it = std::find(ordered.begin(), ordered.end(), current_focus_);
        if (it == ordered.end() || it == ordered.begin())
        {
            requestFocus(ordered.back());
            return;
        }

        requestFocus(*std::prev(it));
    }

    void FocusManager::moveFocusDirectional(FocusDirection direction)
    {
        std::vector<FocusNode*> candidates = traversalCandidates();
        if (candidates.empty()) return;

        if (!current_focus_)
        {
            requestFocus(candidates.front());
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

        for (FocusNode* candidate : candidates)
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
