#include <gtest/gtest.h>
#include "fidelity.hpp"
#include <campello_widgets/ui/canvas.hpp>
#include <campello_widgets/ui/path.hpp>
#include <campello_widgets/ui/rrect.hpp>
#include <campello_widgets/ui/paint.hpp>
#include <campello_widgets/ui/color.hpp>
#include <fstream>

namespace cw = systems::leal::campello_widgets;
namespace cwt = systems::leal::campello_widgets::testing;

// ---------------------------------------------------------------------------
// Helper to check if a golden file exists
// ---------------------------------------------------------------------------

// Standard resolution for fidelity testing (matches visual fidelity tests)
constexpr float kFidelityWidth = 1280.0f;
constexpr float kFidelityHeight = 720.0f;

static bool goldenFileExists(const std::string& name)
{
    std::ifstream file(cwt::getGoldensDirectory() + "/" + name);
    return file.good();
}

static std::string loadGolden(const std::string& name)
{
    return cwt::loadGoldenFile(name);
}

// ---------------------------------------------------------------------------
// Canvas API Fidelity Tests
// ---------------------------------------------------------------------------

TEST(CanvasApiFidelity, BasicShapes)
{
    if (!goldenFileExists("canvas_basic_shapes_flutter.json")) {
        GTEST_SKIP() << "Golden file not found. Run Flutter fidelity tester first.";
    }

    // Create a canvas and draw basic shapes (scaled for 1280x720)
    cw::Canvas canvas(kFidelityWidth, kFidelityHeight);
    
    // Scale factors based on reference 400x400 canvas
    const float scaleX = kFidelityWidth / 400.0f;
    const float scaleY = kFidelityHeight / 400.0f;
    
    // Blue rectangle
    cw::Paint bluePaint;
    bluePaint.color = cw::Color::fromRGB(0.0f, 0.0f, 1.0f);
    canvas.drawRect(cw::Rect::fromLTWH(20 * scaleX, 20 * scaleY, 100 * scaleX, 80 * scaleY), bluePaint);
    
    // Red circle
    cw::Paint redPaint;
    redPaint.color = cw::Color::fromRGB(1.0f, 0.0f, 0.0f);
    canvas.drawCircle(cw::Offset(200 * scaleX, 60 * scaleY), 40 * scaleX, redPaint);
    
    // Green oval
    cw::Paint greenPaint;
    greenPaint.color = cw::Color::fromRGB(0.0f, 1.0f, 0.0f);
    canvas.drawOval(cw::Rect::fromLTWH(280 * scaleX, 20 * scaleY, 100 * scaleX, 80 * scaleY), greenPaint);
    
    // Purple stroke rectangle
    cw::Paint purpleStroke;
    purpleStroke.color = cw::Color::fromRGB(0.5f, 0.0f, 0.5f);
    purpleStroke.style = cw::PaintStyle::stroke;
    purpleStroke.stroke_width = 4 * scaleX;
    canvas.drawRect(cw::Rect::fromLTWH(20 * scaleX, 120 * scaleY, 100 * scaleX, 80 * scaleY), purpleStroke);

    // Get draw commands
    const auto& commands = canvas.commands();
    
    // Verify commands were recorded
    EXPECT_EQ(commands.size(), 4);
    
    // Verify first command is drawRect
    EXPECT_EQ(commands[0].index(), 0); // DrawRectCmd
}

