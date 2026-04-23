#include "linux_text_rasterizer.hpp"
#include <campello_widgets/ui/draw_backend.hpp>
#include <campello_widgets/ui/rect.hpp>

#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_BITMAP_H
#include FT_GLYPH_H

#include <hb.h>
#include <hb-ft.h>

#include <fontconfig/fontconfig.h>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <iostream>

namespace systems::leal::campello_widgets
{

// ---------------------------------------------------------------------------
// Glyph layout helper
// ---------------------------------------------------------------------------
struct GlyphLayout
{
    uint32_t glyph_id;
    float    origin_x;
    float    origin_y;
    float    bmp_left;
    float    bmp_top;
    int      bmp_width;
    int      bmp_height;
};

static bool shapeAndLayout(
    FT_Face    face,
    hb_font_t* hb_font,
    const char* text,
    float       size,
    std::vector<GlyphLayout>& out_glyphs,
    float& out_width,
    float& out_height)
{
    out_glyphs.clear();
    if (!face || !hb_font || !text || text[0] == '\0') return false;

    FT_Set_Pixel_Sizes(face, 0, static_cast<FT_UInt>(std::max(1.0f, size)));

    hb_buffer_t* buf = hb_buffer_create();
    hb_buffer_add_utf8(buf, text, -1, 0, -1);
    hb_buffer_set_direction(buf, HB_DIRECTION_LTR);
    hb_buffer_set_script(buf, HB_SCRIPT_LATIN);
    hb_buffer_set_language(buf, hb_language_from_string("en", -1));

    hb_shape(hb_font, buf, nullptr, 0);

    unsigned int         glyph_count = 0;
    hb_glyph_info_t*     infos       = hb_buffer_get_glyph_infos(buf, &glyph_count);
    hb_glyph_position_t* positions   = hb_buffer_get_glyph_positions(buf, &glyph_count);

    if (glyph_count == 0) {
        hb_buffer_destroy(buf);
        return false;
    }

    out_glyphs.reserve(glyph_count);

    float cursor_x = 0.0f;
    float cursor_y = 0.0f;
    float min_x = 1e9f, max_x = -1e9f;
    float min_y = 1e9f, max_y = -1e9f;

    (void)face->size->metrics.descender; // unused

    for (unsigned int i = 0; i < glyph_count; ++i) {
        uint32_t gid = infos[i].codepoint;
        float x_offset  = positions[i].x_offset  / 64.0f;
        float y_offset  = positions[i].y_offset  / 64.0f;
        float x_advance = positions[i].x_advance / 64.0f;

        float origin_x = cursor_x + x_offset;
        float origin_y = cursor_y + y_offset;

        // Load glyph metrics (no rendering yet — just bbox)
        if (FT_Load_Glyph(face, gid, FT_LOAD_DEFAULT) != 0) {
            cursor_x += x_advance;
            continue;
        }

        FT_GlyphSlot slot = face->glyph;
        float bl = static_cast<float>(slot->bitmap_left);
        float bt = static_cast<float>(slot->bitmap_top);
        float gw = static_cast<float>(slot->bitmap.width);
        float gh = static_cast<float>(slot->bitmap.rows);

        GlyphLayout gl{};
        gl.glyph_id   = gid;
        gl.origin_x   = origin_x;
        gl.origin_y   = origin_y;
        gl.bmp_left   = bl;
        gl.bmp_top    = bt;
        gl.bmp_width  = static_cast<int>(gw);
        gl.bmp_height = static_cast<int>(gh);
        out_glyphs.push_back(gl);

        float glyph_left   = origin_x + bl;
        float glyph_right  = glyph_left + gw;
        float glyph_top    = origin_y - bt;
        float glyph_bottom = glyph_top + gh;

        min_x = std::min(min_x, glyph_left);
        max_x = std::max(max_x, glyph_right);
        min_y = std::min(min_y, glyph_top);
        max_y = std::max(max_y, glyph_bottom);

        cursor_x += x_advance;
    }

    hb_buffer_destroy(buf);

    if (out_glyphs.empty()) {
        out_width  = 0.0f;
        out_height = 0.0f;
        return false;
    }

    out_width  = max_x - min_x;
    out_height = max_y - min_y;

    // Adjust glyph origins so the top-left of the bbox is at (0,0)
    for (auto& gl : out_glyphs) {
        gl.origin_x -= min_x;
        gl.origin_y -= min_y;
    }

    return true;
}

// ---------------------------------------------------------------------------
// Construction / destruction
// ---------------------------------------------------------------------------

LinuxTextRasterizer::LinuxTextRasterizer()
{
    initialize();
}

LinuxTextRasterizer::~LinuxTextRasterizer()
{
    if (hb_font_) {
        hb_font_destroy(hb_font_);
        hb_font_ = nullptr;
    }
    if (face_) {
        FT_Done_Face(face_);
        face_ = nullptr;
    }
    if (ft_lib_) {
        FT_Done_FreeType(ft_lib_);
        ft_lib_ = nullptr;
    }
}

bool LinuxTextRasterizer::initialize()
{
    if (initialized_) return face_ != nullptr;
    initialized_ = true;

    if (FT_Init_FreeType(&ft_lib_) != 0) {
        std::cerr << "[LinuxTextRasterizer] Failed to init FreeType\n";
        return false;
    }

    std::string font_path;
    if (!findSystemFont(font_path)) {
        std::cerr << "[LinuxTextRasterizer] No system font found\n";
        return false;
    }

    if (FT_New_Face(ft_lib_, font_path.c_str(), 0, &face_) != 0) {
        std::cerr << "[LinuxTextRasterizer] Failed to load font: " << font_path << "\n";
        return false;
    }

    hb_font_ = hb_ft_font_create_referenced(face_);
    if (!hb_font_) {
        std::cerr << "[LinuxTextRasterizer] Failed to create HarfBuzz font\n";
        return false;
    }

    return true;
}

bool LinuxTextRasterizer::findSystemFont(std::string& out_path)
{
    if (!FcInit()) return false;

    FcPattern* pattern = FcPatternCreate();
    if (!pattern) return false;

    FcPatternAddString(pattern, FC_FAMILY, reinterpret_cast<const FcChar8*>("sans"));
    FcConfigSubstitute(nullptr, pattern, FcMatchPattern);
    FcDefaultSubstitute(pattern);

    FcResult result;
    FcPattern* font = FcFontMatch(nullptr, pattern, &result);
    FcPatternDestroy(pattern);

    if (!font) return false;

    FcChar8* file = nullptr;
    if (FcPatternGetString(font, FC_FILE, 0, &file) == FcResultMatch && file) {
        out_path = reinterpret_cast<const char*>(file);
    }

    FcPatternDestroy(font);
    return !out_path.empty();
}

// ---------------------------------------------------------------------------
// measure
// ---------------------------------------------------------------------------

Size LinuxTextRasterizer::measure(const TextSpan& span)
{
    if (!isAvailable() || span.text.empty()) {
        const float char_width  = span.style.font_size * 0.6f;
        const float line_height = span.style.font_size * 1.2f;
        return Size{ char_width * static_cast<float>(span.text.size()), line_height };
    }

    std::vector<GlyphLayout> glyphs;
    float width, height;
    if (!shapeAndLayout(face_, hb_font_, span.text.c_str(),
                        span.style.font_size, glyphs, width, height)) {
        const float char_width  = span.style.font_size * 0.6f;
        const float line_height = span.style.font_size * 1.2f;
        return Size{ char_width * static_cast<float>(span.text.size()), line_height };
    }

    return Size{ width, height };
}

// ---------------------------------------------------------------------------
// rasterize
// ---------------------------------------------------------------------------

LinuxTextRasterizer::Bitmap LinuxTextRasterizer::rasterize(const TextSpan& span)
{
    Bitmap bmp;
    if (!isAvailable() || span.text.empty()) return bmp;

    std::vector<GlyphLayout> glyphs;
    float width_f, height_f;
    if (!shapeAndLayout(face_, hb_font_, span.text.c_str(),
                        span.style.font_size, glyphs, width_f, height_f)) {
        return bmp;
    }

    int w = static_cast<int>(std::ceil(width_f));
    int h = static_cast<int>(std::ceil(height_f));
    if (w <= 0 || h <= 0) return bmp;

    // Allocate BGRA8 buffer, cleared to transparent black
    bmp.width  = w;
    bmp.height = h;
    bmp.pixels.resize(static_cast<size_t>(w) * h * 4, 0);

    const Color& c = span.style.color;

    for (const auto& gl : glyphs) {
        if (gl.bmp_width <= 0 || gl.bmp_height <= 0) continue;

        if (FT_Load_Glyph(face_, gl.glyph_id, FT_LOAD_RENDER) != 0) continue;

        FT_GlyphSlot slot = face_->glyph;
        FT_Bitmap* ftbmp = &slot->bitmap;

        if (ftbmp->pixel_mode != FT_PIXEL_MODE_GRAY) continue;

        int dst_x = static_cast<int>(gl.origin_x + gl.bmp_left);
        int dst_y = static_cast<int>(gl.origin_y - gl.bmp_top);

        // Clamp to bitmap bounds
        int src_w = ftbmp->width;
        int src_h = ftbmp->rows;
        int src_pitch = ftbmp->pitch;

        for (int row = 0; row < src_h; ++row) {
            int py = dst_y + row;
            if (py < 0 || py >= h) continue;

            const uint8_t* src_row = ftbmp->buffer + row * src_pitch;
            uint8_t* dst_row = bmp.pixels.data() + (py * w + dst_x) * 4;

            for (int col = 0; col < src_w; ++col) {
                int px = dst_x + col;
                if (px < 0 || px >= w) continue;

                uint8_t alpha = src_row[col];
                if (alpha == 0) continue;

                uint8_t* p = dst_row + col * 4;
                // Premultiplied alpha BGRA
                p[0] = static_cast<uint8_t>(c.b * alpha);
                p[1] = static_cast<uint8_t>(c.g * alpha);
                p[2] = static_cast<uint8_t>(c.r * alpha);
                p[3] = alpha;
            }
        }
    }

    return bmp;
}

} // namespace systems::leal::campello_widgets
