#pragma once

#include <campello_widgets/ui/size.hpp>
#include <campello_widgets/ui/color.hpp>
#include <campello_widgets/ui/text_span.hpp>
#include <vector>
#include <cstdint>
#include <string>
#include <memory>

// Forward declarations for FreeType / HarfBuzz
typedef struct FT_LibraryRec_  *FT_Library;
typedef struct FT_FaceRec_     *FT_Face;
struct hb_font_t;

namespace systems::leal::campello_widgets
{

// ---------------------------------------------------------------------------
// LinuxTextRasterizer
//
// CPU text rasterisation using FreeType + HarfBuzz + fontconfig.
//
//   measure()    → bounding size of a text span
//   rasterize()  → RGBA8 premultiplied bitmap (BGRA memory layout)
//
// One texture per draw call — no glyph atlas (same pattern as Metal).
// ---------------------------------------------------------------------------
class LinuxTextRasterizer
{
public:
    LinuxTextRasterizer();
    ~LinuxTextRasterizer();

    /** @brief Returns true if a system font was successfully loaded. */
    bool isAvailable() const noexcept { return face_ != nullptr; }

    /** @brief Measures the bounding box of @p span. */
    Size measure(const TextSpan& span);

    struct Bitmap
    {
        std::vector<uint8_t> pixels; ///< BGRA8 premultiplied, row-major
        int                  width  = 0;
        int                  height = 0;
    };

    /** @brief Rasterises @p span into a CPU bitmap. */
    Bitmap rasterize(const TextSpan& span);

private:
    bool initialize();
    bool findSystemFont(std::string& out_path);

    FT_Library ft_lib_  = nullptr;
    FT_Face    face_    = nullptr;
    hb_font_t* hb_font_ = nullptr;
    bool       initialized_ = false;
};

} // namespace systems::leal::campello_widgets