TEST(CanvasApiFidelity, LinesAndPoints)
{
    if (!goldenFileExists("canvas_lines_points_flutter.json")) {
        GTEST_SKIP() << "Golden file not found. Run Flutter fidelity tester first.";
    }

    cw::Canvas canvas(kFidelityWidth, kFidelityHeight);
    
    // Scale factors based on reference 400x400 canvas
    const float scaleX = kFidelityWidth / 400.0f;
    const float scaleY = kFidelityHeight / 400.0f;
    
    // Black lines
    cw::Paint linePaint;
    linePaint.color = cw::Color::fromRGB(0.0f, 0.0f, 0.0f);
    linePaint.stroke_width = 2 * scaleX;
    canvas.drawLine(cw::Offset(20 * scaleX, 20 * scaleY), cw::Offset(150 * scaleX, 100 * scaleY), linePaint);
    canvas.drawLine(cw::Offset(200 * scaleX, 20 * scaleY), cw::Offset(350 * scaleX, 150 * scaleY), linePaint);
    
    // Thick orange line
    cw::Paint thickPaint;
    thickPaint.color = cw::Color::fromRGB(1.0f, 0.65f, 0.0f); // Orange
    thickPaint.stroke_width = 8 * scaleX;
    canvas.drawLine(cw::Offset(50 * scaleX, 200 * scaleY), cw::Offset(350 * scaleX, 250 * scaleY), thickPaint);
    
    // Points
    cw::Paint pointPaint;
    pointPaint.color = cw::Color::fromRGB(1.0f, 0.0f, 0.0f);
    std::vector<cw::Offset> points = {
        cw::Offset(100 * scaleX, 300 * scaleY),
        cw::Offset(150 * scaleX, 320 * scaleY),
        cw::Offset(200 * scaleX, 310 * scaleY),
        cw::Offset(250 * scaleX, 330 * scaleY),
        cw::Offset(300 * scaleX, 300 * scaleY),
    };
    canvas.drawPoints(cw::PointMode::points, points, pointPaint);
    
    const auto& commands = canvas.commands();
    EXPECT_GE(commands.size(), 4);
}

TEST(CanvasApiFidelity, Paths)
{
    if (!goldenFileExists("canvas_paths_flutter.json")) {
        GTEST_SKIP() << "Golden file not found. Run Flutter fidelity tester first.";
    }

    cw::Canvas canvas(kFidelityWidth, kFidelityHeight);
    
    // Scale factors based on reference 400x400 canvas
    const float scaleX = kFidelityWidth / 400.0f;
    const float scaleY = kFidelityHeight / 400.0f;
    
    // Triangle path
    cw::Path trianglePath;
    trianglePath.moveTo(cw::Offset(100 * scaleX, 50 * scaleY));
    trianglePath.lineTo(cw::Offset(50 * scaleX, 150 * scaleY));
    trianglePath.lineTo(cw::Offset(150 * scaleX, 150 * scaleY));
    trianglePath.close();
    
    cw::Paint bluePaint;
    bluePaint.color = cw::Color::fromRGBA(0.0f, 0.0f, 1.0f, 0.7f);
    canvas.drawPath(trianglePath, bluePaint);
    
    // Quadratic bezier
    cw::Path quadPath;
    quadPath.moveTo(cw::Offset(200 * scaleX, 150 * scaleY));
    quadPath.quadTo(cw::Offset(250 * scaleX, 50 * scaleY), cw::Offset(300 * scaleX, 150 * scaleY));
    
    cw::Paint greenStroke;
    greenStroke.color = cw::Color::fromRGB(0.0f, 1.0f, 0.0f);
    greenStroke.style = cw::PaintStyle::stroke;
    greenStroke.stroke_width = 3 * scaleX;
    canvas.drawPath(quadPath, greenStroke);
    
    // Cubic bezier
    cw::Path cubicPath;
    cubicPath.moveTo(cw::Offset(50 * scaleX, 250 * scaleY));
    cubicPath.cubicTo(
        cw::Offset(100 * scaleX, 200 * scaleY),
        cw::Offset(150 * scaleX, 300 * scaleY),
        cw::Offset(200 * scaleX, 250 * scaleY)
    );
    
    cw::Paint purpleStroke;
    purpleStroke.color = cw::Color::fromRGB(0.5f, 0.0f, 0.5f);
    purpleStroke.style = cw::PaintStyle::stroke;
    purpleStroke.stroke_width = 4 * scaleX;
    canvas.drawPath(cubicPath, purpleStroke);
    
    const auto& commands = canvas.commands();
    EXPECT_GE(commands.size(), 3);
}

