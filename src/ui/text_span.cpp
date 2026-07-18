#include <campello_widgets/ui/text_span.hpp>

namespace systems::leal::campello_widgets
{

    size_t TextSpanHash::operator()(const TextSpan& s) const noexcept
    {
        size_t h = std::hash<std::string>{}(s.text);
        auto mix = [&h](size_t v) { h ^= v + 0x9e3779b97f4a7c15ULL + (h << 6) + (h >> 2); };
        mix(std::hash<std::string>{}(s.style.font_family));
        mix(std::hash<float>{}(s.style.font_size));
        mix(std::hash<float>{}(s.style.color.r));
        mix(std::hash<float>{}(s.style.color.g));
        mix(std::hash<float>{}(s.style.color.b));
        mix(std::hash<float>{}(s.style.color.a));
        mix(std::hash<int>{}(static_cast<int>(s.style.font_weight)));
        mix(std::hash<bool>{}(s.style.italic));
        return h;
    }

} // namespace systems::leal::campello_widgets
