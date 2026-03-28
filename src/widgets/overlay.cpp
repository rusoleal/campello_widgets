#include <campello_widgets/widgets/overlay.hpp>
#include <campello_widgets/widgets/stack.hpp>
#include <campello_widgets/widgets/positioned.hpp>
#include <campello_widgets/widgets/element.hpp>
#include <campello_widgets/widgets/stateful_element.hpp>
#include <campello_widgets/widgets/render_object_element.hpp>
#include <campello_widgets/ui/render_stack.hpp>

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

OverlayState* Overlay::global_state_ = nullptr;

void Overlay::setGlobalState(OverlayState* state) noexcept
{
    global_state_ = state;
}

OverlayState* Overlay::globalState() noexcept
{
    return global_state_;
}

void Overlay::insert(std::shared_ptr<OverlayEntry> entry)
{
    if (global_state_) {
        global_state_->insert(std::move(entry));
    }
}

void Overlay::insertAt(int index, std::shared_ptr<OverlayEntry> entry)
{
    if (global_state_) {
        global_state_->insertAt(index, std::move(entry));
    }
}

void Overlay::remove(std::shared_ptr<OverlayEntry> entry)
{
    if (global_state_) {
        global_state_->remove(std::move(entry));
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
    
    // Register as global overlay
    Overlay::setGlobalState(this);
}

void OverlayState::dispose()
{
    // Unregister if we're the global overlay
    if (Overlay::globalState() == this) {
        Overlay::setGlobalState(nullptr);
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
    if (!entry) return;
    
    auto it = std::find(entries.begin(), entries.end(), entry);
    if (it != entries.end()) {
        // Call on_remove callback if set
        if (entry->on_remove) {
            entry->on_remove();
        }
        entries.erase(it);
        _markDirty();
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