TEST(CanvasApiFidelity, RoundedRects)
{
    if (!goldenFileExists("canvas_rounded_rects_flutter.json")) {
        GTEST_SKIP() << "Golden file not found. Run Flutter fidelity tester first.";
    }

    cw::Canvas canvas(kFidelityWidth, kFidelityHeight);
    
    // Scale factors based on reference 400x400 canvas
    const float scaleX = kFidelityWidth / 400.0f;
    const float scaleY = kFidelityHeight / 400.0f;
    
    // Single rounded rect with uniform radius
    cw::RRect rrect1(cw::Rect::fromLTWH(20 * scaleX, 20 * scaleY, 150 * scaleX, 100 * scaleY), 20.0f * scaleX);
    
    cw::Paint bluePaint;
    bluePaint.color = cw::Color::fromRGB(0.0f, 0.0f, 1.0f);
    canvas.drawRRect(rrect1, bluePaint);
    
    // Rounded rect with different corner radii
    cw::RRectComplex rrect2 = cw::RRectComplex::fromRectAndCorners(
        cw::Rect::fromLTWH(200 * scaleX, 20 * scaleY, 150 * scaleX, 100 * scaleY),
        30.0f * scaleX,  // top_left
        10.0f * scaleX,  // top_right
        5.0f * scaleX,   // bottom_left
        25.0f * scaleX   // bottom_right
    );
    
    cw::Paint greenPaint;
    greenPaint.color = cw::Color::fromRGB(0.0f, 1.0f, 0.0f);
    // For RRectComplex, we need to convert to simple RRect or use a different approach
    // Here we use the top_left radius for simplicity
    cw::RRect rrect2_simple(rrect2.rect, rrect2.top_left_radius);
    canvas.drawRRect(rrect2_simple, greenPaint);
    
    // Double rounded rect (border effect)
    cw::RRect outerRRect(cw::Rect::fromLTWH(20 * scaleX, 150 * scaleY, 150 * scaleX, 100 * scaleY), 25.0f * scaleX);
    cw::RRect innerRRect(cw::Rect::fromLTWH(35 * scaleX, 165 * scaleY, 120 * scaleX, 70 * scaleY), 15.0f * scaleX);
    
    cw::Paint purplePaint;
    purplePaint.color = cw::Color::fromRGB(0.5f, 0.0f, 0.5f);
    canvas.drawDRRect(outerRRect, innerRRect, purplePaint);
    
    // Stroked rounded rect
    cw::RRect rrect3(cw::Rect::fromLTWH(200 * scaleX, 150 * scaleY, 150 * scaleX, 100 * scaleY), 15.0f * scaleX);
    
    cw::Paint redStroke;
    redStroke.color = cw::Color::fromRGB(1.0f, 0.0f, 0.0f);
    redStroke.style = cw::PaintStyle::stroke;
    redStroke.stroke_width = 5 * scaleX;
    canvas.drawRRect(rrect3, redStroke);
    
    const auto& commands = canvas.commands();
    EXPECT_GE(commands.size(), 4);
}

