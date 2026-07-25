#include <gtest/gtest.h>
#include <campello_widgets/ui/renderer.hpp>
#include <campello_widgets/ui/render_box.hpp>
#include <campello_widgets/ui/box_constraints.hpp>
#include <campello_widgets/ui/paint_context.hpp>
#include <campello_widgets/ui/paint.hpp>
#include <campello_widgets/ui/color.hpp>
#include <campello_widgets/ui/rect.hpp>
#include <campello_widgets/ui/debug_flags.hpp>
#include <campello_widgets/ui/draw_command.hpp>

#include <variant>

namespace cw = systems::leal::campello_widgets;

namespace
{
    // A leaf RenderBox that always emits one draw command, so buildFrame()
    // never returns std::nullopt due to an empty draw list.
    class SimplePaintingBox : public cw::RenderBox
    {
    public:
        void performLayout() override
        {
            size_ = constraints_.constrain(cw::Size{50.0f, 50.0f});
        }

        void performPaint(cw::PaintContext& context, const cw::Offset& offset) override
        {
            context.canvas().drawRect(
                cw::Rect::fromLTWH(offset.x, offset.y, size_.width, size_.height),
                cw::Paint::filled(cw::Color::black()));
        }
    };
} // namespace

// This is the invariant the whole UI/raster thread split depends on:
// buildFrame() must snapshot device_pixel_ratio_/clear_color_ into the
// returned FramePackage *by value*, not let the raster phase read them
// live off the Renderer later — otherwise a UI-thread mutation between
// frame N's buildFrame() and frame N's rasterFrame() (on another thread)
// would retroactively change frame N's already-built package.
TEST(Renderer, BuildFrameSnapshotsDprAndClearColorIndependently)
{
    auto root = std::make_shared<SimplePaintingBox>();
    cw::Renderer renderer(/*device=*/nullptr, root, cw::Color::white());

    renderer.setDevicePixelRatio(1.0f);
    renderer.setClearColor(cw::Color::white());

    auto pkg1 = renderer.buildFrame(100.0f, 100.0f);
    ASSERT_TRUE(pkg1.has_value());
    EXPECT_FLOAT_EQ(pkg1->device_pixel_ratio, 1.0f);
    EXPECT_FLOAT_EQ(pkg1->clear_color.r, 1.0f);
    EXPECT_FLOAT_EQ(pkg1->clear_color.g, 1.0f);
    EXPECT_FLOAT_EQ(pkg1->clear_color.b, 1.0f);

    // Mutate Renderer's live state as if the UI thread had moved on to
    // preparing the next frame while pkg1 is (hypothetically) still being
    // rastered elsewhere — pkg1 must be completely unaffected.
    renderer.setDevicePixelRatio(2.0f);
    renderer.setClearColor(cw::Color::black());

    EXPECT_FLOAT_EQ(pkg1->device_pixel_ratio, 1.0f);
    EXPECT_FLOAT_EQ(pkg1->clear_color.r, 1.0f);

    auto pkg2 = renderer.buildFrame(100.0f, 100.0f);
    ASSERT_TRUE(pkg2.has_value());
    EXPECT_FLOAT_EQ(pkg2->device_pixel_ratio, 2.0f);
    EXPECT_FLOAT_EQ(pkg2->clear_color.r, 0.0f);
    EXPECT_FLOAT_EQ(pkg2->clear_color.g, 0.0f);
    EXPECT_FLOAT_EQ(pkg2->clear_color.b, 0.0f);
}

