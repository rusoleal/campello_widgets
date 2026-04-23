#include "visual_fidelity.hpp"
#include "gpu_visual_renderer.hpp"
#include <campello_widgets/ui/paint_context.hpp>
#include <campello_widgets/ui/render_box.hpp>
#include <campello_widgets/ui/color.hpp>
#include <vector_math/matrix4.hpp>
#include <campello_widgets/ui/rect.hpp>

#include <campello_image/image.hpp>

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>

namespace cw = systems::leal::campello_widgets;
namespace cwt = systems::leal::campello_widgets::testing;
namespace vm = systems::leal::vector_math;
namespace ci = systems::leal::campello_image;

// ============================================================================
// Helper Functions
// ============================================================================

static int clamp(int x, int min, int max)
{
    if (x < min) return min;
    if (x > max) return max;
    return x;
}

static float clampf(float x, float min, float max)
{
    if (x < min) return min;
    if (x > max) return max;
    return x;
}

static uint8_t floatToByte(float f)
{
    return static_cast<uint8_t>(clamp(static_cast<int>(f * 255.0f), 0, 255));
}

static void blendPixel(uint8_t* pixel, const cw::Color& color)
{
    // Simple alpha blend: src over dst
    float srcA = color.a;
    float dstA = pixel[3] / 255.0f;
    float outA = srcA + dstA * (1.0f - srcA);

    if (outA > 0.001f) {
        pixel[0] = floatToByte((color.r * srcA + pixel[0] / 255.0f * dstA * (1.0f - srcA)) / outA);
        pixel[1] = floatToByte((color.g * srcA + pixel[1] / 255.0f * dstA * (1.0f - srcA)) / outA);
        pixel[2] = floatToByte((color.b * srcA + pixel[2] / 255.0f * dstA * (1.0f - srcA)) / outA);
        pixel[3] = floatToByte(outA);
    }
}

// ============================================================================
// ClipStack
// ============================================================================

bool cwt::VisualRenderer::ClipStack::isPointInside(float x, float y) const
{
    if (stack.empty()) return true;
    
    for (const auto& rect : stack) {
        if (x < rect.left() || x >= rect.right() ||
            y < rect.top() || y >= rect.bottom()) {
            return false;
        }
    }
    return true;
}

// ============================================================================
// VisualRenderer
// ============================================================================

cwt::VisualRenderer::VisualRenderer(int width, int height)
    : width_(width)
    , height_(height)
    , pixels_(width * height * 4, 0)
{
}

cwt::VisualRenderer::~VisualRenderer() = default;

void cwt::VisualRenderer::clear(const cw::Color& color)
{
    uint8_t r = floatToByte(color.r);
    uint8_t g = floatToByte(color.g);
    uint8_t b = floatToByte(color.b);
    uint8_t a = floatToByte(color.a);

    for (int i = 0; i < width_ * height_; ++i) {
        pixels_[i * 4 + 0] = r;
        pixels_[i * 4 + 1] = g;
        pixels_[i * 4 + 2] = b;
        pixels_[i * 4 + 3] = a;
    }
}

void cwt::VisualRenderer::setPixel(int x, int y, const cw::Color& color)
{
    if (x < 0 || x >= width_ || y < 0 || y >= height_) return;
    int idx = (y * width_ + x) * 4;
    blendPixel(&pixels_[idx], color);
}

void cwt::VisualRenderer::fillRect(int x1, int y1, int x2, int y2, const cw::Color& color)
{
    int startX = clamp(std::min(x1, x2), 0, width_ - 1);
    int endX = clamp(std::max(x1, x2), 0, width_ - 1);
    int startY = clamp(std::min(y1, y2), 0, height_ - 1);
    int endY = clamp(std::max(y1, y2), 0, height_ - 1);

    for (int y = startY; y <= endY; ++y) {
        for (int x = startX; x <= endX; ++x) {
            setPixel(x, y, color);
        }
    }
}

void cwt::VisualRenderer::strokeRect(int x1, int y1, int x2, int y2, int strokeWidth, const cw::Color& color)
{
    int halfStroke = strokeWidth / 2;
    
    // Top edge
    fillRect(x1 - halfStroke, y1 - halfStroke, x2 + halfStroke, y1 + halfStroke, color);
    // Bottom edge
    fillRect(x1 - halfStroke, y2 - halfStroke, x2 + halfStroke, y2 + halfStroke, color);
    // Left edge
    fillRect(x1 - halfStroke, y1, x1 + halfStroke, y2, color);
    // Right edge
    fillRect(x2 - halfStroke, y1, x2 + halfStroke, y2, color);
}