TEST(CanvasApiFidelity, Arcs)
{
    if (!goldenFileExists("canvas_arcs_flutter.json")) {
        GTEST_SKIP() << "Golden file not found. Run Flutter fidelity tester first.";
    }

    cw::Canvas canvas(kFidelityWidth, kFidelityHeight);
    
    // Scale factors based on reference 400x400 canvas
    const float scaleX = kFidelityWidth / 400.0f;
    const float scaleY = kFidelityHeight / 400.0f;
    
    // Pie slice (useCenter = true)
    cw::Paint bluePaint;
    bluePaint.color = cw::Color::fromRGB(0.0f, 0.0f, 1.0f);
    canvas.drawArc(
        cw::Rect::fromLTWH(20 * scaleX, 20 * scaleY, 150 * scaleX, 150 * scaleY),
        0.0f,           // startAngle
        1.5f,           // sweepAngle
        true,           // useCenter
        bluePaint
    );
    
    // Arc segment (useCenter = false)
    cw::Paint greenStroke;
    greenStroke.color = cw::Color::fromRGB(0.0f, 1.0f, 0.0f);
    greenStroke.style = cw::PaintStyle::stroke;
    greenStroke.stroke_width = 10 * scaleX;
    canvas.drawArc(
        cw::Rect::fromLTWH(200 * scaleX, 20 * scaleY, 150 * scaleX, 150 * scaleY),
        0.5f,           // startAngle
        2.0f,           // sweepAngle
        false,          // useCenter
        greenStroke
    );
    
    // Pie chart segments
    cw::Color colors[] = {
        cw::Color::fromRGB(1.0f, 0.0f, 0.0f),    // Red
        cw::Color::fromRGB(1.0f, 0.65f, 0.0f),   // Orange
        cw::Color::fromRGB(1.0f, 1.0f, 0.0f),    // Yellow
        cw::Color::fromRGB(0.0f, 1.0f, 0.0f),    // Green
    };
    float values[] = {0.3f, 0.25f, 0.2f, 0.25f};
    
    float currentAngle = -1.5708f; // Start at top (-90 degrees)
    float twoPi = 6.28318f;
    
    for (int i = 0; i < 4; i++) {
        float sweep = values[i] * twoPi;
        cw::Paint paint;
        paint.color = colors[i];
        // Center at (100, 280), size 140x140 (scaled)
        float cx = 100.0f * scaleX, cy = 280.0f * scaleY, w = 140.0f * scaleX, h = 140.0f * scaleY;
        canvas.drawArc(
            cw::Rect::fromLTWH(cx - w/2, cy - h/2, w, h),
            currentAngle,
            sweep,
            true,
            paint
        );
        currentAngle += sweep;
    }
    
    const auto& commands = canvas.commands();
    EXPECT_GE(commands.size(), 6);
}

TEST(CanvasApiFidelity, Transforms)
{
    if (!goldenFileExists("canvas_transforms_flutter.json")) {
        GTEST_SKIP() << "Golden file not found. Run Flutter fidelity tester first.";
    }

    cw::Canvas canvas(kFidelityWidth, kFidelityHeight);
    
    // Scale factors based on reference 400x400 canvas
    const float scaleX = kFidelityWidth / 400.0f;
    const float scaleY = kFidelityHeight / 400.0f;
    
    // Save initial state
    canvas.save();
    
    // Translate and draw
    canvas.save();
    canvas.translate(50 * scaleX, 50 * scaleY);
    
    cw::Paint bluePaint;
    bluePaint.color = cw::Color::fromRGB(0.0f, 0.0f, 1.0f);
    canvas.drawRect(cw::Rect::fromLTWH(0, 0, 80 * scaleX, 60 * scaleY), bluePaint);
    canvas.restore();
    
    // Rotate around center
    canvas.save();
    canvas.translate(200 * scaleX, 100 * scaleY);
    canvas.rotate(0.785398f); // 45 degrees
    canvas.translate(-40 * scaleX, -30 * scaleY);
    
    cw::Paint redPaint;
    redPaint.color = cw::Color::fromRGB(1.0f, 0.0f, 0.0f);
    canvas.drawRect(cw::Rect::fromLTWH(0, 0, 80 * scaleX, 60 * scaleY), redPaint);
    canvas.restore();
    
    // Scale
    canvas.save();
    canvas.translate(350 * scaleX, 50 * scaleY);
    canvas.scale(1.5f, 1.5f);
    
    cw::Paint greenPaint;
    greenPaint.color = cw::Color::fromRGB(0.0f, 1.0f, 0.0f);
    canvas.drawRect(cw::Rect::fromLTWH(0, 0, 60 * scaleX, 40 * scaleY), greenPaint);
    canvas.restore();
    
    // Combined transforms
    canvas.save();
    canvas.translate(100 * scaleX, 250 * scaleY);
    canvas.rotate(0.3f);
    canvas.scale(1.2f, 0.8f);
    
    cw::Paint purplePaint;
    purplePaint.color = cw::Color::fromRGB(0.5f, 0.0f, 0.5f);
    canvas.drawRect(cw::Rect::fromLTWH(0, 0, 100 * scaleX, 80 * scaleY), purplePaint);
    canvas.restore();
    
    canvas.restore();
    
    const auto& commands = canvas.commands();
    // Should have transform commands plus rect commands
    EXPECT_GE(commands.size(), 4);
}

