#include <gtest/gtest.h>
#include "visual_fidelity.hpp"
#include "visual_fidelity_helpers.hpp"

#include <campello_widgets/ui/canvas.hpp>
#include <campello_widgets/ui/path.hpp>
#include <campello_widgets/ui/rrect.hpp>
#include <campello_widgets/ui/paint.hpp>
#include <campello_widgets/ui/color.hpp>
#include <campello_widgets/ui/rect.hpp>
#include <campello_widgets/ui/offset.hpp>
#include <campello_widgets/ui/alignment.hpp>
#include <vector_math/matrix4.hpp>
#include <iostream>

namespace cw = systems::leal::campello_widgets;
namespace cwt = systems::leal::campello_widgets::testing;

using cwt::kFidelityWidth;
using cwt::kFidelityHeight;
using cwt::flutterGoldenExists;
using cwt::getFlutterGoldenPath;
using cwt::getCppOutputPath;

namespace {

const float kRefCanvasSize = 400.0f;

inline float scaleX() { return kFidelityWidth / kRefCanvasSize; }
inline float scaleY() { return kFidelityHeight / kRefCanvasSize; }

bool renderCanvasToPng(cw::Canvas& canvas, const std::string& name,
                       cw::Color clearColor = cw::Color::white())
{
    std::string outputPath = getCppOutputPath(name + ".png");

    // GPU path via the production Renderer/IDrawBackend, falling back to the
    // CPU software rasterizer only when no GPU device is available.
    if (cwt::captureDrawListToPng(canvas.commands(), kFidelityWidth, kFidelityHeight, clearColor, outputPath)) {
        return true;
    }

    cwt::VisualRenderer renderer(static_cast<int>(kFidelityWidth),
                                 static_cast<int>(kFidelityHeight));
    renderer.clear(clearColor);
    renderer.renderDrawList(canvas.commands());
    return renderer.saveToPng(outputPath);
}

void compareWithFlutterGolden(const std::string& name)
{
    if (!flutterGoldenExists(name + ".png")) {
        GTEST_SKIP() << "Flutter golden not found: " << name;
    }

    auto result = cwt::comparePngImages(
        getFlutterGoldenPath(name + ".png"),
        getCppOutputPath(name + ".png"),
        5, true);

    if (!result.diffImagePath.empty())
        std::cout << "Diff image: " << result.diffImagePath << std::endl;

    EXPECT_LT(result.pixelDifference, 5.0)
        << "Visual difference too large: " << result.pixelDifference
        << "% of pixels differ";
}

} // namespace

// ---------------------------------------------------------------------------
// Canvas API visual fidelity tests
// ---------------------------------------------------------------------------

TEST(VisualFidelityCanvasApi, Arcs)
{
    const float sx = scaleX();
    const float sy = scaleY();

    cw::Canvas canvas(kFidelityWidth, kFidelityHeight);

    // Blue pie slice (useCenter = true)
    cw::Paint bluePaint;
    bluePaint.color = cw::Color::fromRGB(0.1294f, 0.5882f, 0.9529f);
    canvas.drawArc(
        cw::Rect::fromLTWH(20 * sx, 20 * sy, 150 * sx, 150 * sy),
        0.0f, 1.5f, true, bluePaint);

    // Green stroke arc (useCenter = false)
    cw::Paint greenStroke;
    greenStroke.color = cw::Color::fromRGB(0.2980f, 0.6863f, 0.3137f);
    greenStroke.style = cw::PaintStyle::stroke;
    greenStroke.stroke_width = 10 * sx;
    canvas.drawArc(
        cw::Rect::fromLTWH(200 * sx, 20 * sy, 150 * sx, 150 * sy),
        0.5f, 2.0f, false, greenStroke);

    // Pie chart segments
    cw::Color colors[] = {
        cw::Color::fromRGB(1.0f, 0.0f, 0.0f),
        cw::Color::fromRGB(1.0f, 0.65f, 0.0f),
        cw::Color::fromRGB(1.0f, 1.0f, 0.0f),
        cw::Color::fromRGB(0.0f, 1.0f, 0.0f),
    };
    float values[] = {0.3f, 0.25f, 0.2f, 0.25f};
    float currentAngle = -1.5708f;
    const float twoPi = 6.28318f;

    for (int i = 0; i < 4; i++) {
        float sweep = values[i] * twoPi;
        cw::Paint paint;
        paint.color = colors[i];
        float cx = 100.0f * sx, cy = 280.0f * sy;
        float w = 70.0f * sx, h = 70.0f * sy;
        canvas.drawArc(
            cw::Rect::fromLTWH(cx - w, cy - h, w * 2, h * 2),
            currentAngle, sweep, true, paint);
        currentAngle += sweep;
    }

    EXPECT_TRUE(renderCanvasToPng(canvas, "canvas_api_arcs"));
    compareWithFlutterGolden("canvas_api_arcs");
}

