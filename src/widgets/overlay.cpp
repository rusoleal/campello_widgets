#include <campello_widgets/widgets/overlay.hpp>
#include <campello_widgets/widgets/stack.hpp>
#include <campello_widgets/widgets/positioned.hpp>
#include <campello_widgets/widgets/element.hpp>
#include <campello_widgets/widgets/stateful_element.hpp>
#include <campello_widgets/widgets/render_object_element.hpp>
#include <campello_widgets/ui/render_stack.hpp>
#include <campello_widgets/ui/frame_scheduler.hpp>
#include <campello_widgets/ui/thread_checker.hpp>
#include <campello_widgets/ui/view.hpp>

#include <algorithm>
#include <vector>

namespace systems::leal::campello_widgets
{

// ---------------------------------------------------------------------------
// OverlayEntry
// ---------------------------------------------------------------------------



// ---------------------------------------------------------------------------
// OverlayEntryState
// ---------------------------------------------------------------------------

void OverlayEntryState::initState()
{
    State<OverlayEntry>::initState();
    const auto& entry = widget();
    const_cast<OverlayEntry&>(entry).setEntryState(this);
}

void OverlayEntryState::dispose()
{
    const auto& entry = widget();
    const_cast<OverlayEntry&>(entry).setEntryState(nullptr);
    State<OverlayEntry>::dispose();
}

void OverlayEntryState::remove()
{
    if (overlay_) {
        const auto& entry = widget();
        auto shared = const_cast<OverlayEntry&>(entry).shared_from_this();
        overlay_->remove(std::static_pointer_cast<OverlayEntry>(shared));
    }
}

void OverlayEntryState::markNeedsBuild()
{
    // Schedule rebuild via element
    if (auto e = element()) {
        e->markNeedsBuild();
    }
}

WidgetRef OverlayEntryState::build(BuildContext& context)
{
    (void)context;
    const auto& entry = widget();
    if (entry.child) {
        return entry.child;
    }
    return nullptr;
}

// ---------------------------------------------------------------------------
// Overlay static members
// ---------------------------------------------------------------------------

std::atomic<OverlayState*> Overlay::global_state_{nullptr};

namespace {
// Every live OverlayState, in creation order (one window may have several:
// the most recent wins, as for the global one).
std::vector<OverlayState*>& overlayRegistry()
{
    static std::vector<OverlayState*> registry;
    return registry;
}

OverlayState* insertionTarget()
{
    if (auto* state = Overlay::forView(View::current())) return state;
    return Overlay::globalState();
}
} // namespace

OverlayState* Overlay::forView(const View* view) noexcept
{
    if (!view) return nullptr;
    auto& registry = overlayRegistry();
    for (auto it = registry.rbegin(); it != registry.rend(); ++it)
        if ((*it)->view() == view) return *it;
    return nullptr;
}

void Overlay::setGlobalState(OverlayState* state) noexcept
{
    global_state_.store(state, std::memory_order_release);
}

OverlayState* Overlay::globalState() noexcept
{
    return global_state_.load(std::memory_order_acquire);
}

void Overlay::insert(std::shared_ptr<OverlayEntry> entry)
{
    ThreadChecker::instance().assertOnBoundThread("Overlay::insert");
    if (auto* state = insertionTarget()) {
        state->insert(std::move(entry));
    }
}

void Overlay::insertAt(int index, std::shared_ptr<OverlayEntry> entry)
{
    ThreadChecker::instance().assertOnBoundThread("Overlay::insertAt");
    if (auto* state = insertionTarget()) {
        state->insertAt(index, std::move(entry));
    }
}

void Overlay::remove(std::shared_ptr<OverlayEntry> entry)
{
    ThreadChecker::instance().assertOnBoundThread("Overlay::remove");
    for (OverlayState* state : overlayRegistry()) {
        if (std::find(state->entries.begin(), state->entries.end(), entry) != state->entries.end()) {
            state->remove(std::move(entry));
            return;
        }
    }
    if (auto* state = globalState()) {
        state->remove(std::move(entry));
    }
}

// ---------------------------------------------------------------------------
// OverlayState
// ---------------------------------------------------------------------------

void OverlayState::initState()
{
    State<Overlay>::initState();
    
    const auto& overlay = widget();
    entries = overlay.initial_entries;
    
    // Register as global overlay, and as its window's.
    Overlay::setGlobalState(this);
    view_ = View::current();
    overlayRegistry().push_back(this);
}

void OverlayState::dispose()
{
    auto& registry = overlayRegistry();
    registry.erase(std::remove(registry.begin(), registry.end(), this), registry.end());
    // Unregister if we're the global overlay (the previous one, if any, takes over).
    if (Overlay::globalState() == this) {
        Overlay::setGlobalState(registry.empty() ? nullptr : registry.back());
    }
    State<Overlay>::dispose();
}

void OverlayState::insert(std::shared_ptr<OverlayEntry> entry)
{
    if (!entry) return;
    
    entry->setEntryState(nullptr);  // Will be set by initState
    entries.push_back(std::move(entry));
    _markDirty();
}

void OverlayState::insertAt(int index, std::shared_ptr<OverlayEntry> entry)
{
    if (!entry) return;
    if (index < 0) index = 0;
    if (index > static_cast<int>(entries.size())) index = entries.size();
    
    entry->setEntryState(nullptr);
    entries.insert(entries.begin() + index, std::move(entry));
    _markDirty();
}

void OverlayState::remove(std::shared_ptr<OverlayEntry> entry)
{
    if (!entry || removing_) return;
    
    auto it = std::find(entries.begin(), entries.end(), entry);
    if (it != entries.end()) {
        // Erase first, THEN call the callback. This prevents UAF if
        // on_remove() destroys the OverlayState, and prevents infinite
        // recursion if on_remove() calls entry->remove() again.
        entries.erase(it);
        _markDirty();
        if (entry->on_remove) {
            removing_ = true;
            entry->on_remove();
            removing_ = false;
        }
    }
}

void OverlayState::_markDirty()
{
    if (auto e = element()) {
        e->markNeedsBuild();
    }
}

WidgetRef OverlayState::build(BuildContext& context)
{
    (void)context;
    
    // Build list of child widgets from entries
    std::vector<WidgetRef> children;
    children.reserve(entries.size());
    
    for (const auto& entry : entries) {
        if (entry) {
            children.push_back(entry);
        }
    }
    
    // Create stack with all entries
    // Using expand to fill the entire overlay area
    auto stack = std::make_shared<Stack>();
    stack->fit = StackFit::expand;
    stack->children = std::move(children);
    
    return stack;
}

void OverlayState::didUpdateWidget(const Widget& old_widget)
{
    const auto* old_overlay = dynamic_cast<const Overlay*>(&old_widget);
    if (!old_overlay) return;
    
    // Sync entries if initial_entries changed
    // For now, we don't replace existing entries, just ensure state is correct
}

// ---------------------------------------------------------------------------
// OverlayEntry factory
// ---------------------------------------------------------------------------

std::shared_ptr<OverlayEntry> OverlayEntry::create(WidgetRef child)
{
    auto entry = std::make_shared<OverlayEntry>();
    entry->child = std::move(child);
    return entry;
}

} // namespace systems::leal::campello_widgets