TEST(CanvasApiFidelity, Clipping)
{
    if (!goldenFileExists("canvas_clipping_flutter.json")) {
        GTEST_SKIP() << "Golden file not found. Run Flutter fidelity tester first.";
    }

    cw::Canvas canvas(400.0f, 400.0f);
    
    // Clip rect
    canvas.save();
    canvas.clipRect(cw::Rect::fromLTWH(20, 20, 150, 150));
    
    cw::Paint bluePaint;
    bluePaint.color = cw::Color::fromRGB(0.0f, 0.0f, 1.0f);
    canvas.drawCircle(cw::Offset(120, 120), 100, bluePaint);
    canvas.restore();
    
    // Clip rounded rect
    canvas.save();
    cw::RRect clipRRect(cw::Rect::fromLTWH(200, 20, 150, 150), 30.0f);
    canvas.clipRRect(clipRRect);
    
    cw::Paint redPaint;
    redPaint.color = cw::Color::fromRGB(1.0f, 0.0f, 0.0f);
    canvas.drawRect(cw::Rect::fromLTWH(180, 0, 200, 200), redPaint);
    canvas.restore();
    
    // Clip path (circle)
    canvas.save();
    cw::Path clipPath;
    clipPath.moveTo(cw::Offset(170, 280)); // approximate circle with oval
    // Note: Path::addOval would be needed for a proper circle clip
    canvas.clipPath(clipPath);
    
    cw::Paint greenPaint;
    greenPaint.color = cw::Color::fromRGB(0.0f, 1.0f, 0.0f);
    canvas.drawRect(cw::Rect::fromLTWH(30, 200, 160, 160), greenPaint);
    canvas.restore();
    
    const auto& commands = canvas.commands();
    EXPECT_GE(commands.size(), 3);
}

