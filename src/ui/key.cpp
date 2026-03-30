#include <campello_widgets/ui/key.hpp>

namespace systems::leal::campello_widgets
{

    // Static registry: GlobalKey* → Element*
    std::unordered_map<GlobalKey*, Element*> GlobalKey::s_registry_;

    GlobalKey::~GlobalKey()
    {
        s_registry_.erase(this);
    }

    Element* GlobalKey::currentElement() const noexcept
    {
        auto it = s_registry_.find(const_cast<GlobalKey*>(this));
        return it != s_registry_.end() ? it->second : nullptr;
    }

    void GlobalKey::_register(GlobalKey* key, Element* element) noexcept
    {
        if (key) s_registry_[key] = element;
    }

    void GlobalKey::_unregister(GlobalKey* key) noexcept
    {
        if (key) s_registry_.erase(key);
    }

} // namespace systems::leal::campello_widgets