TEST(VisualFidelityCanvasApi, Paths)
{
    const float sx = scaleX();
    const float sy = scaleY();

    cw::Canvas canvas(kFidelityWidth, kFidelityHeight);

    // Filled blue triangle
    cw::Path triangle;
    triangle.moveTo(cw::Offset(100 * sx, 50 * sy));
    triangle.lineTo(cw::Offset(50 * sx, 150 * sy));
    triangle.lineTo(cw::Offset(150 * sx, 150 * sy));
    triangle.close();

    cw::Paint blueFill;
    blueFill.color = cw::Color::fromRGBA(0.0f, 0.0f, 1.0f, 0.7f);
    canvas.drawPath(triangle, blueFill);

    // Stroked green quadratic bezier
    cw::Path quad;
    quad.moveTo(cw::Offset(200 * sx, 150 * sy));
    quad.quadTo(cw::Offset(250 * sx, 50 * sy), cw::Offset(300 * sx, 150 * sy));

    cw::Paint greenStroke;
    greenStroke.color = cw::Color::fromRGB(0.0f, 1.0f, 0.0f);
    greenStroke.style = cw::PaintStyle::stroke;
    greenStroke.stroke_width = 3 * sx;
    canvas.drawPath(quad, greenStroke);

    // Stroked purple cubic bezier
    cw::Path cubic;
    cubic.moveTo(cw::Offset(50 * sx, 250 * sy));
    cubic.cubicTo(
        cw::Offset(100 * sx, 200 * sy),
        cw::Offset(150 * sx, 300 * sy),
        cw::Offset(200 * sx, 250 * sy));

    cw::Paint purpleStroke;
    purpleStroke.color = cw::Color::fromRGB(0.5f, 0.0f, 0.5f);
    purpleStroke.style = cw::PaintStyle::stroke;
    purpleStroke.stroke_width = 4 * sx;
    canvas.drawPath(cubic, purpleStroke);

    EXPECT_TRUE(renderCanvasToPng(canvas, "canvas_api_paths"));
    compareWithFlutterGolden("canvas_api_paths");
}

TEST(VisualFidelityCanvasApi, Points)
{
    const float sx = scaleX();
    const float sy = scaleY();

    cw::Canvas canvas(kFidelityWidth, kFidelityHeight);

    // Individual points as red circles
    cw::Paint pointPaint;
    pointPaint.color = cw::Color::fromRGB(1.0f, 0.0f, 0.0f);
    pointPaint.stroke_width = 8 * sx;
    std::vector<cw::Offset> points = {
        cw::Offset(100 * sx, 100 * sy),
        cw::Offset(150 * sx, 120 * sy),
        cw::Offset(200 * sx, 110 * sy),
        cw::Offset(250 * sx, 130 * sy),
        cw::Offset(300 * sx, 100 * sy),
    };
    canvas.drawPoints(cw::PointMode::points, points, pointPaint);

    // Connected lines
    cw::Paint linePaint;
    linePaint.color = cw::Color::fromRGB(0.0f, 0.0f, 1.0f);
    linePaint.stroke_width = 4 * sx;
    std::vector<cw::Offset> linePoints = {
        cw::Offset(50 * sx, 200 * sy),
        cw::Offset(150 * sx, 280 * sy),
        cw::Offset(250 * sx, 220 * sy),
        cw::Offset(350 * sx, 300 * sy),
    };
    canvas.drawPoints(cw::PointMode::lines, linePoints, linePaint);

    // Polygon loop
    cw::Paint polyPaint;
    polyPaint.color = cw::Color::fromRGB(0.0f, 0.5f, 0.0f);
    polyPaint.stroke_width = 3 * sx;
    std::vector<cw::Offset> polyPoints = {
        cw::Offset(200 * sx, 350 * sy),
        cw::Offset(250 * sx, 450 * sy),
        cw::Offset(150 * sx, 450 * sy),
    };
    canvas.drawPoints(cw::PointMode::polygon, polyPoints, polyPaint);

    EXPECT_TRUE(renderCanvasToPng(canvas, "canvas_api_points"));
    compareWithFlutterGolden("canvas_api_points");
}