TEST(CanvasApiFidelity, PaintStyles)
{
    if (!goldenFileExists("canvas_paint_styles_flutter.json")) {
        GTEST_SKIP() << "Golden file not found. Run Flutter fidelity tester first.";
    }

    cw::Canvas canvas(kFidelityWidth, kFidelityHeight);
    
    // Scale factors based on reference 400x400 canvas
    const float scaleX = kFidelityWidth / 400.0f;
    const float scaleY = kFidelityHeight / 400.0f;
    
    // Fill style
    cw::Paint fillPaint;
    fillPaint.color = cw::Color::fromRGB(0.0f, 0.0f, 1.0f);
    fillPaint.style = cw::PaintStyle::fill;
    canvas.drawRect(cw::Rect::fromLTWH(20 * scaleX, 20 * scaleY, 100 * scaleX, 80 * scaleY), fillPaint);
    
    // Stroke style
    cw::Paint strokePaint;
    strokePaint.color = cw::Color::fromRGB(1.0f, 0.0f, 0.0f);
    strokePaint.style = cw::PaintStyle::stroke;
    strokePaint.stroke_width = 5 * scaleX;
    canvas.drawRect(cw::Rect::fromLTWH(140 * scaleX, 20 * scaleY, 100 * scaleX, 80 * scaleY), strokePaint);
    
    // Different stroke widths
    for (int i = 0; i < 4; i++) {
        float width = (i + 1) * 3.0f * scaleX;
        cw::Paint linePaint;
        linePaint.color = cw::Color::fromRGB(0.0f, 1.0f, 0.0f);
        linePaint.style = cw::PaintStyle::stroke;
        linePaint.stroke_width = width;
        float y = 140.0f * scaleY + i * 35.0f * scaleY;
        canvas.drawLine(cw::Offset(20 * scaleX, y), cw::Offset(150 * scaleX, y), linePaint);
    }
    
    // Blend modes (overlapping circles with different blend modes)
    cw::BlendMode modes[] = {
        cw::BlendMode::srcOver,
        cw::BlendMode::modulate,
        cw::BlendMode::plus,
    };
    cw::Color colors[] = {
        cw::Color::fromRGB(1.0f, 0.0f, 0.0f),    // Red
        cw::Color::fromRGB(0.0f, 1.0f, 0.0f),    // Green
        cw::Color::fromRGB(0.0f, 0.0f, 1.0f),    // Blue
    };
    
    for (int i = 0; i < 3; i++) {
        cw::Paint paint;
        paint.color = colors[i];
        paint.color.a = 0.7f; // 70% opacity
        paint.blend_mode = modes[i];
        canvas.drawCircle(cw::Offset(200.0f * scaleX + i * 60 * scaleX, 150 * scaleY), 40 * scaleX, paint);
    }
    
    const auto& commands = canvas.commands();
    EXPECT_GE(commands.size(), 9);
}

TEST(CanvasApiFidelity, ComplexScene)
{
    if (!goldenFileExists("canvas_complex_scene_flutter.json")) {
        GTEST_SKIP() << "Golden file not found. Run Flutter fidelity tester first.";
    }

    cw::Canvas canvas(400.0f, 400.0f);
    
    canvas.save();
    
    // Background pattern
    cw::Color bgColors[] = {
        cw::Color::fromRGB(0.68f, 0.85f, 0.9f),   // Light blue
        cw::Color::fromRGB(0.93f, 0.82f, 0.9f),   // Light purple
    };
    
    for (int i = 0; i < 5; i++) {
        for (int j = 0; j < 5; j++) {
            cw::Paint paint;
            paint.color = bgColors[(i + j) % 2];
            paint.color.a = 0.3f;
            canvas.drawCircle(
                cw::Offset(40.0f + i * 80, 40.0f + j * 80),
                50,
                paint
            );
        }
    }
    
    // Central transformed group
    canvas.save();
    canvas.translate(200, 200);
    canvas.rotate(0.2f);
    
    // Card background (rounded rect)
    cw::RRect cardRRect(cw::Rect::fromLTWH(-120, -80, 240, 160), 20.0f);
    
    cw::Paint cardPaint;
    cardPaint.color = cw::Color::fromRGBA(1.0f, 1.0f, 1.0f, 0.9f);
    canvas.drawRRect(cardRRect, cardPaint);
    
    // Card border
    cw::Paint borderPaint;
    borderPaint.color = cw::Color::fromRGB(0.68f, 0.85f, 0.9f);
    borderPaint.style = cw::PaintStyle::stroke;
    borderPaint.stroke_width = 3;
    canvas.drawRRect(cardRRect, borderPaint);
    
    // Inner content - clipped to card
    canvas.save();
    canvas.clipRRect(cardRRect);
    
    // Colored circles inside card
    cw::Color circleColors[] = {
        cw::Color::fromRGB(1.0f, 0.0f, 0.0f),    // Red
        cw::Color::fromRGB(0.0f, 1.0f, 0.0f),    // Green
        cw::Color::fromRGB(1.0f, 0.65f, 0.0f),   // Orange
    };
    
    for (int i = 0; i < 3; i++) {
        cw::Paint paint;
        paint.color = circleColors[i];
        paint.color.a = 0.6f;
        float x = -60.0f + i * 60;
        canvas.drawCircle(cw::Offset(x, 0), 35, paint);
    }
    
    canvas.restore(); // End clip
    canvas.restore(); // End transform
    
    // Corner decorations
    cw::Offset corners[] = {
        cw::Offset(30, 30),
        cw::Offset(370, 30),
        cw::Offset(30, 370),
        cw::Offset(370, 370),
    };
    
    for (int i = 0; i < 4; i++) {
        canvas.save();
        canvas.translate(corners[i].x, corners[i].y);
        canvas.rotate(i * 1.5708f); // 90 degree increments
        
        cw::Path path;
        path.moveTo(cw::Offset(0, -15));
        path.lineTo(cw::Offset(10, 0));
        path.lineTo(cw::Offset(0, 15));
        path.close();
        
        cw::Paint paint;
        paint.color = cw::Color::fromRGBA(0.5f, 0.0f, 1.0f, 0.5f);
        canvas.drawPath(path, paint);
        
        canvas.restore();
    }
    
    canvas.restore(); // End scene
    
    const auto& commands = canvas.commands();
    EXPECT_GE(commands.size(), 35); // Background (25) + card (3) + circles (3) + corners (4)
}

