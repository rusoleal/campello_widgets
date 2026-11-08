#pragma once

#include <campello_gpu/constants/pixel_format.hpp>

#include <cstdint>
#include <string>

namespace systems::leal::campello_widgets::detail
{
    /**
     * @brief Writes a downloaded frame (tightly packed, 4 bytes per pixel) to
     *        a PNG file as an opaque RGB image.
     *
     * Accepts rgba8unorm(_srgb) and bgra8unorm(_srgb) pixels — the formats
     * campello_widgets' draw backends render the window in. Returns false
     * (with a message on stderr) for any other format or a write failure.
     */
    bool writeFramePng(const std::string&        path,
                       uint32_t                  width,
                       uint32_t                  height,
                       const uint8_t*            pixels,
                       campello_gpu::PixelFormat format);

} // namespace systems::leal::campello_widgets::detail
