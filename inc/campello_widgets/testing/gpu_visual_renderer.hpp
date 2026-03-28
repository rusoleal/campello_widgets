#pragma once

#include <campello_widgets/ui/draw_command.hpp>
#include <campello_widgets/ui/color.hpp>
#include <memory>
#include <string>

namespace systems::leal::campello_widgets::testing
{

    /**
     * @brief GPU-backed visual renderer for fidelity testing.
     *
     * Creates a headless GPU device, renders a DrawList to an offscreen RGBA8
     * texture via MetalDrawBackend, and reads back pixels using
     * Texture::download() for PNG export.
     *
     * Falls back gracefully: isValid() returns false when no GPU device is
     * available (headless CI), and renderDrawList() returns false when the
     * draw list contains commands unsupported by the GPU path (caller should
     * fall back to VisualRenderer).
     *
     * Platform support: macOS (Metal).  All other platforms use a null stub
     * that always returns false / isValid() == false.
     */
    class GpuVisualRenderer
    {
    public:
        GpuVisualRenderer(int width, int height);
        ~GpuVisualRenderer();

        GpuVisualRenderer(const GpuVisualRenderer&)            = delete;
        GpuVisualRenderer& operator=(const GpuVisualRenderer&) = delete;

        /** @brief Returns true if a GPU device was successfully created. */
        bool isValid() const;

        /** @brief Sets the background clear colour used by the next renderDrawList call. */
        void setClearColor(const Color& color);

        /**
         * @brief Renders the draw list via GPU.
         *
         * @return true  — rendering succeeded; call saveToPng() to save output.
         * @return false — one or more commands are unsupported (path, shadow,
         *                 non-affine transform, etc.); caller should fall back
         *                 to the CPU VisualRenderer.
         */
        bool renderDrawList(const DrawList& commands);

        /**
         * @brief Downloads pixels from the GPU and writes a PNG file.
         * Must be called after a successful renderDrawList().
         */
        bool saveToPng(const std::string& filepath);

        int width()  const;
        int height() const;

    private:
        struct Impl;
        std::unique_ptr<Impl> impl_;
    };

} // namespace systems::leal::campello_widgets::testing
