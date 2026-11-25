// Integration test for RenderVideoPlayer::copyExternalFrame(): a decoded
// NV12 frame copied on the GPU (campello_gpu's copyExternalImageToTexture())
// must look like the same frame decoded to BGRA and uploaded from the CPU
// (uploadFrame()), which is what VideoPlayerController fell back to before.
// Needs a real GPU (BUILD_INTEGRATION_TESTS) and the gallery's sample video.

#import <AVFoundation/AVFoundation.h>
#import <CoreVideo/CoreVideo.h>

#include <gtest/gtest.h>
#include <campello_widgets/ui/render_video_player.hpp>
#include <campello_widgets/ui/draw_backend.hpp>
#include <campello_widgets/ui/draw_backend_factory.hpp>
#include <campello_widgets/ui/renderer.hpp>

#include <campello_gpu/device.hpp>
#include <campello_gpu/texture.hpp>
#include <campello_gpu/texture_view.hpp>
#include <campello_gpu/constants/texture_type.hpp>
#include <campello_gpu/constants/texture_usage.hpp>

#include <cstdlib>
#include <cstring>
#include <vector>

namespace cw  = systems::leal::campello_widgets;
namespace gpu = systems::leal::campello_gpu;

namespace
{
    // First video frame of `path`, as a CVPixelBuffer of `format` (+1
    // retained; the caller releases it).
    CVPixelBufferRef firstFrame(const char* path, OSType format)
    {
        @autoreleasepool
        {
            NSURL* url = [NSURL fileURLWithPath:[NSString stringWithUTF8String:path]];
            AVURLAsset* asset = [AVURLAsset URLAssetWithURL:url options:nil];
            NSError* error = nil;
            AVAssetReader* reader = [AVAssetReader assetReaderWithAsset:asset error:&error];
            if (!reader) return nullptr;
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
            NSArray<AVAssetTrack*>* tracks = [asset tracksWithMediaType:AVMediaTypeVideo];
#pragma clang diagnostic pop
            if (tracks.count == 0) return nullptr;
            NSDictionary* attrs = @{
                (NSString*)kCVPixelBufferPixelFormatTypeKey : @(format),
                (NSString*)kCVPixelBufferMetalCompatibilityKey : @YES,
            };
            AVAssetReaderTrackOutput* output =
                [AVAssetReaderTrackOutput assetReaderTrackOutputWithTrack:tracks[0]
                                                           outputSettings:attrs];
            [reader addOutput:output];
            if (![reader startReading]) return nullptr;
            CMSampleBufferRef sample = [output copyNextSampleBuffer];
            if (!sample) return nullptr;
            CVPixelBufferRef pixels = CMSampleBufferGetImageBuffer(sample);
            if (pixels) CVPixelBufferRetain(pixels);
            CFRelease(sample);
            [reader cancelReading];
            return pixels;
        }
    }

    std::vector<uint8_t> tightlyPacked(CVPixelBufferRef bgra)
    {
        CVPixelBufferLockBaseAddress(bgra, kCVPixelBufferLock_ReadOnly);
        const size_t w = CVPixelBufferGetWidth(bgra), h = CVPixelBufferGetHeight(bgra);
        const size_t stride = CVPixelBufferGetBytesPerRow(bgra);
        const auto* base = static_cast<const uint8_t*>(CVPixelBufferGetBaseAddress(bgra));
        std::vector<uint8_t> out(w * h * 4);
        for (size_t y = 0; y < h; ++y) std::memcpy(out.data() + y * w * 4, base + y * stride, w * 4);
        CVPixelBufferUnlockBaseAddress(bgra, kCVPixelBufferLock_ReadOnly);
        return out;
    }

    std::vector<uint8_t> readback(gpu::Texture* t)
    {
        std::vector<uint8_t> pixels(size_t(t->getWidth()) * t->getHeight() * 4);
        EXPECT_TRUE(t->download(0, 0, pixels.data(), pixels.size()));
        return pixels;
    }

    // `a` (converted on the GPU) against `b` (AVFoundation's BGRA).
    void expectSameFrame(const std::vector<uint8_t>& a, const std::vector<uint8_t>& b, uint32_t w, uint32_t h)
    {
        ASSERT_EQ(a.size(), b.size());
        // Both decode the same samples; only YCbCr→RGB rounding (and chroma
        // filtering at edges) differs between AVFoundation's converter and ours.
        long long sum = 0;
        int worst = 0;
        size_t over = 0;
        for (size_t i = 0; i < a.size(); ++i)
        {
            if (i % 4 == 3) continue;   // alpha
            const int d = std::abs(int(a[i]) - int(b[i]));
            sum += d;
            worst = std::max(worst, d);
            if (d > 6) ++over;
        }
        const double mean = double(sum) / (a.size() / 4 * 3);
        // A blank frame would match whatever the conversion did.
        int lo = 255, hi = 0;
        for (size_t i = 0; i < b.size(); ++i)
            if (i % 4 != 3) { lo = std::min(lo, int(b[i])); hi = std::max(hi, int(b[i])); }
        ASSERT_GT(hi - lo, 64) << "sample frame has no contrast";
        std::printf("%ux%u: mean diff %.3f, worst %d, >6: %zu, range %d..%d\n", w, h, mean, worst, over, lo, hi);
        EXPECT_LT(mean, 1.5) << "worst " << worst << ", " << over << " samples off by more than 6";
        EXPECT_LT(double(over) / (a.size() / 4 * 3), 0.01) << "worst " << worst;
    }

    struct ActiveBackend
    {
        explicit ActiveBackend(cw::IDrawBackend* b) { cw::RenderObject::setActiveBackend(b); }
        ~ActiveBackend() { cw::RenderObject::setActiveBackend(nullptr); }
    };
} // namespace

