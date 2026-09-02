#pragma once

#include <string>
#include <unordered_map>
#include <vector>
#include <campello_widgets/widgets/single_child_render_object_widget.hpp>
#include <campello_widgets/widgets/element.hpp>
#include <campello_widgets/widgets/navigator.hpp>
#include <campello_widgets/widgets/overlay.hpp>
#include <campello_widgets/ui/animation_controller.hpp>
#include <campello_widgets/ui/value_notifier.hpp>
#include <campello_widgets/ui/rect.hpp>

namespace systems::leal::campello_widgets
{

    /**
     * @brief Marks a widget as a shared-element transition endpoint, matched
     * by `tag` between two routes.
     *
     * `tag` is a plain `std::string` for this stage -- Flutter's real
     * `Hero.tag` is a fully generic `Object`; this codebase already has a
     * `Key`/`ValueKey` hierarchy that could back a more general tag later.
     * Starting with `std::string` is a deliberate simplification (the
     * overwhelmingly common real usage anyway), not an accidental limit.
     *
     * A `SingleChildRenderObjectWidget` (not `StatelessWidget`) wrapping a
     * `RenderHero` -- Stage 5 of the Hero widget initiative. This gives
     * Hero's own Element direct RenderObject ownership
     * (`nearestRenderObjectElement()` returns itself), which
     * `HeroController` needs to reach the captured on-screen rect
     * (`RenderHero::globalRect()`) and to hide/reveal the endpoint
     * (`RenderHero::setHidden()`) during a flight.
     */
    class Hero : public SingleChildRenderObjectWidget
    {
    public:
        std::string tag; // `child` is inherited from SingleChildRenderObjectWidget

        std::shared_ptr<RenderObject> createRenderObject() const override;

        /**
         * @brief Collects every descendant Hero under `root` into a
         * tag -> Element map, via Element::visitAllDescendants() -- mirrors
         * Flutter's real Hero._allHeroesFor(): a fresh walk every call, not
         * a persistent registry, and `root` itself is never checked, only
         * its descendants. A duplicate tag within the same subtree
         * overwrites (last-visited wins) -- unspecified but harmless,
         * matching Flutter's own lack of an explicit dedup guard.
         */
        static std::unordered_map<std::string, Element*> collectHeroesFor(Element* root);
    };

    /**
     * @brief Builds a tag-matched manifest of Hero pairs across a route
     * transition, then runs the flight -- Stage 5 (final) of the Hero widget
     * initiative. Register on Navigator::observers.
     *
     * Rect capture and the flight itself are deferred to a post-frame
     * callback (`PostFrameCallbacks::schedule()`), since
     * `RenderHero::globalRect()` is only valid once the destination route
     * has actually painted -- see `runFlights()`.
     */
    class HeroController : public NavigatorObserver
    {
    public:
        struct FlightManifest
        {
            std::string tag;
            Element*    from_element = nullptr;
            Element*    to_element   = nullptr;
        };

        /**
         * @brief One in-flight shared-element transition, tracked purely for
         * introspection (e.g. tests) -- HeroController doesn't consult this
         * list itself, cleanup is entirely driven by `animation`'s own
         * listener (see runFlights()).
         */
        struct ActiveFlight
        {
            std::string                          tag;
            std::shared_ptr<AnimationController> controller; // shared with the route transition
            std::shared_ptr<OverlayEntry>        entry;
            std::shared_ptr<ValueNotifier<Rect>> notifier;
        };

        void didChangeTop(Route* top_route, std::shared_ptr<AnimationController> animation,
                           Route* previous_top_route) override;

        /** @brief Manifests captured by the most recent didChangeTop() call. */
        const std::vector<FlightManifest>& manifests() const noexcept { return manifests_; }

        /** @brief Flights currently in progress (for introspection/tests). */
        const std::vector<ActiveFlight>& activeFlights() const noexcept { return active_flights_; }

    private:
        /**
         * @brief Captures both endpoints' rects and starts a flight for each
         * manifest entry -- called via a post-frame callback scheduled from
         * didChangeTop(), once the destination route has actually painted.
         */
        void runFlights(std::shared_ptr<AnimationController> animation);

        std::vector<FlightManifest> manifests_;
        std::vector<ActiveFlight>   active_flights_;
    };

} // namespace systems::leal::campello_widgets
