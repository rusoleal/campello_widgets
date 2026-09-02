#pragma once

#include <string>
#include <unordered_map>
#include <campello_widgets/widgets/stateless_widget.hpp>
#include <campello_widgets/widgets/element.hpp>

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

} // namespace systems::leal::campello_widgets
