// Text on the web: the browser's Canvas2D (measureText / fillText), through
// src/web/campello_widgets_web.js. Same contract as the Linux FreeType
// rasterizer: width is the advance, height ascent + descent, bitmap BGRA8
// premultiplied.

#include "gpu/vulkan/text_rasterizer.hpp"

#include <campello_widgets/ui/text_style.hpp>

#include <cmath>
#include <string>

extern "C" {
void cw_web_text_measure(const char* text, const char* font, float* out);
void cw_web_text_rasterize(const char* text, const char* font, float r, float g, float b, float a,
                           int w, int h, float baseline, uint8_t* out);
}

namespace systems::leal::campello_widgets
{

namespace {

// CSS font shorthand for a style: "[italic] <weight> <size>px <family>".
std::string cssFont(const TextStyle& style)
{
    std::string font;
    if (style.italic) font += "italic ";
    font += std::to_string(static_cast<int>(style.font_weight));
    font += ' ';
    font += std::to_string(style.font_size);
    font += "px ";
    if (style.font_family.empty()) {
        font += "system-ui, -apple-system, 'Segoe UI', Roboto, sans-serif";
    } else {
        font += '"';
        font += style.font_family;
        font += "\", system-ui, sans-serif";
    }
    return font;
}

struct Metrics
{
    float advance = 0.0f;
    float ascent  = 0.0f;
    float descent = 0.0f;
};

Metrics measureSpan(const TextSpan& span, const std::string& font)
{
    float out[3] = {};
    cw_web_text_measure(span.text.c_str(), font.c_str(), out);
    return Metrics{ out[0], out[1], out[2] };
}

class WebTextRasterizer final : public ITextRasterizer
{
public:
    bool isAvailable() const noexcept override { return true; }

    Size measure(const TextSpan& span) override
    {
        const auto m = measureSpan(span, cssFont(span.style));
        return Size{ m.advance, m.ascent + m.descent };
    }

    Bitmap rasterize(const TextSpan& span) override
    {
        Bitmap bmp;
        if (span.text.empty()) return bmp;
        const std::string font = cssFont(span.style);
        const auto m = measureSpan(span, font);
        bmp.width  = static_cast<int>(std::ceil(m.advance));
        bmp.height = static_cast<int>(std::ceil(m.ascent + m.descent));
        if (bmp.width <= 0 || bmp.height <= 0) return Bitmap{};
        bmp.pixels.resize(static_cast<size_t>(bmp.width) * bmp.height * 4);
        const Color& c = span.style.color;
        cw_web_text_rasterize(span.text.c_str(), font.c_str(), c.r, c.g, c.b, c.a,
                              bmp.width, bmp.height, m.ascent, bmp.pixels.data());
        return bmp;
    }
};

} // namespace

std::unique_ptr<ITextRasterizer> createPlatformTextRasterizer()
{
    return std::make_unique<WebTextRasterizer>();
}

} // namespace systems::leal::campello_widgets
