// Integration test for Renderer::requestFrameCapture(): the frame is drawn a
// second time into an offscreen texture and saved as a PNG whose pixels match
// what was drawn. Needs a real GPU (BUILD_INTEGRATION_TESTS).

#include <gtest/gtest.h>
#include <campello_widgets/ui/renderer.hpp>
#include <campello_widgets/ui/render_box.hpp>
#include <campello_widgets/ui/paint_context.hpp>
#include <campello_widgets/ui/paint.hpp>
#include <campello_widgets/ui/draw_backend_factory.hpp>

#include <campello_gpu/device.hpp>
#include <campello_gpu/texture.hpp>
#include <campello_gpu/texture_view.hpp>
#include <campello_gpu/constants/texture_type.hpp>
#include <campello_gpu/constants/texture_usage.hpp>
#include <campello_gpu/constants/pixel_format.hpp>

#define STB_IMAGE_STATIC
#define STB_IMAGE_IMPLEMENTATION
#include "../third_party/stb_image.h"

#include <cstdio>
#include <filesystem>
#include <string>

namespace cw  = systems::leal::campello_widgets;
namespace gpu = systems::leal::campello_gpu;

namespace
{
    // Fills the whole viewport white, with a red square in the top-left quarter.
    class QuarterSquareBox : public cw::RenderBox
    {
    public:
        void performLayout() override { size_ = constraints_.constrain(cw::Size{64.0f, 64.0f}); }

        void performPaint(cw::PaintContext& context, const cw::Offset& offset) override
        {
            context.canvas().drawRect(cw::Rect::fromLTWH(offset.x, offset.y, size_.width, size_.height),
                                      cw::Paint::filled(cw::Color::white()));
            context.canvas().drawRect(cw::Rect::fromLTWH(offset.x, offset.y, size_.width / 2, size_.height / 2),
                                      cw::Paint::filled(cw::Color::red()));
        }
    };
} // namespace

TEST(FrameCapture, WritesThePresentedFrameAsPng)
{
    auto device = gpu::Device::createDefaultDevice(nullptr);
    if (!device) GTEST_SKIP() << "no GPU device";

    constexpr uint32_t kSize = 64;
    // The format a platform window would use; the capture must cope with BGRA.
    constexpr auto kFormat = gpu::PixelFormat::bgra8unorm;
    auto backend = cw::createDrawBackend(device, cw::Color::white(), kFormat);
    if (!backend) GTEST_SKIP() << "no draw backend on this platform";

    auto root = std::make_shared<QuarterSquareBox>();
    cw::Renderer renderer(device, root, cw::Color::white());
    renderer.setDrawBackend(std::move(backend));
    renderer.setDevicePixelRatio(1.0f);

    auto texture = device->createTexture(
        gpu::TextureType::tt2d, kFormat, kSize, kSize, 1, 1, 1,
        static_cast<gpu::TextureUsage>(static_cast<int>(gpu::TextureUsage::renderTarget) |
                                       static_cast<int>(gpu::TextureUsage::copySrc)));
    ASSERT_NE(texture, nullptr);
    auto view = texture->createView(kFormat, 1);
    ASSERT_NE(view, nullptr);

    const std::string path =
        (std::filesystem::temp_directory_path() / "campello_widgets_frame_capture_test.png").string();
    std::remove(path.c_str());

    renderer.requestFrameCapture(path);
    auto package = renderer.buildFrame(static_cast<float>(kSize), static_cast<float>(kSize));
    ASSERT_TRUE(package.has_value());
    package->target = view;
    ASSERT_TRUE(renderer.rasterFrame(*package));

    int w = 0, h = 0, channels = 0;
    unsigned char* pixels = stbi_load(path.c_str(), &w, &h, &channels, 3);
    ASSERT_NE(pixels, nullptr) << "no PNG written at " << path;
    EXPECT_EQ(w, static_cast<int>(kSize));
    EXPECT_EQ(h, static_cast<int>(kSize));

    auto rgb = [&](int x, int y) { return pixels + (static_cast<size_t>(y) * w + x) * 3; };
    const unsigned char* red   = rgb(kSize / 4, kSize / 4);
    const unsigned char* white = rgb(3 * kSize / 4, 3 * kSize / 4);
    // Red stays red (R/B not swapped for BGRA targets); white stays white.
    EXPECT_GT(red[0], 200);
    EXPECT_LT(red[1], 50);
    EXPECT_LT(red[2], 50);
    EXPECT_GT(white[0], 200);
    EXPECT_GT(white[1], 200);
    EXPECT_GT(white[2], 200);

    stbi_image_free(pixels);
    std::remove(path.c_str());
}