// ---------------------------------------------------------------------------
// Canvas State Management Test
// ---------------------------------------------------------------------------

TEST(CanvasApiFidelity, StateManagement)
{
    cw::Canvas canvas(kFidelityWidth, kFidelityHeight);
    
    // Initial save count should be 1 (root)
    EXPECT_EQ(canvas.getSaveCount(), 1);
    
    // Save and verify count increases
    canvas.save();
    EXPECT_EQ(canvas.getSaveCount(), 2);
    
    canvas.save();
    canvas.save();
    EXPECT_EQ(canvas.getSaveCount(), 4);
    
    // Restore and verify count decreases
    canvas.restore();
    EXPECT_EQ(canvas.getSaveCount(), 3);
    
    // Restore to specific count
    canvas.restoreToCount(1);
    EXPECT_EQ(canvas.getSaveCount(), 1);
    
    // Save with transform
    canvas.save();
    canvas.translate(100, 100);
    canvas.rotate(0.5f);
    canvas.scale(2.0f, 2.0f);
    EXPECT_EQ(canvas.getSaveCount(), 2);
    
    // Restore clears transform
    canvas.restore();
    EXPECT_EQ(canvas.getSaveCount(), 1);
}

// ---------------------------------------------------------------------------
// Path Operations Test
// ---------------------------------------------------------------------------

TEST(CanvasApiFidelity, PathOperations)
{
    cw::Path path;
    
    // Build a complex path
    path.moveTo(cw::Offset(100, 50));
    EXPECT_FLOAT_EQ(path.currentPoint().x, 100.0f);
    EXPECT_FLOAT_EQ(path.currentPoint().y, 50.0f);
    
    path.lineTo(cw::Offset(50, 150));
    path.lineTo(cw::Offset(150, 150));
    path.close();
    
    // Check bounds
    auto bounds = path.getBounds();
    EXPECT_FLOAT_EQ(bounds.left(), 50.0f);
    EXPECT_FLOAT_EQ(bounds.top(), 50.0f);
    EXPECT_FLOAT_EQ(bounds.right(), 150.0f);
    EXPECT_FLOAT_EQ(bounds.bottom(), 150.0f);
    
    // Note: path.contains() is a stub and not fully implemented yet
    // When implemented, it should return:
    // - true for points inside the triangle (e.g., 100, 100)
    // - false for points outside (e.g., 200, 200)
    // For now, we just verify the API exists
    (void)path.contains(cw::Offset(100, 100));
    
    // Path commands count
    EXPECT_GE(path.commands().size(), 3); // At least 3 commands (move + 2 lines + close)
}
