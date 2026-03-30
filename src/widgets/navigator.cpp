#include <campello_widgets/widgets/navigator.hpp>
#include <campello_widgets/widgets/stack.hpp>
#include <campello_widgets/widgets/slide_transition.hpp>
#include <campello_widgets/ui/stack_fit.hpp>
#include <campello_widgets/ui/key.hpp>
#include <campello_widgets/ui/curves.hpp>

#include <algorithm>

namespace systems::leal::campello_widgets
{

// --------------------------------------------------------------------------
// Navigator
// --------------------------------------------------------------------------

std::unique_ptr<StateBase> Navigator::createState() const
{
    return std::make_unique<NavigatorState>();
}

NavigatorState* Navigator::of(BuildContext& ctx)
{
    const auto* scope = ctx.dependOnInheritedWidgetOfExactType<NavigatorScope>();
    return scope ? scope->state : nullptr;
}

// --------------------------------------------------------------------------
// NavigatorState — helpers
// --------------------------------------------------------------------------

NavigatorState::RouteEntry NavigatorState::makeEntry(
    std::shared_ptr<Route> route, bool snap_to_end)
{
    auto ctrl = std::make_shared<AnimationController>(300.0);

    // Wire animation ticks to setState so the slide repaints each frame.
    const uint64_t id = ctrl->addListener([this]() { setState([]{}); });

    if (snap_to_end)
    {
        // Initial route: skip the transition (jump to fully visible).
        ctrl->forward(1.0);
    }

    return RouteEntry{std::move(route), std::move(ctrl), id, false};
}

// --------------------------------------------------------------------------
// NavigatorState — lifecycle
// --------------------------------------------------------------------------

void NavigatorState::initState()
{
    if (auto r = widget().initial_route)
        stack_.push_back(makeEntry(std::move(r), /*snap_to_end=*/true));
}

void NavigatorState::dispose()
{
    // Remove all animation listeners to avoid dangling callbacks.
    for (auto& entry : stack_)
    {
        if (entry.animation && entry.listener_id)
            entry.animation->removeListener(entry.listener_id);
    }
    stack_.clear();
}

// --------------------------------------------------------------------------
// NavigatorState — navigation controls
// --------------------------------------------------------------------------

void NavigatorState::push(std::shared_ptr<Route> route)
{
    auto entry = makeEntry(std::move(route));
    entry.animation->forward(0.0); // animate from off-screen right to visible
    stack_.push_back(std::move(entry));
    setState([]{});
}

void NavigatorState::pop()
{
    if (stack_.size() <= 1) return;

    auto& top   = stack_.back();
    top.popping = true;
    top.animation->reverse(); // slide back to the right
}

void NavigatorState::pushReplacement(std::shared_ptr<Route> route)
{
    // Remove the current top immediately (no exit animation).
    if (!stack_.empty())
    {
        auto& top = stack_.back();
        if (top.animation && top.listener_id)
            top.animation->removeListener(top.listener_id);
        stack_.pop_back();
    }

    auto entry = makeEntry(std::move(route));
    entry.animation->forward(0.0);
    stack_.push_back(std::move(entry));
    setState([]{});
}

// --------------------------------------------------------------------------
// NavigatorState — build
// --------------------------------------------------------------------------

WidgetRef NavigatorState::build(BuildContext& ctx)
{
    // Prune entries whose pop animation has fully reversed.
    stack_.erase(
        std::remove_if(stack_.begin(), stack_.end(),
            [](const RouteEntry& e) {
                return e.popping &&
                       e.animation &&
                       e.animation->status() == AnimationStatus::dismissed;
            }),
        stack_.end());

    // Find the bottom-most visible route: the topmost opaque entry below the
    // current top determines how far down we need to build.
    int bottom = 0;
    for (int i = static_cast<int>(stack_.size()) - 1; i >= 0; --i)
    {
        if (stack_[i].route->opaque()) { bottom = i; break; }
    }

    // Build the visible layer stack from bottom to top.
    std::vector<WidgetRef> layers;
    layers.reserve(static_cast<std::size_t>(static_cast<int>(stack_.size()) - bottom));

    for (int i = bottom; i < static_cast<int>(stack_.size()); ++i)
    {
        auto&     entry   = stack_[i];
        WidgetRef content = entry.route->build(ctx);

        // Wrap each entry in a SlideTransition (right → center on push,
        // center → right on pop).  The ObjectKey stabilises the element
        // across rebuilds so the render object is reused during animation.
        auto slide        = std::make_shared<SlideTransition>();
        slide->key        = std::make_shared<ObjectKey>(entry.route.get());
        slide->controller = entry.animation;
        slide->offset     = {{1.0f, 0.0f}, {0.0f, 0.0f}};
        slide->curve      = Curves::easeInOut;
        slide->child      = std::move(content);

        layers.push_back(std::move(slide));
    }

    // Stack layers so the top route is drawn last (on top).
    auto stack_widget = Stack::create(StackFit::expand, std::move(layers));

    // Expose NavigatorState* to descendants via NavigatorScope.
    auto scope   = std::make_shared<NavigatorScope>();
    scope->state = this;
    scope->child = std::move(stack_widget);
    return scope;
}

} // namespace systems::leal::campello_widgets