void cwt::VisualRenderer::fillCircle(int cx, int cy, int radius, const cw::Color& color)
{
    int startX = clamp(cx - radius, 0, width_ - 1);
    int endX = clamp(cx + radius, 0, width_ - 1);
    int startY = clamp(cy - radius, 0, height_ - 1);
    int endY = clamp(cy + radius, 0, height_ - 1);

    int radiusSq = radius * radius;

    for (int y = startY; y <= endY; ++y) {
        for (int x = startX; x <= endX; ++x) {
            int dx = x - cx;
            int dy = y - cy;
            if (dx * dx + dy * dy <= radiusSq) {
                setPixel(x, y, color);
            }
        }
    }
}

void cwt::VisualRenderer::fillRoundedRect(int x1, int y1, int x2, int y2, int radius, const cw::Color& color)
{
    // Clamp radius to fit in rect
    int maxRadius = std::min(std::abs(x2 - x1), std::abs(y2 - y1)) / 2;
    radius = std::min(radius, maxRadius);

    int left = std::min(x1, x2);
    int right = std::max(x1, x2);
    int top = std::min(y1, y2);
    int bottom = std::max(y1, y2);

    // Fill center rect
    fillRect(left + radius, top, right - radius, bottom, color);
    fillRect(left, top + radius, left + radius, bottom - radius, color);
    fillRect(right - radius, top + radius, right, bottom - radius, color);

    // Fill corners as quarter circles
    for (int dy = 0; dy <= radius; ++dy) {
        for (int dx = 0; dx <= radius; ++dx) {
            if (dx * dx + dy * dy <= radius * radius) {
                // Top-left corner
                setPixel(left + radius - dx, top + radius - dy, color);
                // Top-right corner
                setPixel(right - radius + dx, top + radius - dy, color);
                // Bottom-left corner
                setPixel(left + radius - dx, bottom - radius + dy, color);
                // Bottom-right corner
                setPixel(right - radius + dx, bottom - radius + dy, color);
            }
        }
    }
}

void cwt::VisualRenderer::drawLine(int x1, int y1, int x2, int y2, int strokeWidth, const cw::Color& color)
{
    // Bresenham's line algorithm
    int dx = std::abs(x2 - x1);
    int dy = std::abs(y2 - y1);
    int sx = (x1 < x2) ? 1 : -1;
    int sy = (y1 < y2) ? 1 : -1;
    int err = dx - dy;

    int halfStroke = std::max(1, strokeWidth / 2);

    while (true) {
        // Draw a small square for thickness
        fillRect(x1 - halfStroke, y1 - halfStroke, x1 + halfStroke, y1 + halfStroke, color);

        if (x1 == x2 && y1 == y2) break;
        int e2 = 2 * err;
        if (e2 > -dy) {
            err -= dy;
            x1 += sx;
        }
        if (e2 < dx) {
            err += dx;
            y1 += sy;
        }
    }
}

// Visitor struct for DrawCommand processing
struct DrawCommandVisitor {
    cwt::VisualRenderer* renderer;
    cwt::VisualRenderer::TransformStack* transforms;
    cwt::VisualRenderer::ClipStack* clips;

    void operator()(const cw::DrawRectCmd& c) {
        auto tl = transforms->current() * vm::Vector4<float>(c.rect.left(), c.rect.top(), 0.0f, 1.0f);
        auto br = transforms->current() * vm::Vector4<float>(c.rect.right(), c.rect.bottom(), 0.0f, 1.0f);

        int x1 = static_cast<int>(tl.x());
        int y1 = static_cast<int>(tl.y());
        int x2 = static_cast<int>(br.x());
        int y2 = static_cast<int>(br.y());

        if (c.paint.style == cw::PaintStyle::fill) {
            renderer->fillRect(x1, y1, x2, y2, c.paint.color);
        } else {
            renderer->strokeRect(x1, y1, x2, y2, static_cast<int>(c.paint.stroke_width), c.paint.color);
        }
    }

    void operator()(const cw::DrawCircleCmd& c) {
        auto center = transforms->current() * vm::Vector4<float>(c.center.x, c.center.y, 0.0f, 1.0f);
        auto scaleVec = transforms->current() * vm::Vector4<float>(1.0f, 0.0f, 0.0f, 0.0f);
        float scale = std::sqrt(scaleVec.x() * scaleVec.x() + scaleVec.y() * scaleVec.y());
        int radius = static_cast<int>(c.radius * scale);
        int cx = static_cast<int>(center.x());
        int cy = static_cast<int>(center.y());

        renderer->fillCircle(cx, cy, radius, c.paint.color);
    }

