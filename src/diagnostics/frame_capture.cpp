#include "frame_capture.hpp"

// stb_image_write, private to this file: STB_IMAGE_WRITE_STATIC gives every
// function internal linkage, so it can't clash with the copy compiled into
// src/testing/visual_fidelity.cpp when an app links both.
#define STB_IMAGE_WRITE_STATIC
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "../../tests/third_party/stb_image_write.h"

#include <cstdio>
#include <vector>

namespace systems::leal::campello_widgets::detail
{
    bool writeFramePng(const std::string&        path,
                       uint32_t                  width,
                       uint32_t                  height,
                       const uint8_t*            pixels,
                       campello_gpu::PixelFormat format)
    {
        namespace GPU = campello_gpu;
        bool swap_red_blue = false;
        switch (format)
        {
            case GPU::PixelFormat::rgba8unorm:
            case GPU::PixelFormat::rgba8unorm_srgb:
                break;
            case GPU::PixelFormat::bgra8unorm:
            case GPU::PixelFormat::bgra8unorm_srgb:
                swap_red_blue = true;
                break;
            default:
                std::fprintf(stderr, "[campello_widgets] frame capture: unsupported pixel format %d\n",
                             static_cast<int>(format));
                return false;
        }

        // A window screenshot: RGB only. The window is drawn over an opaque
        // clear colour, so alpha carries no information worth keeping.
        const size_t count = static_cast<size_t>(width) * height;
        std::vector<uint8_t> rgb(count * 3);
        for (size_t i = 0; i < count; ++i)
        {
            const uint8_t* p = pixels + i * 4;
            rgb[i * 3 + 0] = swap_red_blue ? p[2] : p[0];
            rgb[i * 3 + 1] = p[1];
            rgb[i * 3 + 2] = swap_red_blue ? p[0] : p[2];
        }

        if (stbi_write_png(path.c_str(), static_cast<int>(width), static_cast<int>(height), 3,
                           rgb.data(), static_cast<int>(width) * 3) == 0)
        {
            std::fprintf(stderr, "[campello_widgets] frame capture: could not write %s\n", path.c_str());
            return false;
        }
        return true;
    }

} // namespace systems::leal::campello_widgets::detail