// Covers setDisplayRefreshHz()'s contract: the performance overlay's
// budget line (paintPerformanceOverlay()/paintUnifiedFrameChart()) reads
// this instead of a hardcoded 60Hz assumption, so a window dragged onto a
// higher-refresh-rate display (e.g. a 144Hz external monitor on macOS)
// shows a budget line that reflects that display's actual frame budget.
// Defaults to 60Hz so platforms that never call this see unchanged
// behavior; clamped to a sane range so a bogus value (e.g. 0, from a
// platform query gone wrong) can't divide-by-zero the budget-line math.
TEST(Renderer, SetDisplayRefreshHzDefaultsAndClamps)
{
    auto root = std::make_shared<SimplePaintingBox>();
    cw::Renderer renderer(/*device=*/nullptr, root, cw::Color::white());

    EXPECT_FLOAT_EQ(renderer.displayRefreshHz(), 60.0f)
        << "must default to 60Hz for platforms that never call setDisplayRefreshHz()";

    renderer.setDisplayRefreshHz(144.0f);
    EXPECT_FLOAT_EQ(renderer.displayRefreshHz(), 144.0f);

    renderer.setDisplayRefreshHz(0.0f);
    EXPECT_FLOAT_EQ(renderer.displayRefreshHz(), 1.0f)
        << "must clamp to a sane minimum, not divide-by-zero the budget line";

    renderer.setDisplayRefreshHz(5000.0f);
    EXPECT_FLOAT_EQ(renderer.displayRefreshHz(), 1000.0f)
        << "must clamp to a sane maximum";
}

TEST(Renderer, BuildFrameReturnsNulloptWhenNotDirty)
{
    auto root = std::make_shared<SimplePaintingBox>();
    cw::Renderer renderer(/*device=*/nullptr, root, cw::Color::white());

    auto pkg1 = renderer.buildFrame(100.0f, 100.0f);
    ASSERT_TRUE(pkg1.has_value());

    // Nothing changed since the last build — root is no longer paint-dirty.
    auto pkg2 = renderer.buildFrame(100.0f, 100.0f);
    EXPECT_FALSE(pkg2.has_value());
}

namespace
{
    // RAII guard so a test that flips a global DebugFlags switch can't leak
    // that state into whichever test happens to run next.
    struct ScopedPerformanceOverlay
    {
        bool previous;
        explicit ScopedPerformanceOverlay(bool enabled)
            : previous(cw::DebugFlags::showPerformanceOverlay)
        {
            cw::DebugFlags::showPerformanceOverlay = enabled;
        }
        ~ScopedPerformanceOverlay()
        {
            cw::DebugFlags::showPerformanceOverlay = previous;
        }
    };
} // namespace

// Covers the unified frame chart added to replace the old two-lane overlay
// (matching Flutter DevTools' "Flutter frames chart": one shared chart with
// a UI bar + a raster bar per frame, rather than two separate panels).
// Can't verify pixel-level layout here (no GPU/screenshot in this test
// environment — see project notes), but this exercises the real code path
// end-to-end via buildFrame() and checks the resulting DrawList is sane:
// one label naming both phases, and one bar per recorded UI sample (no
// raster samples exist here since rasterFrame() is never called without a
// real Device).
TEST(Renderer, PerformanceOverlayEmitsUnifiedChartDrawCommands)
{
    ScopedPerformanceOverlay overlay_guard(true);

    auto root = std::make_shared<SimplePaintingBox>();
    cw::Renderer renderer(/*device=*/nullptr, root, cw::Color::white());

    constexpr int kFrames = 5;
    std::optional<cw::FramePackage> last_pkg;
    for (int i = 0; i < kFrames; ++i)
    {
        root->markNeedsPaint();
        last_pkg = renderer.buildFrame(200.0f, 200.0f);
        ASSERT_TRUE(last_pkg.has_value());
    }

    bool found_label = false;
    int  rect_count  = 0;
    for (const auto& cmd : last_pkg->draw_list)
    {
        if (const auto* text = std::get_if<cw::DrawTextCmd>(&cmd))
        {
            if (text->span.text.find("UI:") != std::string::npos &&
                text->span.text.find("RASTER:") != std::string::npos)
                found_label = true;
            if (text->span.text.find("FPS:") != std::string::npos)
            {
                EXPECT_NE(text->span.text.find("FPS: --"), std::string::npos)
                    << "No rasterFrame() ever ran in this test, so the real-FPS "
                       "sampler must still read as empty ('--'), not a bogus 0";
            }
        }
        else if (std::get_if<cw::DrawRectCmd>(&cmd))
        {
            ++rect_count;
        }
    }

    EXPECT_TRUE(found_label) << "Expected one label naming both UI and RASTER phases";

    // paintPerformanceOverlay() reads build_sampler_ before the current
    // frame's own duration is recorded (intentional — the overlay is one
    // frame stale, same as the original two-lane design), so the last of
    // kFrames builds only sees (kFrames - 1) prior UI samples.
    // Box content (1) + overlay background (1) + (kFrames-1) UI bars
    // (raster bars are 0 since no rasterFrame() ever ran) + 1 budget line.
    EXPECT_EQ(rect_count, 1 + 1 + (kFrames - 1) + 1);
}

