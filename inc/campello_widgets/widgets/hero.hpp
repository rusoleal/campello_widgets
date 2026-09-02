#pragma once

#include <string>
#include <unordered_map>
#include <vector>
#include <campello_widgets/widgets/stateless_widget.hpp>
#include <campello_widgets/widgets/element.hpp>
#include <campello_widgets/widgets/navigator.hpp>

namespace systems::leal::campello_widgets
{

    /**
     * @brief Marks a widget as a shared-element transition endpoint, matched
     * by `tag` between two routes -- Stage 2 of the Hero widget initiative
     * (see the Hero Widget Scoping artifact). Inert at this stage: a pure
     * passthrough, no transition logic, no RenderObject of its own.
     *
     * `tag` is a plain `std::string` for this stage -- Flutter's real
     * `Hero.tag` is a fully generic `Object`; this codebase already has a
     * `Key`/`ValueKey` hierarchy that could back a more general tag later.
     * Starting with `std::string` is a deliberate simplification (the
     * overwhelmingly common real usage anyway), not an accidental limit.
     */
    class Hero : public StatelessWidget
    {
    public:
        std::string tag;
        WidgetRef   child;

        WidgetRef build(BuildContext&) const override { return child; }

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
     * transition -- Stage 4 of the Hero widget initiative. Register on
     * Navigator::observers.
     *
     * Rect capture is deferred to Stage 5 (needs a post-frame-callback
     * mechanism this codebase doesn't have yet): this stage only identifies
     * which Hero Elements match up, not where they are on screen.
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

        void didChangeTop(Route* top_route, std::shared_ptr<AnimationController> animation,
                           Route* previous_top_route) override;

        /** @brief Manifests captured by the most recent didChangeTop() call -- no rects yet, see Stage 5. */
        const std::vector<FlightManifest>& manifests() const noexcept { return manifests_; }

    private:
        std::vector<FlightManifest> manifests_;
    };

} // namespace systems::leal::campello_widgets
