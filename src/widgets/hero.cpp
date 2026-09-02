#include <campello_widgets/widgets/hero.hpp>

namespace systems::leal::campello_widgets
{

    std::unordered_map<std::string, Element*> Hero::collectHeroesFor(Element* root)
    {
        std::unordered_map<std::string, Element*> result;
        Element::visitAllDescendants(root, [&](Element* e)
        {
            if (auto* hero = dynamic_cast<const Hero*>(&e->widget()))
                result[hero->tag] = e;
        });
        return result;
    }

    void HeroController::didChangeTop(
        Route* top_route, std::shared_ptr<AnimationController> /*animation*/, Route* previous_top_route)
    {
        manifests_.clear();
        if (!navigator() || !top_route || !previous_top_route) return;

        Element* to_layer   = navigator()->elementForRoute(top_route);
        Element* from_layer = navigator()->elementForRoute(previous_top_route);
        if (!to_layer || !from_layer) return;

        auto from_heroes = Hero::collectHeroesFor(from_layer);
        auto to_heroes   = Hero::collectHeroesFor(to_layer);

        for (auto& [tag, from_element] : from_heroes)
        {
            auto it = to_heroes.find(tag);
            if (it == to_heroes.end()) continue;
            manifests_.push_back(FlightManifest{tag, from_element, it->second});
        }
    }

} // namespace systems::leal::campello_widgets
