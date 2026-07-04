#pragma once

#include <campello_widgets/ui/size.hpp>
#include <campello_widgets/ui/color.hpp>
#include <campello_widgets/ui/text_span.hpp>
#include <vector>
#include <cstdint>
#include <string>
#include <memory>
#include <mutex>

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

    /** @brief Returns true if a system font was successfully loaded. */
    bool isAvailable() const noexcept { return variants_[0].face != nullptr; }

private:
    bool initialize();
    bool findSystemFont(bool bold, bool italic, std::string& out_path);

    struct FontVariant {
        FT_Face    face     = nullptr;
        hb_font_t* hb_font  = nullptr;
    };

    FontVariant& variantFor(const TextSpan& span);

    // FreeType and HarfBuzz are not thread-safe.  The UI thread calls
    // measure() during layout; the raster thread calls rasterize() during
    // paint.  This mutex serialises both to prevent concurrent FT_Load_Glyph
    // calls on the same FT_Face from crashing inside TT_RunIns.
    mutable std::mutex ft_mutex_;

    FT_Library   ft_lib_      = nullptr;
    // Indexed as: [bold<<1 | italic] → 0=regular, 1=italic, 2=bold, 3=bold+italic
    FontVariant  variants_[4];
    bool         initialized_ = false;
};

} // namespace systems::leal::campello_widgets