TEST(VisualFidelityCanvasApi, Clipping)
{
    const float sx = scaleX();
    const float sy = scaleY();

    cw::Canvas canvas(kFidelityWidth, kFidelityHeight);

    // Full-screen red background
    cw::Paint redPaint;
    redPaint.color = cw::Color::fromRGB(1.0f, 0.0f, 0.0f);
    canvas.drawRect(cw::Rect::fromLTWH(0, 0, kFidelityWidth, kFidelityHeight), redPaint);

    // Rect clip: blue circle clipped to a rect
    canvas.save();
    canvas.clipRect(cw::Rect::fromLTWH(20 * sx, 20 * sy, 150 * sx, 150 * sy));
    cw::Paint bluePaint;
    bluePaint.color = cw::Color::fromRGB(0.0f, 0.0f, 1.0f);
    canvas.drawCircle(cw::Offset(120 * sx, 120 * sy), 80 * sx, bluePaint);
    canvas.restore();

    // RRect clip: green circle clipped to rounded rect
    canvas.save();
    cw::RRect clipRRect(cw::Rect::fromLTWH(200 * sx, 20 * sy, 150 * sx, 150 * sy), 30 * sx);
    canvas.clipRRect(clipRRect);
    cw::Paint greenPaint;
    greenPaint.color = cw::Color::fromRGB(0.0f, 1.0f, 0.0f);
    canvas.drawCircle(cw::Offset(280 * sx, 110 * sy), 80 * sx, greenPaint);
    canvas.restore();

    // Path clip: yellow circle clipped to a triangle
    canvas.save();
    cw::Path triangle;
    triangle.moveTo(cw::Offset(100 * sx, 220 * sy));
    triangle.lineTo(cw::Offset(50 * sx, 350 * sy));
    triangle.lineTo(cw::Offset(150 * sx, 350 * sy));
    triangle.close();
    canvas.clipPath(triangle);
    cw::Paint yellowPaint;
    yellowPaint.color = cw::Color::fromRGB(1.0f, 1.0f, 0.0f);
    canvas.drawCircle(cw::Offset(100 * sx, 300 * sy), 80 * sx, yellowPaint);
    canvas.restore();

    EXPECT_TRUE(renderCanvasToPng(canvas, "canvas_api_clipping"));
    compareWithFlutterGolden("canvas_api_clipping");
}

TEST(VisualFidelityCanvasApi, PaintStyles)
{
    const float sx = scaleX();
    const float sy = scaleY();

    cw::Canvas canvas(kFidelityWidth, kFidelityHeight);

    // Filled rectangle
    cw::Paint fillPaint;
    fillPaint.color = cw::Color::fromRGB(0.0f, 0.0f, 1.0f);
    canvas.drawRect(cw::Rect::fromLTWH(20 * sx, 20 * sy, 150 * sx, 100 * sy), fillPaint);

    // Stroked rectangle
    cw::Paint strokePaint;
    strokePaint.color = cw::Color::fromRGB(1.0f, 0.0f, 0.0f);
    strokePaint.style = cw::PaintStyle::stroke;
    strokePaint.stroke_width = 8 * sx;
    canvas.drawRect(cw::Rect::fromLTWH(200 * sx, 20 * sy, 150 * sx, 100 * sy), strokePaint);

    // Thick vs thin strokes
    for (int i = 0; i < 4; i++) {
        cw::Paint p;
        p.color = cw::Color::fromRGB(0.0f, 0.5f, 0.0f);
        p.style = cw::PaintStyle::stroke;
        p.stroke_width = (i + 1) * 3.0f * sx;
        float y = 150.0f * sy + i * 30.0f * sy;
        canvas.drawLine(cw::Offset(20 * sx, y), cw::Offset(370 * sx, y), p);
    }

    // Opacity
    cw::Paint opacityPaint;
    opacityPaint.color = cw::Color::fromRGBA(1.0f, 0.0f, 1.0f, 0.5f);
    canvas.drawCircle(cw::Offset(100 * sx, 320 * sy), 50 * sx, opacityPaint);

    EXPECT_TRUE(renderCanvasToPng(canvas, "canvas_api_paint_styles"));
    compareWithFlutterGolden("canvas_api_paint_styles");
}