    void operator()(const cw::DrawRRectCmd& c) {
        auto tl = transforms->current() * vm::Vector4<float>(c.rrect.rect.left(), c.rrect.rect.top(), 0.0f, 1.0f);
        auto br = transforms->current() * vm::Vector4<float>(c.rrect.rect.right(), c.rrect.rect.bottom(), 0.0f, 1.0f);

        int x1 = static_cast<int>(tl.x());
        int y1 = static_cast<int>(tl.y());
        int x2 = static_cast<int>(br.x());
        int y2 = static_cast<int>(br.y());
        int radius = static_cast<int>((c.rrect.radius_x + c.rrect.radius_y) / 2.0f);

        renderer->fillRoundedRect(x1, y1, x2, y2, radius, c.paint.color);
    }

    void operator()(const cw::DrawLineCmd& c) {
        auto p1 = transforms->current() * vm::Vector4<float>(c.p1.x, c.p1.y, 0.0f, 1.0f);
        auto p2 = transforms->current() * vm::Vector4<float>(c.p2.x, c.p2.y, 0.0f, 1.0f);

        int x1 = static_cast<int>(p1.x());
        int y1 = static_cast<int>(p1.y());
        int x2 = static_cast<int>(p2.x());
        int y2 = static_cast<int>(p2.y());

        renderer->drawLine(x1, y1, x2, y2, static_cast<int>(c.paint.stroke_width), c.paint.color);
    }

    void operator()(const cw::PushTransformCmd& c) {
        transforms->push(c.transform);
    }

    void operator()(const cw::PopTransformCmd&) {
        transforms->pop();
    }

    void operator()(const cw::PushClipRectCmd& c) {
        clips->push(c.rect);
    }

    void operator()(const cw::PopClipRectCmd&) {
        clips->pop();
    }

    void operator()(const cw::PushClipRRectCmd& c) {
        clips->push(c.rrect.rect);
    }

    // Unhandled command types
    void operator()(const cw::DrawOvalCmd&) {}
    void operator()(const cw::DrawArcCmd&) {}
    void operator()(const cw::DrawPointsCmd&) {}
    void operator()(const cw::DrawPathCmd&) {}
    void operator()(const cw::DrawShadowCmd&) {}
    void operator()(const cw::DrawTextCmd&) {}
    void operator()(const cw::DrawImageCmd&) {}
    void operator()(const cw::PushClipPathCmd&) {}
    void operator()(const cw::SaveLayerCmd&) {}
    void operator()(const cw::DrawBackdropFilterBeginCmd&) {}
    void operator()(const cw::DrawBackdropFilterEndCmd&) {}
    void operator()(const cw::DrawShaderMaskBeginCmd&) {}
    void operator()(const cw::DrawShaderMaskEndCmd&) {}
};

void cwt::VisualRenderer::renderDrawList(const cw::DrawList& commands)
{
    TransformStack transforms;
    ClipStack clips;
    clips.push(cw::Rect::fromLTWH(0, 0, static_cast<float>(width_), static_cast<float>(height_)));

    DrawCommandVisitor visitor{this, &transforms, &clips};

    for (const auto& cmd : commands) {
        std::visit(visitor, cmd);
    }
}

