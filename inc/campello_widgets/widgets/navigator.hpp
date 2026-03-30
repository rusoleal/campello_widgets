#pragma once

#include <campello_widgets/widgets/stateful_widget.hpp>
#include <campello_widgets/widgets/inherited_widget.hpp>
#include <campello_widgets/ui/animation_controller.hpp>
#include <campello_widgets/ui/tween.hpp>
#include <campello_widgets/ui/offset.hpp>

#include <functional>
#include <memory>
#include <vector>

namespace systems::leal::campello_widgets
{

class BuildContext;
class NavigatorState;

// ==========================================================================
// Route  — abstract base for a single screen / dialog
// ==========================================================================

/**
 * @brief Abstract base class for an entry in the Navigator's route stack.
 *
 * Subclass and override `build()` to define the screen's widget tree.
 * The simplest concrete subclass is `PageRoute`.
 */
class Route
{
public:
    virtual ~Route() = default;

    /** @brief Returns the widget tree for this screen. */
    virtual WidgetRef build(BuildContext& ctx) = 0;

    /**
     * @brief If true, routes below this one in the stack are not built.
     *
     * Opaque routes cover the entire viewport, so there is no need to build
     * routes underneath them. Transparent routes (dialogs, bottom sheets)
     * should return false.
     */
    virtual bool opaque() const { return true; }
};

// ==========================================================================
// PageRoute  — full-screen route with a slide-from-right transition
// ==========================================================================

/**
 * @brief A full-screen route built from a builder function.
 *
 * Slides in from the right edge when pushed and slides back out when popped.
 *
 * @code
 * Navigator::of(ctx)->push(std::make_shared<PageRoute>([](BuildContext& ctx) {
 *     return std::make_shared<MyScreen>();
 * }));
 * @endcode
 */
class PageRoute : public Route
{
public:
    explicit PageRoute(std::function<WidgetRef(BuildContext&)> builder)
        : builder_(std::move(builder)) {}

    WidgetRef build(BuildContext& ctx) override { return builder_(ctx); }

private:
    std::function<WidgetRef(BuildContext&)> builder_;
};

// ==========================================================================
// NavigatorScope  — InheritedWidget that exposes NavigatorState*
// ==========================================================================

/**
 * @brief InheritedWidget placed at the root of the Navigator's subtree.
 *
 * Allows any descendant to reach the nearest NavigatorState via
 * `Navigator::of(context)`.
 */
class NavigatorScope : public InheritedWidget
{
public:
    NavigatorState* state = nullptr;

    bool updateShouldNotify(const InheritedWidget& old) const override
    {
        return static_cast<const NavigatorScope&>(old).state != state;
    }
};

// ==========================================================================
// Navigator
// ==========================================================================

/**
 * @brief A widget that manages a stack of routes (screens).
 *
 * Place a Navigator near the root of your widget tree (typically inside
 * your top-level StatefulWidget) and supply an `initial_route`. Push new
 * routes with `Navigator::of(ctx)->push(...)` from any descendant context.
 *
 * @code
 * auto nav = std::make_shared<Navigator>();
 * nav->initial_route = std::make_shared<PageRoute>([](BuildContext& ctx) {
 *     return std::make_shared<HomeScreen>();
 * });
 * @endcode
 */
class Navigator : public StatefulWidget
{
public:
    /** @brief The first route shown when the Navigator is inserted into the tree. */
    std::shared_ptr<Route> initial_route;

    std::unique_ptr<StateBase> createState() const override;

    /**
     * @brief Returns the nearest NavigatorState ancestor.
     *
     * The calling element registers as a dependent of the NavigatorScope;
     * it will be rebuilt whenever the Navigator rebuilds (e.g. after a push/pop).
     *
     * Returns nullptr if no Navigator is present in the tree.
     */
    static NavigatorState* of(BuildContext& ctx);
};

// ==========================================================================
// NavigatorState
// ==========================================================================

/**
 * @brief Mutable state for Navigator.
 *
 * Obtain via `Navigator::of(context)`. All methods trigger a rebuild of the
 * Navigator's subtree, which initiates slide-in / slide-out transitions.
 */
class NavigatorState : public State<Navigator>
{
public:
    void initState() override;
    void dispose()   override;

    // ------------------------------------------------------------------
    // Navigation controls
    // ------------------------------------------------------------------

    /**
     * @brief Push `route` onto the stack.
     *
     * The new route slides in from the right (300 ms, ease-in-out).
     */
    void push(std::shared_ptr<Route> route);

    /**
     * @brief Pop the top route from the stack.
     *
     * The current top route slides out to the right. No-op if only one
     * route remains (popping the last route would leave the stack empty).
     */
    void pop();

    /**
     * @brief Replace the top route with `route` (no pop animation).
     *
     * Equivalent to an immediate pop followed by a push.
     */
    void pushReplacement(std::shared_ptr<Route> route);

    /** @brief True if more than one route is in the stack. */
    bool canPop() const { return stack_.size() > 1; }

    WidgetRef build(BuildContext& ctx) override;

private:
    struct RouteEntry
    {
        std::shared_ptr<Route>               route;
        std::shared_ptr<AnimationController> animation; ///< null for the initial route
        uint64_t                             listener_id = 0;
        bool                                 popping     = false;
    };

    std::vector<RouteEntry> stack_;

    /// Create a 300 ms animation controller wired to setState (for pushed routes).
    RouteEntry makeEntry(std::shared_ptr<Route> route, bool snap_to_end = false);
};

} // namespace systems::leal::campello_widgets