TEST(VisualFidelityCanvasApi, SaveLayer)
{
    const float sx = scaleX();
    const float sy = scaleY();

    cw::Canvas canvas(kFidelityWidth, kFidelityHeight);

    // Background
    cw::Paint bgPaint;
    bgPaint.color = cw::Color::fromRGB(0.9f, 0.9f, 0.9f);
    canvas.drawRect(cw::Rect::fromLTWH(0, 0, kFidelityWidth, kFidelityHeight), bgPaint);

    // SaveLayer with 50% opacity
    cw::Paint layerPaint;
    layerPaint.color = cw::Color::fromRGBA(1.0f, 1.0f, 1.0f, 0.5f);
    canvas.saveLayer(cw::Rect::fromLTWH(50 * sx, 50 * sy, 300 * sx, 300 * sy), layerPaint);

    cw::Paint redPaint;
    redPaint.color = cw::Color::fromRGB(1.0f, 0.0f, 0.0f);
    canvas.drawRect(cw::Rect::fromLTWH(50 * sx, 50 * sy, 150 * sx, 150 * sy), redPaint);

    cw::Paint bluePaint;
    bluePaint.color = cw::Color::fromRGB(0.0f, 0.0f, 1.0f);
    canvas.drawCircle(cw::Offset(250 * sx, 200 * sy), 60 * sx, bluePaint);

    canvas.restore();

    EXPECT_TRUE(renderCanvasToPng(canvas, "canvas_api_save_layer"));
    compareWithFlutterGolden("canvas_api_save_layer");
}

TEST(VisualFidelityCanvasApi, DrawColor)
{
    const float sx = scaleX();
    const float sy = scaleY();

    cw::Canvas canvas(kFidelityWidth, kFidelityHeight);

    // Left half: solid red fill via drawColor
    canvas.save();
    canvas.clipRect(cw::Rect::fromLTWH(0, 0, kFidelityWidth * 0.5f, kFidelityHeight));
    canvas.drawColor(cw::Color::fromRGB(1.0f, 0.0f, 0.0f), cw::BlendMode::srcOver);
    canvas.restore();

    // Right half: blue background with a green rect on top
    canvas.save();
    canvas.clipRect(cw::Rect::fromLTWH(kFidelityWidth * 0.5f, 0, kFidelityWidth * 0.5f, kFidelityHeight));
    canvas.drawColor(cw::Color::fromRGB(0.0f, 0.0f, 1.0f), cw::BlendMode::srcOver);

    cw::Paint greenPaint;
    greenPaint.color = cw::Color::fromRGB(0.0f, 1.0f, 0.0f);
    canvas.drawRect(cw::Rect::fromLTWH(80 * sx, 80 * sy, 120 * sx, 120 * sy), greenPaint);
    canvas.restore();

    EXPECT_TRUE(renderCanvasToPng(canvas, "canvas_api_draw_color"));
    compareWithFlutterGolden("canvas_api_draw_color");
}