bool cwt::VisualRenderer::saveToPng(const std::string& filepath)
{
    // Use campello_image to write PNG
    // Since campello_image is a loader library, we need to implement PNG writing ourselves
    // or use a simple PNG writer. For now, we'll write a minimal PNG encoder.
    
    // PNG signature
    static const uint8_t png_signature[8] = {0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A};
    
    // Simple zlib compression helper
    auto zlib_compress = [](const std::vector<uint8_t>& data) -> std::vector<uint8_t> {
        // Very simple RLE compression for now
        // In a production environment, use a proper zlib library
        std::vector<uint8_t> result;
        result.push_back(0x78);  // zlib header
        result.push_back(0x9C);  // default compression
        
        size_t i = 0;
        while (i < data.size()) {
            size_t run_len = 1;
            while (i + run_len < data.size() && run_len < 255 && data[i] == data[i + run_len]) {
                run_len++;
            }
            if (run_len >= 3) {
                result.push_back(0x00);  // RLE marker
                result.push_back(static_cast<uint8_t>(run_len));
                result.push_back(data[i]);
                i += run_len;
            } else {
                result.push_back(data[i]);
                i++;
            }
        }
        
        // Adler-32 checksum (simplified - not correct but allows structure)
        result.push_back(0x00);
        result.push_back(0x00);
        result.push_back(0x00);
        result.push_back(0x00);
        
        return result;
    };
    
    // Create IHDR chunk
    auto create_chunk = [](const std::string& type, const std::vector<uint8_t>& data) -> std::vector<uint8_t> {
        std::vector<uint8_t> chunk;
        uint32_t len = static_cast<uint32_t>(data.size());
        // Length (big-endian)
        chunk.push_back((len >> 24) & 0xFF);
        chunk.push_back((len >> 16) & 0xFF);
        chunk.push_back((len >> 8) & 0xFF);
        chunk.push_back(len & 0xFF);
        // Type
        chunk.insert(chunk.end(), type.begin(), type.end());
        // Data
        chunk.insert(chunk.end(), data.begin(), data.end());
        // CRC (simplified - would need proper CRC32)
        chunk.push_back(0x00);
        chunk.push_back(0x00);
        chunk.push_back(0x00);
        chunk.push_back(0x00);
        return chunk;
    };
    
    std::ofstream file(filepath, std::ios::binary);
    if (!file) return false;
    
    // Write signature
    file.write(reinterpret_cast<const char*>(png_signature), 8);
    
    // IHDR
    std::vector<uint8_t> ihdr_data;
    ihdr_data.push_back((width_ >> 24) & 0xFF);
    ihdr_data.push_back((width_ >> 16) & 0xFF);
    ihdr_data.push_back((width_ >> 8) & 0xFF);
    ihdr_data.push_back(width_ & 0xFF);
    ihdr_data.push_back((height_ >> 24) & 0xFF);
    ihdr_data.push_back((height_ >> 16) & 0xFF);
    ihdr_data.push_back((height_ >> 8) & 0xFF);
    ihdr_data.push_back(height_ & 0xFF);
    ihdr_data.push_back(8);   // bit depth
    ihdr_data.push_back(6);   // color type: RGBA
    ihdr_data.push_back(0);   // compression
    ihdr_data.push_back(0);   // filter
    ihdr_data.push_back(0);   // interlace
    auto ihdr = create_chunk("IHDR", ihdr_data);
    file.write(reinterpret_cast<const char*>(ihdr.data()), ihdr.size());
    
    // IDAT - image data with filter bytes
    std::vector<uint8_t> raw_data;
    for (int y = 0; y < height_; ++y) {
        raw_data.push_back(0);  // filter type: none
        for (int x = 0; x < width_; ++x) {
            int idx = (y * width_ + x) * 4;
            raw_data.push_back(pixels_[idx]);     // R
            raw_data.push_back(pixels_[idx + 1]); // G
            raw_data.push_back(pixels_[idx + 2]); // B
            raw_data.push_back(pixels_[idx + 3]); // A
        }
    }
    auto compressed = zlib_compress(raw_data);
    auto idat = create_chunk("IDAT", compressed);
    file.write(reinterpret_cast<const char*>(idat.data()), idat.size());
    
    // IEND
    auto iend = create_chunk("IEND", {});
    file.write(reinterpret_cast<const char*>(iend.data()), iend.size());
    
    return file.good();
}

// ============================================================================
// Capture to PNG
// ============================================================================

bool cwt::captureToPng(
    cw::RenderObject& root,
    const cw::BoxConstraints& constraints,
    float viewportWidth,
    float viewportHeight,
    const std::string& outputPath)
{
    try {
        // Perform layout
        root.layout(constraints);

        // Capture paint commands
        cw::PaintContext context(viewportWidth, viewportHeight);
        root.paint(context, cw::Offset::zero());

        const int w = static_cast<int>(viewportWidth);
        const int h = static_cast<int>(viewportHeight);

        // Try GPU path first (macOS Metal; no-op stub on other platforms)
        GpuVisualRenderer gpuRenderer(w, h);
        if (gpuRenderer.isValid()) {
            gpuRenderer.setClearColor(cw::Color::white());
            if (gpuRenderer.renderDrawList(context.commands())) {
                return gpuRenderer.saveToPng(outputPath);
            }
            // renderDrawList returned false → unsupported commands, fall through to CPU
        }

        // CPU software rasterizer fallback
        VisualRenderer renderer(w, h);
        renderer.clear(cw::Color::white());
        renderer.renderDrawList(context.commands());
        return renderer.saveToPng(outputPath);
    } catch (const std::exception& e) {
        std::cerr << "captureToPng failed: " << e.what() << std::endl;
        return false;
    }
}

