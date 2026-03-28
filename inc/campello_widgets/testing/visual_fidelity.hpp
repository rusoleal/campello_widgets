#pragma once

#include <campello_widgets/ui/canvas.hpp>
#include <campello_widgets/ui/render_object.hpp>
#include <campello_widgets/ui/color.hpp>
#include <campello_widgets/ui/draw_command.hpp>
#include <string>
#include <vector>
#include <memory>

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
     * @brief Captures a RenderObject tree to a PNG file.
     *
     * This is the main entry point for visual fidelity testing.
     * It performs layout, captures paint commands, and renders to PNG.
     *
     * @param root The root RenderObject to render
     * @param constraints Layout constraints
     * @param viewportWidth The viewport width in pixels
     * @param viewportHeight The viewport height in pixels
     * @param outputPath Path to save the PNG file
     * @return true on success
     */
    bool captureToPng(
        RenderObject& root,
        const BoxConstraints& constraints,
        float viewportWidth,
        float viewportHeight,
        const std::string& outputPath);

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