TEST(VisualFidelityCanvasApi, Skew)
{
    const float sx = scaleX();
    const float sy = scaleY();
    namespace vm = systems::leal::vector_math;

    cw::Canvas canvas(kFidelityWidth, kFidelityHeight);

    // Horizontal skew
    canvas.save();
    canvas.translate(80 * sx, 100 * sy);
    canvas.skew(0.3f, 0.0f);
    cw::Paint bluePaint;
    bluePaint.color = cw::Color::fromRGB(0.1294f, 0.5882f, 0.9529f);
    canvas.drawRect(cw::Rect::fromLTWH(0, 0, 100 * sx, 100 * sy), bluePaint);
    canvas.restore();

    // Vertical skew
    canvas.save();
    canvas.translate(220 * sx, 80 * sy);
    canvas.skew(0.0f, 0.25f);
    cw::Paint greenPaint;
    greenPaint.color = cw::Color::fromRGB(0.2980f, 0.6863f, 0.3137f);
    canvas.drawRect(cw::Rect::fromLTWH(0, 0, 80 * sx, 120 * sy), greenPaint);
    canvas.restore();

    // Matrix4 rotation + scale via transform()
    canvas.save();
    canvas.translate(320 * sx, 200 * sy);
    auto matrix = vm::Matrix4<float>::rotateZ(0.523599f);
    matrix = matrix * vm::Matrix4<float>::scale(vm::Vector3<float>(1.2f, 1.2f, 1.0f));
    canvas.transform(matrix);
    cw::Paint orangePaint;
    orangePaint.color = cw::Color::fromRGB(1.0f, 0.5961f, 0.0f);
    canvas.drawRect(cw::Rect::fromLTWH(-40 * sx, -40 * sy, 80 * sx, 80 * sy), orangePaint);
    canvas.restore();

    EXPECT_TRUE(renderCanvasToPng(canvas, "canvas_api_skew"));
    compareWithFlutterGolden("canvas_api_skew");
}

TEST(VisualFidelityCanvasApi, PathArc)
{
    const float sx = scaleX();
    const float sy = scaleY();

    cw::Canvas canvas(kFidelityWidth, kFidelityHeight);

    // Filled path built with arcTo (pie slice)
    cw::Path pie;
    pie.moveTo(cw::Offset(100 * sx, 100 * sy));
    pie.arcTo(
        cw::Rect::fromLTWH(50 * sx, 50 * sy, 100 * sx, 100 * sy),
        -1.5708f, 2.3562f, false);
    pie.close();

    cw::Paint bluePaint;
    bluePaint.color = cw::Color::fromRGB(0.1294f, 0.5882f, 0.9529f);
    canvas.drawPath(pie, bluePaint);

    // Stroked open arc
    cw::Path openArc;
    openArc.moveTo(cw::Offset(250 * sx, 150 * sy));
    openArc.arcTo(
        cw::Rect::fromLTWH(200 * sx, 50 * sy, 150 * sx, 150 * sy),
        0.0f, 3.9270f, false);

    cw::Paint redStroke;
    redStroke.color = cw::Color::fromRGB(1.0f, 0.0f, 0.0f);
    redStroke.style = cw::PaintStyle::stroke;
    redStroke.stroke_width = 6 * sx;
    canvas.drawPath(openArc, redStroke);

    // Full oval built with addArc
    cw::Path oval;
    oval.addArc(
        cw::Rect::fromLTWH(80 * sx, 200 * sy, 120 * sx, 80 * sy),
        0.0f, 6.28318f);

    cw::Paint greenStroke;
    greenStroke.color = cw::Color::fromRGB(0.2980f, 0.6863f, 0.3137f);
    greenStroke.style = cw::PaintStyle::stroke;
    greenStroke.stroke_width = 4 * sx;
    canvas.drawPath(oval, greenStroke);

    EXPECT_TRUE(renderCanvasToPng(canvas, "canvas_api_path_arc"));
    compareWithFlutterGolden("canvas_api_path_arc");
}

