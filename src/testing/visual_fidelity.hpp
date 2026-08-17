#pragma once

#include <campello_widgets/ui/canvas.hpp>
#include <campello_widgets/ui/render_object.hpp>
#include <campello_widgets/ui/color.hpp>
#include <campello_widgets/ui/draw_command.hpp>
#include <string>
#include <vector>
#include <memory>

namespace systems::leal::campello_gpu { class Device; }

namespace systems::leal::campello_widgets
{
    class RenderBox;
}

namespace systems::leal::campello_widgets::testing
{

    /**
     * @brief Renders a Canvas/DrawList to a PNG file.
     *
     * This is a software rasterizer for visual fidelity testing. It renders
     * the recorded draw commands to a CPU buffer and saves as PNG.
     *
     * Features:
     * - Basic shapes: rect, circle, rounded rect
     * - Solid fills and strokes
     * - Transform support
     * - Clip support
     *
     * Limitations:
     * - No anti-aliasing (pixel-perfect rendering)
     * - No gradients or complex shaders
     * - Text rendering is simplified (bounding boxes only)
     */
    class VisualRenderer
    {
    public:
        /**
         * @brief Creates a renderer with the specified viewport size.
         */
        VisualRenderer(int width, int height);
        ~VisualRenderer();

        /**
         * @brief Clears the buffer with the specified color.
         */
        void clear(const Color& color);

        /**
         * @brief Renders a DrawList to the buffer.
         */
        void renderDrawList(const DrawList& commands);

        /**
         * @brief Saves the current buffer as a PNG file.
         * @return true on success
         */
        bool saveToPng(const std::string& filepath);

        /**
         * @brief Returns the raw RGBA pixel data.
         */
        const std::vector<uint8_t>& pixelData() const { return pixels_; }

        int width() const { return width_; }
        int height() const { return height_; }

        // Drawing methods (public so visitor can access)
        void setPixel(int x, int y, const Color& color);
        void fillRect(int x1, int y1, int x2, int y2, const Color& color);
        void strokeRect(int x1, int y1, int x2, int y2, int strokeWidth, const Color& color);
        void fillCircle(int cx, int cy, int radius, const Color& color);
        void fillRoundedRect(int x1, int y1, int x2, int y2, int radius, const Color& color);
        void drawLine(int x1, int y1, int x2, int y2, int strokeWidth, const Color& color);

        /**
         * @brief Draw a drop shadow for a rounded rectangle.
         *
         * The shadow is rendered outside the shape using a Gaussian falloff
         * matching the production renderer's blur profile.
         *
         * @param x1, y1, x2, y2 Screen-space bounding box of the occluder.
         * @param radius Screen-space corner radius.
         * @param sigma Screen-space Gaussian sigma (controls softness).
         * @param color Shadow color and peak alpha.
         */
        void drawShadow(int x1, int y1, int x2, int y2, int radius, float sigma, const Color& color);

        // Transform handling
        struct TransformStack {
            std::vector<Matrix4> stack;
            TransformStack() { stack.push_back(Matrix4::identity()); }
            Matrix4& current() { return stack.back(); }
            void push(const Matrix4& m) { stack.push_back(m * current()); }
            void pop() { if (stack.size() > 1) stack.pop_back(); }
        };

        // Clip handling
        struct ClipStack {
            std::vector<Rect> stack;
            void push(const Rect& r) { stack.push_back(r); }
            void pop() { if (!stack.empty()) stack.pop_back(); }
            bool isPointInside(float x, float y) const;
        };

    private:
        int width_;
        int height_;
        std::vector<uint8_t> pixels_;  // RGBA, row-major
    };

    /**
     * @brief Returns the GPU device shared by every offscreen capture in this
     * process, creating it on first call.
     *
     * A Vulkan `VkImageView` (and similarly Metal/D3D12 resources) can only
     * be bound into commands submitted on the same logical device that
     * created it. Tests that build their own GPU resources (e.g. loading an
     * image into a texture) and then feed them through captureDrawListToPng()
     * / captureRenderBoxToPng() must create those resources on this shared
     * device — not a separate `Device::createDefaultDevice()` — or the
     * resource will be invalid on the device actually used to render.
     */
    std::shared_ptr<campello_gpu::Device> sharedGpuDevice();