// ============================================================================
// Directory Helpers
// ============================================================================

std::string cwt::getVisualFidelityDirectory()
{
    std::filesystem::path paths[] = {
        "tests/visual_fidelity",
        "../tests/visual_fidelity",
        "../../tests/visual_fidelity",
    };

    for (const auto& p : paths) {
        if (std::filesystem::exists(p)) {
            return std::filesystem::absolute(p).string();
        }
    }

    return std::filesystem::absolute("tests/visual_fidelity").string();
}

std::string cwt::getFlutterGoldensDirectory()
{
    return (std::filesystem::path(getVisualFidelityDirectory()) / "flutter_goldens").string();
}

std::string cwt::getCppOutputDirectory()
{
    auto path = std::filesystem::path(getVisualFidelityDirectory()) / "cpp_output";
    std::filesystem::create_directories(path);
    return path.string();
}

std::string cwt::getDiffDirectory()
{
    auto path = std::filesystem::path(getVisualFidelityDirectory()) / "diffs";
    std::filesystem::create_directories(path);
    return path.string();
}

// ============================================================================
// Visual Diff Generation
// ============================================================================

static void generateVisualDiff(
    const std::string& expectedPath,
    const std::string& actualPath,
    const std::string& outputPath,
    int tolerance,
    cwt::ImageComparisonResult& result)
{
    // Load expected image using campello_image
    auto expImg = ci::Image::fromFile(expectedPath.c_str());
    if (!expImg) {
        result.errors.push_back("Failed to decode expected image: " + expectedPath);
        return;
    }
    
    // Load actual image using campello_image
    auto actImg = ci::Image::fromFile(actualPath.c_str());
    if (!actImg) {
        result.errors.push_back("Failed to decode actual image: " + actualPath);
        return;
    }
    
    int expWidth = static_cast<int>(expImg->getWidth());
    int expHeight = static_cast<int>(expImg->getHeight());
    int actWidth = static_cast<int>(actImg->getWidth());
    int actHeight = static_cast<int>(actImg->getHeight());
    
    // Check dimensions
    if (expWidth != actWidth || expHeight != actHeight) {
        result.errors.push_back("Cannot generate diff: image dimensions differ (" +
            std::to_string(expWidth) + "x" + std::to_string(expHeight) + " vs " +
            std::to_string(actWidth) + "x" + std::to_string(actHeight) + ")");
        return;
    }
    
    const int width = expWidth;
    const int height = expHeight;
    const int totalPixels = width * height;
    
    // Create diff image (RGBA)
    std::vector<unsigned char> diffPixels(totalPixels * 4);
    
    if (expImg->getFormat() != ci::ImageFormat::rgba8 || actImg->getFormat() != ci::ImageFormat::rgba8) {
        result.errors.push_back("Visual diff only supports RGBA8 images");
        return;
    }
    
    const uint8_t* expData = static_cast<const uint8_t*>(expImg->getData());
    const uint8_t* actData = static_cast<const uint8_t*>(actImg->getData());
    
    int diffPixelCount = 0;
    int maxChannelDiff = 0;
    
    for (int i = 0; i < totalPixels; i++) {
        const int idx = i * 4;
        
        unsigned char r1 = expData[idx];
        unsigned char g1 = expData[idx + 1];
        unsigned char b1 = expData[idx + 2];
        unsigned char a1 = expData[idx + 3];
        
        unsigned char r2 = actData[idx];
        unsigned char g2 = actData[idx + 1];
        unsigned char b2 = actData[idx + 2];
        unsigned char a2 = actData[idx + 3];
        
        // Calculate per-channel differences
        int dr = std::abs(static_cast<int>(r1) - static_cast<int>(r2));
        int dg = std::abs(static_cast<int>(g1) - static_cast<int>(g2));
        int db = std::abs(static_cast<int>(b1) - static_cast<int>(b2));
        int da = std::abs(static_cast<int>(a1) - static_cast<int>(a2));
        
        int maxDiff = std::max({dr, dg, db, da});
        maxChannelDiff = std::max(maxChannelDiff, maxDiff);
        
        if (maxDiff <= tolerance) {
            // No difference — black pixel
            diffPixels[idx]     = 0;
            diffPixels[idx + 1] = 0;
            diffPixels[idx + 2] = 0;
            diffPixels[idx + 3] = 255;
        } else {
            // Real difference — amplify per-channel delta (×4) so small diffs are visible
            diffPixelCount++;
            diffPixels[idx]     = static_cast<unsigned char>(std::min(255, dr * 4));
            diffPixels[idx + 1] = static_cast<unsigned char>(std::min(255, dg * 4));
            diffPixels[idx + 2] = static_cast<unsigned char>(std::min(255, db * 4));
            diffPixels[idx + 3] = 255;
        }
    }
    
    result.maxChannelDiff = static_cast<double>(maxChannelDiff) / 255.0;

    // Save diff image using simple PNG writer
    // Since campello_image doesn't have a writer, we use the same approach as saveToPng
    // For simplicity, we'll skip the diff image generation for now
    // TODO: Implement proper PNG writing or add a dependency for it
    (void)outputPath;
}