namespace {

// Build a Path that draws a rounded rectangle with different radii per corner.
// Flutter's RRect.fromRectAndCorners uses circular corners; this helper mirrors
// that shape using cubic Bezier approximations. Cubics avoid the Vulkan path
// renderer's elliptical-arc flattening path that currently hangs the GPU when
// several non-uniform per-corner radii are drawn in a single path.
cw::Path buildComplexRRectPath(const cw::RRectComplex& rr)
{
    const float x = rr.rect.left();
    const float y = rr.rect.top();
    const float r = rr.rect.right();
    const float b = rr.rect.bottom();
    const float tl = rr.top_left_radius;
    const float tr = rr.top_right_radius;
    const float bl = rr.bottom_left_radius;
    const float br = rr.bottom_right_radius;

    // Cubic Bezier approximation constant for a 90-degree circular corner.
    const float k = 0.5522847498f;

    cw::Path path;
    path.moveTo(cw::Offset(x + tl, y));
    path.lineTo(cw::Offset(r - tr, y));
    if (tr > 0.0f) {
        const float cx = r - tr;
        const float cy = y + tr;
        path.cubicTo(
            cw::Offset(cx + k * tr, y),
            cw::Offset(r, cy - k * tr),
            cw::Offset(r, cy));
    }
    path.lineTo(cw::Offset(r, b - br));
    if (br > 0.0f) {
        const float cx = r - br;
        const float cy = b - br;
        path.cubicTo(
            cw::Offset(r, cy + k * br),
            cw::Offset(cx + k * br, b),
            cw::Offset(cx, b));
    }
    path.lineTo(cw::Offset(x + bl, b));
    if (bl > 0.0f) {
        const float cx = x + bl;
        const float cy = b - bl;
        path.cubicTo(
            cw::Offset(cx - k * bl, b),
            cw::Offset(x, cy + k * bl),
            cw::Offset(x, cy));
    }
    path.lineTo(cw::Offset(x, y + tl));
    if (tl > 0.0f) {
        const float cx = x + tl;
        const float cy = y + tl;
        path.cubicTo(
            cw::Offset(x, cy - k * tl),
            cw::Offset(cx - k * tl, y),
            cw::Offset(cx, y));
    }
    path.close();
    return path;
}

} // namespace

TEST(VisualFidelityCanvasApi, DrawPaint)
{
    const float sx = scaleX();
    const float sy = scaleY();

    cw::Canvas canvas(kFidelityWidth, kFidelityHeight);

    // Left half: cyan fill via drawPaint
    canvas.save();
    canvas.clipRect(cw::Rect::fromLTWH(0, 0, kFidelityWidth * 0.5f, kFidelityHeight));
    cw::Paint cyanPaint;
    cyanPaint.color = cw::Color::fromRGB(0.0f, 1.0f, 1.0f);
    canvas.drawPaint(cyanPaint);
    canvas.restore();

    // Right half: yellow fill with a magenta rect on top
    canvas.save();
    canvas.clipRect(cw::Rect::fromLTWH(kFidelityWidth * 0.5f, 0, kFidelityWidth * 0.5f, kFidelityHeight));
    cw::Paint yellowPaint;
    yellowPaint.color = cw::Color::fromRGB(1.0f, 1.0f, 0.0f);
    canvas.drawPaint(yellowPaint);

    cw::Paint magentaPaint;
    magentaPaint.color = cw::Color::fromRGB(1.0f, 0.0f, 1.0f);
    canvas.drawRect(cw::Rect::fromLTWH(80 * sx, 80 * sy, 120 * sx, 120 * sy), magentaPaint);
    canvas.restore();

    EXPECT_TRUE(renderCanvasToPng(canvas, "canvas_api_draw_paint"));
    compareWithFlutterGolden("canvas_api_draw_paint");
}

TEST(VisualFidelityCanvasApi, RRectComplex)
{
    const float sx = scaleX();
    const float sy = scaleY();

    cw::Canvas canvas(kFidelityWidth, kFidelityHeight);

    // Filled complex rounded rect
    cw::RRectComplex rrect1 = cw::RRectComplex::fromRectAndCorners(
        cw::Rect::fromLTWH(40 * sx, 40 * sy, 160 * sx, 120 * sy),
        30 * sx, 20 * sx, 25 * sx, 40 * sx);
    cw::Paint bluePaint;
    bluePaint.color = cw::Color::fromRGB(0.1294f, 0.5882f, 0.9529f);
    canvas.drawPath(buildComplexRRectPath(rrect1), bluePaint);

    // Stroked complex rounded rect
    cw::RRectComplex rrect2 = cw::RRectComplex::fromRectAndCorners(
        cw::Rect::fromLTWH(220 * sx, 40 * sy, 160 * sx, 120 * sy),
        10 * sx, 40 * sx, 40 * sx, 10 * sx);
    cw::Paint redStroke;
    redStroke.color = cw::Color::fromRGB(1.0f, 0.0f, 0.0f);
    redStroke.style = cw::PaintStyle::stroke;
    redStroke.stroke_width = 6 * sx;
    canvas.drawPath(buildComplexRRectPath(rrect2), redStroke);

    // Smaller filled complex rrect
    cw::RRectComplex rrect3 = cw::RRectComplex::fromRectAndCorners(
        cw::Rect::fromLTWH(130 * sx, 200 * sy, 140 * sx, 90 * sy),
        0.0f, 25 * sx, 25 * sx, 0.0f);
    cw::Paint greenPaint;
    greenPaint.color = cw::Color::fromRGB(0.2980f, 0.6863f, 0.3137f);
    canvas.drawPath(buildComplexRRectPath(rrect3), greenPaint);

    EXPECT_TRUE(renderCanvasToPng(canvas, "canvas_api_rrect_complex"));
    compareWithFlutterGolden("canvas_api_rrect_complex");
}