    /**
     * @brief Renders an already-recorded DrawList to a PNG file via the
     * production cw::Renderer + platform IDrawBackend — the same GPU code
     * path (Metal/Vulkan/D3D12) real running apps use, not a hand-rolled
     * duplicate.
     *
     * Use this when the caller already has a DrawList with no associated
     * RenderBox tree (e.g. a raw Canvas, or a RenderObject painted directly
     * via captureToPng()). For a mounted widget tree that needs DPR-correct
     * text/layout, use captureRenderBoxToPng() instead.
     *
     * @param commands Draw commands to render, in logical-pixel coordinates.
     * @param width, height Physical-pixel viewport / output texture size.
     * @param clearColor Background color the canvas is cleared to before painting.
     * @param outputPath Path to save the PNG file.
     * @param devicePixelRatio Logical-to-physical scale baked in by the backend.
     * @return true on success (false if no GPU device is available — caller
     *         should fall back to the CPU VisualRenderer).
     */
    bool captureDrawListToPng(
        const DrawList& commands,
        float width,
        float height,
        const Color& clearColor,
        const std::string& outputPath,
        float devicePixelRatio = 1.0f);

    /**
     * @brief Renders a mounted RenderBox tree to a PNG file via a full
     * cw::Renderer (layout + paint + GPU raster), with correct device-pixel-
     * ratio handling — mirrors exactly how a real platform run loop
     * (src/macos/run_app.mm et al.) drives the renderer, just against an
     * offscreen texture instead of a swapchain.
     *
     * @param root RenderBox to lay out and paint (e.g. from Element::mount()).
     *        Must NOT already have had RenderBox::layout() called on it —
     *        RenderObject::activeBackend() is a raw, non-owning global
     *        pointer that only becomes valid once this call's own Renderer
     *        starts its layout pass; laying out earlier reads whatever a
     *        previous captureRenderBoxToPng() call's (already-destroyed)
     *        backend left behind.
     * @param physicalWidth, physicalHeight Physical-pixel viewport / output texture size.
     * @param devicePixelRatio Logical-to-physical scale; `root` is laid out
     *        at (physicalWidth/dpr, physicalHeight/dpr) logical pixels.
     * @param clearColor Background color the canvas is cleared to before painting.
     * @param outputPath Path to save the PNG file.
     * @return true on success.
     */
    bool captureRenderBoxToPng(
        std::shared_ptr<RenderBox> root,
        float physicalWidth,
        float physicalHeight,
        float devicePixelRatio,
        const Color& clearColor,
        const std::string& outputPath);

    /**
     * @brief Captures a RenderObject tree to a PNG file.
     *
     * This is the main entry point for visual fidelity testing.
     * It performs layout, captures paint commands, and renders to PNG via
     * captureDrawListToPng() (falling back to the CPU VisualRenderer only
     * when no GPU device is available).
     *
     * @param root The root RenderObject to render
     * @param constraints Layout constraints
     * @param viewportWidth The viewport width in pixels
     * @param viewportHeight The viewport height in pixels
     * @param outputPath Path to save the PNG file
     * @param clearColor Background color the canvas is cleared to before painting
     * @return true on success
     */
    bool captureToPng(
        RenderObject& root,
        const BoxConstraints& constraints,
        float viewportWidth,
        float viewportHeight,
        const std::string& outputPath,
        const Color& clearColor = Color::white());

    /**
     * @brief Result of comparing two images.
     */
    struct ImageComparisonResult
    {
        bool match = true;
        double pixelDifference = 0.0;  // Percentage of pixels that differ
        double maxChannelDiff = 0.0;   // Maximum per-channel difference
        std::string diffImagePath;     // Path to generated diff image
        std::vector<std::string> errors;
    };

    /**
     * @brief Compares two PNG images pixel-by-pixel.
     *
     * @param expectedPath Path to the expected (golden) PNG
     * @param actualPath Path to the actual PNG
     * @param tolerance Maximum allowed per-channel difference (0-255)
     * @param generateDiff Whether to generate a diff image
     * @return Comparison result
     */
    ImageComparisonResult comparePngImages(
        const std::string& expectedPath,
        const std::string& actualPath,
        int tolerance = 2,
        bool generateDiff = true);

    /**
     * @brief Gets the path to the visual fidelity directory.
     */
    std::string getVisualFidelityDirectory();

    /**
     * @brief Gets the path to the Flutter goldens directory.
     */
    std::string getFlutterGoldensDirectory();

    /**
     * @brief Gets the path to the C++ output directory.
     */
    std::string getCppOutputDirectory();

    /**
     * @brief Gets the path to the diff output directory.
     */
    std::string getDiffDirectory();

} // namespace systems::leal::campello_widgets::testing