TEST(VideoFrames, ZeroCopyFrameMatchesTheCpuUpload)
{
    const char* video = CAMPELLO_WIDGETS_SAMPLE_VIDEO;
    auto device = gpu::Device::createDefaultDevice(nullptr);
    if (!device) GTEST_SKIP() << "no GPU device";
    auto backend = cw::createDrawBackend(device, cw::Color::white(), gpu::PixelFormat::bgra8unorm);
    if (!backend) GTEST_SKIP() << "no draw backend on this platform";
    ActiveBackend active(backend.get());
    if (!cw::RenderVideoPlayer::supportsExternalFrames(gpu::ExternalImageType::cvPixelBuffer))
        GTEST_SKIP() << "device can't import CVPixelBuffers";

    CVPixelBufferRef nv12 = firstFrame(video, kCVPixelFormatType_420YpCbCr8BiPlanarVideoRange);
    CVPixelBufferRef bgra = firstFrame(video, kCVPixelFormatType_32BGRA);
    ASSERT_NE(nv12, nullptr) << video;
    ASSERT_NE(bgra, nullptr) << video;
    const uint32_t w = uint32_t(CVPixelBufferGetWidth(nv12)), h = uint32_t(CVPixelBufferGetHeight(nv12));
    ASSERT_EQ(w, CVPixelBufferGetWidth(bgra));
    ASSERT_EQ(h, CVPixelBufferGetHeight(bgra));

    cw::RenderVideoPlayer gpuPlayer(nullptr), cpuPlayer(nullptr);
    gpu::ExternalImage frame;
    frame.type   = gpu::ExternalImageType::cvPixelBuffer;
    frame.handle = nv12;
    ASSERT_TRUE(gpuPlayer.copyExternalFrame(w, h, frame));
    CVPixelBufferRelease(nv12);   // campello_gpu keeps its own reference
    const auto packed = tightlyPacked(bgra);
    CVPixelBufferRelease(bgra);
    cpuPlayer.uploadFrame(w, h, packed.data());

    ASSERT_NE(gpuPlayer.texture(), nullptr);
    ASSERT_NE(cpuPlayer.texture(), nullptr);
    expectSameFrame(readback(gpuPlayer.texture()), readback(cpuPlayer.texture()), w, h);
}

// The path players use: queued, then recorded at the start of the next
// rasterized frame.
TEST(VideoFrames, QueuedFrameLandsWithTheNextRasterizedFrame)
{
    const char* video = CAMPELLO_WIDGETS_SAMPLE_VIDEO;
    auto device = gpu::Device::createDefaultDevice(nullptr);
    if (!device) GTEST_SKIP() << "no GPU device";
    constexpr auto kFormat = gpu::PixelFormat::bgra8unorm;
    auto backend = cw::createDrawBackend(device, cw::Color::white(), kFormat);
    if (!backend) GTEST_SKIP() << "no draw backend on this platform";
    ActiveBackend active(backend.get());
    if (!cw::RenderVideoPlayer::supportsExternalFrames(gpu::ExternalImageType::cvPixelBuffer))
        GTEST_SKIP() << "device can't import CVPixelBuffers";

    auto player = std::make_shared<cw::RenderVideoPlayer>(nullptr);
    cw::Renderer renderer(device, player, cw::Color::white());   // the current renderer
    auto* backend_ptr = backend.get();
    renderer.setDrawBackend(std::move(backend));
    cw::RenderObject::setActiveBackend(backend_ptr);
    renderer.setDevicePixelRatio(1.0f);

    CVPixelBufferRef nv12 = firstFrame(video, kCVPixelFormatType_420YpCbCr8BiPlanarVideoRange);
    CVPixelBufferRef bgra = firstFrame(video, kCVPixelFormatType_32BGRA);
    ASSERT_NE(nv12, nullptr) << video;
    ASSERT_NE(bgra, nullptr) << video;
    const uint32_t w = uint32_t(CVPixelBufferGetWidth(nv12)), h = uint32_t(CVPixelBufferGetHeight(nv12));

    gpu::ExternalImage frame;
    frame.type   = gpu::ExternalImageType::cvPixelBuffer;
    frame.handle = nv12;
    std::shared_ptr<void> keep_alive(nv12, [](void* p) { CVPixelBufferRelease(static_cast<CVPixelBufferRef>(p)); });
    std::weak_ptr<void> kept = keep_alive;
    ASSERT_TRUE(player->queueExternalFrame(w, h, frame, {}, std::move(keep_alive)));
    EXPECT_FALSE(kept.expired()) << "released before the copy was recorded";

    auto target = device->createTexture(
        gpu::TextureType::tt2d, kFormat, 64, 64, 1, 1, 1,
        static_cast<gpu::TextureUsage>(static_cast<int>(gpu::TextureUsage::renderTarget) |
                                       static_cast<int>(gpu::TextureUsage::copySrc)));
    ASSERT_NE(target, nullptr);
    auto package = renderer.buildFrame(64.0f, 64.0f);
    ASSERT_TRUE(package.has_value());
    EXPECT_EQ(package->gpu_uploads.size(), 1u);
    package->target = target->createView(kFormat, 1);
    ASSERT_TRUE(renderer.rasterFrame(*package));
    package.reset();
    EXPECT_TRUE(kept.expired()) << "kept after the copy was recorded";

    ASSERT_NE(player->texture(), nullptr);
    const auto packed = tightlyPacked(bgra);
    CVPixelBufferRelease(bgra);
    expectSameFrame(readback(player->texture()), packed, w, h);
}