TEST(VisualFidelityCanvasApi, BlendModes)
{
    const float sx = scaleX();
    const float sy = scaleY();

    cw::Canvas canvas(kFidelityWidth, kFidelityHeight);

    // Background: three grey rectangles
    cw::Paint greyPaint;
    greyPaint.color = cw::Color::fromRGB(0.6667f, 0.6667f, 0.6667f);
    for (int i = 0; i < 3; i++) {
        canvas.drawRect(
            cw::Rect::fromLTWH((40 + i * 110) * sx, 60 * sy, 80 * sx, 180 * sy),
            greyPaint);
    }

    // Row 1: srcOver
    cw::Color srcOverColors[3] = {
        cw::Color::fromRGBA(1.0f, 0.0f, 0.0f, 0.5f),
        cw::Color::fromRGBA(0.0f, 1.0f, 0.0f, 0.5f),
        cw::Color::fromRGBA(0.0f, 0.0f, 1.0f, 0.5f),
    };
    for (int i = 0; i < 3; i++) {
        cw::Paint paint;
        paint.color = srcOverColors[i];
        paint.blend_mode = cw::BlendMode::srcOver;
        canvas.drawCircle(cw::Offset((80 + i * 110) * sx, 100 * sy), 35 * sx, paint);
    }

    // Row 2: modulate (opaque sources avoid premultiplied-vs-straight alpha
    // representation mismatches between our GPU output and Flutter's PNG.)
    // Colors match Flutter's Material `Colors.red/green/blue`, which are not
    // pure RGB (0xFFF44336, 0xFF4CAF50, 0xFF2196F3).
    cw::Color modulateColors[3] = {
        cw::Color::fromARGB(0xFFF44336),
        cw::Color::fromARGB(0xFF4CAF50),
        cw::Color::fromARGB(0xFF2196F3),
    };
    for (int i = 0; i < 3; i++) {
        cw::Paint paint;
        paint.color = modulateColors[i];
        paint.blend_mode = cw::BlendMode::modulate;
        canvas.drawCircle(cw::Offset((80 + i * 110) * sx, 170 * sy), 35 * sx, paint);
    }

    // Row 3: plus (opaque sources, same Material colors as row 2)
    cw::Color plusColors[3] = {
        cw::Color::fromARGB(0xFFF44336),
        cw::Color::fromARGB(0xFF4CAF50),
        cw::Color::fromARGB(0xFF2196F3),
    };
    for (int i = 0; i < 3; i++) {
        cw::Paint paint;
        paint.color = plusColors[i];
        paint.blend_mode = cw::BlendMode::plus;
        canvas.drawCircle(cw::Offset((80 + i * 110) * sx, 240 * sy), 35 * sx, paint);
    }

    EXPECT_TRUE(renderCanvasToPng(canvas, "canvas_api_blend_modes"));
    compareWithFlutterGolden("canvas_api_blend_modes");
}

