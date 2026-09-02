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

} // namespace systems::leal::campello_widgets
