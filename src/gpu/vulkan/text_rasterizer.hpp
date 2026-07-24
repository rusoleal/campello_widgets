#pragma once

#include <campello_widgets/ui/size.hpp>
#include <campello_widgets/ui/text_span.hpp>
#include <cstdint>
#include <memory>
#include <vector>

namespace systems::leal::campello_widgets
{

// ---------------------------------------------------------------------------
// ITextRasterizer
//
// Platform-native CPU text rasterization, injected into VulkanDrawBackend so
// the single shared Vulkan backend (src/gpu/vulkan/) can serve both Android
// (JNI Canvas/Paint) and Linux (FreeType + HarfBuzz) without forking the
// GPU-facing draw code. One texture per draw call — no glyph atlas (same
// pattern as the Metal/D3D12 backends).
// ---------------------------------------------------------------------------
class ITextRasterizer
{
public:
    virtual ~ITextRasterizer() = default;

    struct Bitmap
    {
        std::vector<uint8_t> pixels; ///< BGRA8 premultiplied, row-major
        int                  width  = 0;
        int                  height = 0;
    };

    /** @brief Returns true if the rasterizer initialized successfully. */
    virtual bool isAvailable() const noexcept = 0;

    /** @brief Measures the bounding box of @p span. */
    virtual Size measure(const TextSpan& span) = 0;

    /** @brief Rasterises @p span into a CPU bitmap. */
    virtual Bitmap rasterize(const TextSpan& span) = 0;
};

/**
 * @brief Constructs the platform's ITextRasterizer implementation.
 *
 * Implemented once per platform (android_text_rasterizer.cpp on Android,
 * linux_text_rasterizer.cpp on Linux) — exactly one translation unit
 * providing this symbol is linked into any given build.
 */
std::unique_ptr<ITextRasterizer> createPlatformTextRasterizer();

} // namespace systems::leal::campello_widgets
