#include <gtest/gtest.h>
#include <campello_widgets/campello_widgets.hpp>
#include <campello_widgets/linux/embedded_app.hpp>
#include <campello_widgets/widgets/colored_box.hpp>
#include <campello_widgets/ui/pointer_dispatcher.hpp>
#include <campello_widgets/ui/focus_manager.hpp>
#include <campello_widgets/ui/pointer_event.hpp>

#include <campello_gpu/device.hpp>
#include <campello_gpu/texture.hpp>
#include <campello_gpu/texture_view.hpp>
#include <campello_gpu/constants/texture_type.hpp>
#include <campello_gpu/constants/texture_usage.hpp>
#include <campello_gpu/constants/pixel_format.hpp>

#include <vector>

namespace cw = systems::leal::campello_widgets;
namespace gpu = systems::leal::campello_gpu;

// ---------------------------------------------------------------------------
// EmbeddedApp integration tests
// ---------------------------------------------------------------------------
// Require BUILD_INTEGRATION_TESTS (real GPU) -- unlike the universal
// Renderer tests, EmbeddedApp::renderFrame() actually rasters (build +
// raster combined, see its own doc comment), which needs a real Device and
// draw backend, not the nullptr device the universal Renderer tests get
// away with. Device::createDefaultDevice(nullptr) is headless -- no
// window/surface -- so this runs anywhere a Vulkan ICD is available,
// including CI's own lavapipe (software) environment; no display needed.
// ---------------------------------------------------------------------------

namespace {

gpu::TextureUsage combineUsage(gpu::TextureUsage a, gpu::TextureUsage b) {
    return static_cast<gpu::TextureUsage>(static_cast<int>(a) | static_cast<int>(b));
}

class EmbeddedAppTest : public ::testing::Test {
protected:
    void SetUp() override {
        device_ = gpu::Device::createDefaultDevice(nullptr);
        ASSERT_NE(device_, nullptr) << "no Vulkan device available in this environment";

        texture_ = device_->createTexture(
            gpu::TextureType::tt2d, gpu::PixelFormat::rgba8unorm,
            kWidth, kHeight, 1, 1, 1,
            combineUsage(gpu::TextureUsage::renderTarget, gpu::TextureUsage::copySrc));
        ASSERT_NE(texture_, nullptr);

        view_ = texture_->createView(gpu::PixelFormat::rgba8unorm);
        ASSERT_NE(view_, nullptr);
    }

    static constexpr uint32_t kWidth = 64;
    static constexpr uint32_t kHeight = 64;

    std::shared_ptr<gpu::Device> device_;
    std::shared_ptr<gpu::Texture> texture_;
    std::shared_ptr<gpu::TextureView> view_;
};

} // namespace

TEST_F(EmbeddedAppTest, FirstRenderFrameDrawsAndSubsequentIdleFrameDoesNot) {
    cw::EmbeddedApp app(
        device_, cw::mw<cw::ColoredBox>(cw::Color::white()),
        static_cast<float>(kWidth), static_cast<float>(kHeight));

    // paint_requested_ defaults true (see Renderer's own doc comment on
    // that field), so the very first frame must draw regardless of
    // whether anything has explicitly changed yet.
    EXPECT_TRUE(app.renderFrame(view_, static_cast<float>(kWidth), static_cast<float>(kHeight)));

    // Nothing changed since -- buildFrame() should report nothing to draw,
    // matching Renderer::buildFrame()'s std::nullopt-on-no-change contract.
    EXPECT_FALSE(app.renderFrame(view_, static_cast<float>(kWidth), static_cast<float>(kHeight)));
}

TEST_F(EmbeddedAppTest, RenderedPixelsMatchTheRootWidgetsColor) {
    cw::EmbeddedApp app(
        device_, cw::mw<cw::ColoredBox>(cw::Color::red()),
        static_cast<float>(kWidth), static_cast<float>(kHeight),
        cw::Color::red());
    ASSERT_TRUE(app.renderFrame(view_, static_cast<float>(kWidth), static_cast<float>(kHeight)));

    // Real pixel readback, not just "it didn't crash" -- confirms the
    // embedded tree's content actually reached the caller-provided
    // texture, the whole point of EmbeddedApp existing at all.
    std::vector<uint8_t> pixels(static_cast<size_t>(kWidth) * kHeight * 4);
    ASSERT_TRUE(texture_->download(0, 0, pixels.data(), pixels.size()));

    size_t center = (static_cast<size_t>(kHeight) / 2 * kWidth + kWidth / 2) * 4;
    EXPECT_GT(pixels[center + 0], 200) << "red channel";
    EXPECT_LT(pixels[center + 1], 50)  << "green channel";
    EXPECT_LT(pixels[center + 2], 50)  << "blue channel";
}

TEST_F(EmbeddedAppTest, ForceRefreshDrawsEvenWithNothingChanged) {
    cw::EmbeddedApp app(
        device_, cw::mw<cw::ColoredBox>(cw::Color::white()),
        static_cast<float>(kWidth), static_cast<float>(kHeight));
    ASSERT_TRUE(app.renderFrame(view_, static_cast<float>(kWidth), static_cast<float>(kHeight)));
    ASSERT_FALSE(app.renderFrame(view_, static_cast<float>(kWidth), static_cast<float>(kHeight)));

    app.forceRefresh();
    EXPECT_TRUE(app.renderFrame(view_, static_cast<float>(kWidth), static_cast<float>(kHeight)));
}

TEST_F(EmbeddedAppTest, TickAndInputForwardingDoNotCrash) {
    cw::EmbeddedApp app(
        device_, cw::mw<cw::ColoredBox>(cw::Color::white()),
        static_cast<float>(kWidth), static_cast<float>(kHeight));

    app.tick(1000);

    cw::PointerEvent e;
    e.kind = cw::PointerEventKind::move;
    e.pointer_id = 0;
    e.position = {static_cast<float>(kWidth) / 2.0f, static_cast<float>(kHeight) / 2.0f};
    app.pointerDispatcher().handlePointerEvent(e);

    SUCCEED();
}