// Covers the exact scenario that motivated present_fps_sampler_: this is an
// on-demand renderer (see FrameScheduler), so after being idle for a while
// the next presented frame is naturally far apart from the last one. A
// naive cadence sampler would store that gap as a single nonsensical
// "instant fps" sample (e.g. one frame after 3s idle ≈ 0.3fps) and drag the
// rolling average down for a while right as a fresh animation starts.
// Renderer::recordPresentSample() is a static, dependency-free helper
// (no `this`) precisely so this can be tested with synthetic timestamps —
// rasterFrame() itself can't be exercised here without a real Device/GPU.
TEST(Renderer, PresentFpsSamplerResetsAcrossIdleGap)
{
    systems::leal::campello_gpu::FrameTimeSampler sampler;
    uint64_t                       last_ms = 0;

    // A burst of frames at a steady cadence, 16ms apart (~62.5fps).
    cw::Renderer::recordPresentSample(sampler, last_ms, 1000, /*idle_gap_reset_ms=*/200);
    cw::Renderer::recordPresentSample(sampler, last_ms, 1016, 200);
    cw::Renderer::recordPresentSample(sampler, last_ms, 1032, 200);
    ASSERT_EQ(sampler.count(), 2);
    EXPECT_NEAR(1000.0f / sampler.averageMs(), 62.5f, 1.0f);

    // A 3-second idle gap — nothing requested a frame in between.
    cw::Renderer::recordPresentSample(sampler, last_ms, 4032, 200);
    // The gap must never be recorded as a sample: this call only
    // re-establishes the baseline (mirrors FrameTimeSampler::record()'s
    // "first call stores nothing" contract), so count() drops back to 0
    // instead of holding a bogus ~0.3fps reading.
    EXPECT_EQ(sampler.count(), 0);

    // The burst resumes at a normal cadence; the very next sample is
    // measured fresh from the reset baseline, not across the idle gap.
    cw::Renderer::recordPresentSample(sampler, last_ms, 4048, 200);
    ASSERT_EQ(sampler.count(), 1);
    EXPECT_NEAR(1000.0f / sampler.averageMs(), 62.5f, 1.0f); // 16ms delta
}

// An ordinary slow/janky frame (still well under the idle-gap threshold)
// must NOT trigger a reset — only a genuine "nothing was requested for a
// while" gap should. Otherwise every dropped frame would also wipe the
// rolling average, defeating its purpose.
TEST(Renderer, PresentFpsSamplerDoesNotResetForOrdinaryJank)
{
    systems::leal::campello_gpu::FrameTimeSampler sampler;
    uint64_t                       last_ms = 0;

    cw::Renderer::recordPresentSample(sampler, last_ms, 1000, /*idle_gap_reset_ms=*/200);
    cw::Renderer::recordPresentSample(sampler, last_ms, 1016, 200);  // 16ms, normal
    cw::Renderer::recordPresentSample(sampler, last_ms, 1150, 200);  // 134ms jank, still < 200ms

    EXPECT_EQ(sampler.count(), 2) << "Both deltas should be kept — no reset for ordinary jank";
}