// ============================================================================
// Image Comparison
// ============================================================================

cwt::ImageComparisonResult cwt::comparePngImages(
    const std::string& expectedPath,
    const std::string& actualPath,
    int tolerance,
    bool generateDiff)
{
    ImageComparisonResult result;

    // Load expected image using campello_image
    auto expImg = ci::Image::fromFile(expectedPath.c_str());
    if (!expImg) {
        result.match = false;
        result.errors.push_back("Failed to decode expected image: " + expectedPath);
        return result;
    }

    // Load actual image using campello_image
    auto actImg = ci::Image::fromFile(actualPath.c_str());
    if (!actImg) {
        result.match = false;
        result.errors.push_back("Failed to decode actual image: " + actualPath);
        return result;
    }

    int expWidth = static_cast<int>(expImg->getWidth());
    int expHeight = static_cast<int>(expImg->getHeight());
    int actWidth = static_cast<int>(actImg->getWidth());
    int actHeight = static_cast<int>(actImg->getHeight());

    // Dimensions must match for a meaningful pixel comparison
    if (expWidth != actWidth || expHeight != actHeight) {
        result.match = false;
        result.errors.push_back("Image dimensions differ: expected " +
            std::to_string(expWidth) + "x" + std::to_string(expHeight) +
            ", actual " + std::to_string(actWidth) + "x" + std::to_string(actHeight));
        return result;
    }

    const int totalPixels = expWidth * expHeight;
    int diffCount = 0;
    int maxChannelDiff = 0;

    if (expImg->getFormat() != ci::ImageFormat::rgba8 || actImg->getFormat() != ci::ImageFormat::rgba8) {
        result.match = false;
        result.errors.push_back("Pixel comparison only supports RGBA8 images");
        return result;
    }

    const uint8_t* expData = static_cast<const uint8_t*>(expImg->getData());
    const uint8_t* actData = static_cast<const uint8_t*>(actImg->getData());

    for (int i = 0; i < totalPixels; ++i) {
        const int idx = i * 4;
        int dr = std::abs((int)expData[idx]     - (int)actData[idx]);
        int dg = std::abs((int)expData[idx + 1] - (int)actData[idx + 1]);
        int db = std::abs((int)expData[idx + 2] - (int)actData[idx + 2]);
        int da = std::abs((int)expData[idx + 3] - (int)actData[idx + 3]);
        int maxDiff = std::max({dr, dg, db, da});
        if (maxDiff > maxChannelDiff) maxChannelDiff = maxDiff;
        if (maxDiff > tolerance) ++diffCount;
    }

    result.pixelDifference = static_cast<double>(diffCount) / totalPixels * 100.0;
    result.maxChannelDiff  = static_cast<double>(maxChannelDiff) / 255.0;

    if (diffCount > 0) {
        result.match = false;
        result.errors.push_back(std::to_string(diffCount) + " of " +
            std::to_string(totalPixels) + " pixels (" +
            std::to_string(result.pixelDifference) + "%) exceed channel tolerance " +
            std::to_string(tolerance));
    }

    if (generateDiff) {
        std::filesystem::path diffPath = std::filesystem::path(getDiffDirectory()) /
            (std::filesystem::path(expectedPath).stem().string() + "_diff.png");
        result.diffImagePath = diffPath.string();
        generateVisualDiff(expectedPath, actualPath, result.diffImagePath, tolerance, result);
    }

    return result;
}
