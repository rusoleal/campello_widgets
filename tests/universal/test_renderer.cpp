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