TEST(VisualFidelityCanvasApi, BlendModesExtended)
{
    const float sx = scaleX();
    const float sy = scaleY();

    cw::Canvas canvas(kFidelityWidth, kFidelityHeight);

    // The remaining BlendMode enum values not covered by the BlendModes test
    // (srcOver, plus, and modulate are tested there).
    const cw::BlendMode modes[] = {
        cw::BlendMode::clear,
        cw::BlendMode::src,
        cw::BlendMode::dst,
        cw::BlendMode::dstOver,
        cw::BlendMode::srcIn,
        cw::BlendMode::dstIn,
        cw::BlendMode::srcOut,
        cw::BlendMode::dstOut,
        cw::BlendMode::srcATop,
        cw::BlendMode::dstATop,
        cw::BlendMode::xorMode,
    };
    const int modeCount = 11;

    // 4x3 grid of cells, one blend mode per cell: an opaque orange
    // "destination" square with a semi-transparent blue "source" circle
    // composited on top using the tested blend mode.
    const float cellW = 100.0f;
    const float cellH = 130.0f;

    for (int i = 0; i < modeCount; i++) {
        int col = i % 4;
        int row = i / 4;
        float originX = col * cellW;
        float originY = row * cellH;

        cw::Paint destPaint;
        destPaint.color = cw::Color::fromRGB(1.0f, 0.5961f, 0.0f);
        canvas.drawRect(
            cw::Rect::fromLTWH((originX + 10) * sx, (originY + 15) * sy, 55 * sx, 55 * sy),
            destPaint);

        cw::Paint srcPaint;
        srcPaint.color = cw::Color::fromRGBA(0.0f, 0.0f, 1.0f, 0.6f);
        srcPaint.blend_mode = modes[i];
        canvas.drawCircle(
            cw::Offset((originX + 65) * sx, (originY + 70) * sy), 28 * sx, srcPaint);
    }

    EXPECT_TRUE(renderCanvasToPng(canvas, "canvas_api_blend_modes_extended"));
    compareWithFlutterGolden("canvas_api_blend_modes_extended");
}

TEST(VisualFidelityCanvasApi, ClipOval)
{
    const float sx = scaleX();
    const float sy = scaleY();

    cw::Canvas canvas(kFidelityWidth, kFidelityHeight);

    cw::Paint bgPaint;
    bgPaint.color = cw::Color::fromRGB(0.9f, 0.9f, 0.9f);
    canvas.drawRect(cw::Rect::fromLTWH(0, 0, kFidelityWidth, kFidelityHeight), bgPaint);

    // Wide ellipse clip, filled with blue.
    canvas.save();
    canvas.clipOval(cw::Rect::fromLTWH(20 * sx, 20 * sy, 360 * sx, 120 * sy));
    cw::Paint bluePaint;
    bluePaint.color = cw::Color::fromRGB(0.1294f, 0.5882f, 0.9529f);
    canvas.drawRect(cw::Rect::fromLTWH(0, 0, kFidelityWidth, kFidelityHeight), bluePaint);
    canvas.restore();

    // Tall ellipse clip, filled with green.
    canvas.save();
    canvas.clipOval(cw::Rect::fromLTWH(30 * sx, 180 * sy, 120 * sx, 200 * sy));
    cw::Paint greenPaint;
    greenPaint.color = cw::Color::fromRGB(0.2980f, 0.6863f, 0.3137f);
    canvas.drawRect(cw::Rect::fromLTWH(0, 0, kFidelityWidth, kFidelityHeight), greenPaint);
    canvas.restore();

    // Circular clip (equal radii), filled with orange plus a nested circle
    // to confirm content still draws normally inside an oval clip.
    canvas.save();
    canvas.clipOval(cw::Rect::fromLTWH(220 * sx, 180 * sy, 150 * sx, 150 * sy));
    cw::Paint orangePaint;
    orangePaint.color = cw::Color::fromRGB(1.0f, 0.5961f, 0.0f);
    canvas.drawRect(cw::Rect::fromLTWH(0, 0, kFidelityWidth, kFidelityHeight), orangePaint);
    cw::Paint purplePaint;
    purplePaint.color = cw::Color::fromRGB(0.5f, 0.0f, 0.5f);
    canvas.drawCircle(cw::Offset(295 * sx, 255 * sy), 40 * sx, purplePaint);
    canvas.restore();

    EXPECT_TRUE(renderCanvasToPng(canvas, "canvas_api_clip_oval"));
    compareWithFlutterGolden("canvas_api_clip_oval");
}

