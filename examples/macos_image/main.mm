#include <campello_widgets/campello_widgets.hpp>
#include <campello_widgets/macos/run_app.hpp>

namespace cw = systems::leal::campello_widgets;

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "../../tests/third_party/stb_image_write.h"

static std::vector<uint8_t> createTestPNG()
{
    const int width = 100, height = 100;
    std::vector<uint8_t> pixels(width * height * 4);
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            int idx = (y * width + x) * 4;
            pixels[idx + 0] = 255;  // R
            pixels[idx + 1] = 0;    // G
            pixels[idx + 2] = 0;    // B
            pixels[idx + 3] = 255;  // A
        }
    }
    int png_size = 0;
    unsigned char* png = stbi_write_png_to_mem(pixels.data(), width * 4, width, height, 4, &png_size);
    std::vector<uint8_t> result(png, png + png_size);
    free(png);
    return result;
}

int main()
{
    // NO debug banner to avoid confusion
    cw::DebugFlags::showDebugBanner = false;
    cw::DebugFlags::showPerformanceOverlay = true;
    cw::DebugFlags::paintSizeEnabled = true;

    // Initialize loader
    cw::ImageLoader::instance().initialize(4);
    
    auto bytes = createTestPNG();
    
    // Create image widget with explicit size
    auto image = cw::ImageWidget::memory(
        bytes,
        cw::BoxFit::fill,
        100.0f, 100.0f
    );
    
    // Simple fixed-size container - NO scrolling
    auto box = std::make_shared<cw::Container>();
    box->width = 150;
    box->height = 150;
    box->color = cw::Color::fromRGB(0, 0, 1);  // Blue background
    box->child = image;
    
    // Center the box
    auto root = cw::Center::create(box);
    
    return cw::runApp(root, "Image Test", 400, 400);
}
