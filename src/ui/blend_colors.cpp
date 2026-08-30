#include "ui/blend_colors.hpp"
#include <algorithm>

namespace systems::leal::campello_widgets
{
    Color blendColors(const Color& src, const Color& dst, BlendMode mode)
    {
        const float sr = src.r * src.a, sg = src.g * src.a, sb = src.b * src.a, sa = src.a;
        const float dr = dst.r * dst.a, dg = dst.g * dst.a, db = dst.b * dst.a, da = dst.a;

        if (mode == BlendMode::modulate)
        {
            const float a = sa * da;
            return (a > 1e-5f)
                ? Color::fromRGBA(sr * dr / a, sg * dg / a, sb * db / a, a)
                : Color::fromRGBA(0.0f, 0.0f, 0.0f, 0.0f);
        }

        float fa = 1.0f, fb = 0.0f;
        switch (mode)
        {
            case BlendMode::clear:   fa = 0.0f;      fb = 0.0f;      break;
            case BlendMode::src:     fa = 1.0f;      fb = 0.0f;      break;
            case BlendMode::dst:     fa = 0.0f;      fb = 1.0f;      break;
            case BlendMode::srcOver: fa = 1.0f;      fb = 1.0f - sa; break;
            case BlendMode::dstOver: fa = 1.0f - da; fb = 1.0f;      break;
            case BlendMode::srcIn:   fa = da;        fb = 0.0f;      break;
            case BlendMode::dstIn:   fa = 0.0f;      fb = sa;        break;
            case BlendMode::srcOut:  fa = 1.0f - da; fb = 0.0f;      break;
            case BlendMode::dstOut:  fa = 0.0f;      fb = 1.0f - sa; break;
            case BlendMode::srcATop: fa = da;        fb = 1.0f - sa; break;
            case BlendMode::dstATop: fa = 1.0f - da; fb = sa;        break;
            case BlendMode::xorMode: fa = 1.0f - da; fb = 1.0f - sa; break;
            case BlendMode::plus:    fa = 1.0f;      fb = 1.0f;      break;
            case BlendMode::modulate: break; // handled above
        }

        float r = sr * fa + dr * fb;
        float g = sg * fa + dg * fb;
        float b = sb * fa + db * fb;
        float a = sa * fa + da * fb;
        if (mode == BlendMode::plus)
        {
            r = std::min(r, 1.0f); g = std::min(g, 1.0f);
            b = std::min(b, 1.0f); a = std::min(a, 1.0f);
        }

        return (a > 1e-5f)
            ? Color::fromRGBA(r / a, g / a, b / a, a)
            : Color::fromRGBA(0.0f, 0.0f, 0.0f, 0.0f);
    }
}
