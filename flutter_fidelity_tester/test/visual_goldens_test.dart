import 'dart:io';
import 'dart:ui' as ui;
import 'dart:typed_data';
import 'dart:math' as math;
import 'package:flutter/material.dart';
import 'package:flutter/rendering.dart';
import 'package:flutter/services.dart' show rootBundle;
import 'package:flutter_test/flutter_test.dart';

// Enable real font rendering in tests
// This requires fonts to be available in the test environment
void enableRealFonts() {
  // In flutter test, fonts are replaced with 'Ahem' (colored blocks) by default
  // To get real text rendering, use: flutter run lib/golden_generator.dart
}

// Font family to use - will fall back to Ahem in test environment
const String kTestFontFamily = 'Roboto';

/// Generates visual golden PNG files from Flutter for pixel-perfect comparison.
/// 
/// These tests render widgets offscreen and save PNG files that serve as the
/// "ground truth" for C++ visual fidelity testing.
///
/// Run with: flutter test test/visual_goldens_test.dart
/// 
/// NOTE: flutter test renders text as colored blocks (Ahem font) for determinism.
/// For real text rendering, use: flutter run lib/golden_generator.dart
///
/// Standard resolution for fidelity testing - ensures both Flutter and C++ images have same size
const double kFidelityWidth = 1280.0;
const double kFidelityHeight = 720.0;

void main() {
  final goldensDir = Directory('../tests/visual_fidelity/flutter_goldens');
  
  setUpAll(() {
    if (!goldensDir.existsSync()) {
      goldensDir.createSync(recursive: true);
    }
  });

  // Layout tests
  group('Visual Golden PNGs - Layout Tests', () {
    
    testWidgets('simple_column', (WidgetTester tester) async {
      await tester.binding.setSurfaceSize(const Size(kFidelityWidth, kFidelityHeight));
      await tester.pumpWidget(
        MaterialApp(
          debugShowCheckedModeBanner: false,
          home: Scaffold(
            backgroundColor: Colors.white,
            body: SizedBox(
              width: kFidelityWidth,
              height: kFidelityHeight,
              child: RepaintBoundary(
                child: Column(
                  children: [
                    Padding(
                      padding: const EdgeInsets.all(32.0),
                      child: Container(width: 200, height: 200, color: Colors.blue),
                    ),
                    Expanded(child: Container(color: Colors.green.withAlpha(128))),
                  ],
                ),
              ),
            ),
          ),
        ),
      );
      
      await tester.pumpAndSettle();
      await captureAndSave(tester, '${goldensDir.path}/simple_column.png');
    });

    testWidgets('simple_row', (WidgetTester tester) async {
      await tester.binding.setSurfaceSize(const Size(kFidelityWidth, kFidelityHeight));
      await tester.pumpWidget(
        MaterialApp(
          debugShowCheckedModeBanner: false,
          home: Scaffold(
            backgroundColor: Colors.white,
            body: SizedBox(
              width: kFidelityWidth,
              height: kFidelityHeight,
              child: RepaintBoundary(
                child: Row(
                  children: [
                    Container(width: 200, height: 150, color: Colors.red),
                    Expanded(child: Container(color: Colors.green)),
                    Container(width: 200, height: 150, color: Colors.blue),
                  ],
                ),
              ),
            ),
          ),
        ),
      );
      
      await tester.pumpAndSettle();
      await captureAndSave(tester, '${goldensDir.path}/simple_row.png');
    });

    testWidgets('align_center', (WidgetTester tester) async {
      await tester.binding.setSurfaceSize(const Size(kFidelityWidth, kFidelityHeight));
      await tester.pumpWidget(
        MaterialApp(
          debugShowCheckedModeBanner: false,
          home: Scaffold(
            backgroundColor: Colors.white,
            body: SizedBox(
              width: kFidelityWidth,
              height: kFidelityHeight,
              child: RepaintBoundary(
                child: Center(
                  child: Container(width: 300, height: 300, color: Colors.orange),
                ),
              ),
            ),
          ),
        ),
      );
      
      await tester.pumpAndSettle();
      await captureAndSave(tester, '${goldensDir.path}/align_center.png');
    });

    testWidgets('nested_padding', (WidgetTester tester) async {
      await tester.binding.setSurfaceSize(const Size(kFidelityWidth, kFidelityHeight));
      await tester.pumpWidget(
        MaterialApp(
          debugShowCheckedModeBanner: false,
          home: Scaffold(
            backgroundColor: Colors.white,
            body: SizedBox(
              width: kFidelityWidth,
              height: kFidelityHeight,
              child: RepaintBoundary(
                child: const Padding(
                  padding: EdgeInsets.all(48.0),
                  child: Padding(
                    padding: EdgeInsets.symmetric(horizontal: 32.0, vertical: 16.0),
                    child: ColoredBox(
                      color: Colors.purple,
                      child: SizedBox(width: 200, height: 200),
                    ),
                  ),
                ),
              ),
            ),
          ),
        ),
      );
      
      await tester.pumpAndSettle();
      await captureAndSave(tester, '${goldensDir.path}/nested_padding.png');
    });

    testWidgets('flex_expanded', (WidgetTester tester) async {
      await tester.binding.setSurfaceSize(const Size(kFidelityWidth, kFidelityHeight));
      await tester.pumpWidget(
        MaterialApp(
          debugShowCheckedModeBanner: false,
          home: Scaffold(
            backgroundColor: Colors.white,
            body: SizedBox(
              width: kFidelityWidth,
              height: kFidelityHeight,
              child: RepaintBoundary(
                child: Column(
                  children: [
                    Container(height: 100, color: Colors.red),
                    Expanded(flex: 2, child: Container(color: Colors.green)),
                    Expanded(flex: 1, child: Container(color: Colors.blue)),
                  ],
                ),
              ),
            ),
          ),
        ),
      );
      
      await tester.pumpAndSettle();
      await captureAndSave(tester, '${goldensDir.path}/flex_expanded.png');
    });

    testWidgets('stack_positioned', (WidgetTester tester) async {
      await tester.binding.setSurfaceSize(const Size(kFidelityWidth, kFidelityHeight));
      await tester.pumpWidget(
        MaterialApp(
          debugShowCheckedModeBanner: false,
          home: Scaffold(
            backgroundColor: Colors.white,
            body: SizedBox(
              width: kFidelityWidth,
              height: kFidelityHeight,
              child: RepaintBoundary(
                child: Stack(
                  children: [
                    Container(color: Colors.grey.shade300),
                    Positioned(
                      left: 100,
                      top: 100,
                      child: Container(width: 250, height: 200, color: Colors.red),
                    ),
                    Positioned(
                      right: 100,
                      bottom: 100,
                      child: Container(width: 250, height: 200, color: Colors.blue),
                    ),
                    Positioned(
                      left: 450,
                      top: 300,
                      child: Container(width: 250, height: 200, color: Colors.green.withAlpha(178)),
                    ),
                  ],
                ),
              ),
            ),
          ),
        ),
      );
      
      await tester.pumpAndSettle();
      await captureAndSave(tester, '${goldensDir.path}/stack_positioned.png');
    });
  });

  // Canvas tests
  group('Visual Golden PNGs - Canvas API Tests', () {
    
    testWidgets('canvas_basic_shapes', (WidgetTester tester) async {
      await tester.binding.setSurfaceSize(const Size(kFidelityWidth, kFidelityHeight));
      await tester.pumpWidget(
        MaterialApp(
          debugShowCheckedModeBanner: false,
          home: Scaffold(
            backgroundColor: Colors.white,
            body: SizedBox(
              width: kFidelityWidth,
              height: kFidelityHeight,
              child: RepaintBoundary(
                child: CustomPaint(
                  size: const Size(kFidelityWidth, kFidelityHeight),
                  painter: BasicShapesPainter(),
                ),
              ),
            ),
          ),
        ),
      );
      
      await tester.pumpAndSettle();
      await captureAndSave(tester, '${goldensDir.path}/canvas_basic_shapes.png');
    });

    testWidgets('canvas_rounded_rects', (WidgetTester tester) async {
      await tester.binding.setSurfaceSize(const Size(kFidelityWidth, kFidelityHeight));
      await tester.pumpWidget(
        MaterialApp(
          debugShowCheckedModeBanner: false,
          home: Scaffold(
            backgroundColor: Colors.white,
            body: SizedBox(
              width: kFidelityWidth,
              height: kFidelityHeight,
              child: RepaintBoundary(
                child: CustomPaint(
                  size: const Size(kFidelityWidth, kFidelityHeight),
                  painter: RoundedRectsPainter(),
                ),
              ),
            ),
          ),
        ),
      );
      
      await tester.pumpAndSettle();
      await captureAndSave(tester, '${goldensDir.path}/canvas_rounded_rects.png');
    });

    testWidgets('canvas_lines', (WidgetTester tester) async {
      await tester.binding.setSurfaceSize(const Size(kFidelityWidth, kFidelityHeight));
      await tester.pumpWidget(
        MaterialApp(
          debugShowCheckedModeBanner: false,
          home: Scaffold(
            backgroundColor: Colors.white,
            body: SizedBox(
              width: kFidelityWidth,
              height: kFidelityHeight,
              child: RepaintBoundary(
                child: CustomPaint(
                  size: const Size(kFidelityWidth, kFidelityHeight),
                  painter: LinesPainter(),
                ),
              ),
            ),
          ),
        ),
      );
      
      await tester.pumpAndSettle();
      await captureAndSave(tester, '${goldensDir.path}/canvas_lines.png');
    });

    testWidgets('canvas_complex_scene', (WidgetTester tester) async {
      await tester.binding.setSurfaceSize(const Size(kFidelityWidth, kFidelityHeight));
      await tester.pumpWidget(
        MaterialApp(
          debugShowCheckedModeBanner: false,
          home: Scaffold(
            backgroundColor: Colors.white,
            body: SizedBox(
              width: kFidelityWidth,
              height: kFidelityHeight,
              child: RepaintBoundary(
                child: CustomPaint(
                  size: const Size(kFidelityWidth, kFidelityHeight),
                  painter: ComplexScenePainter(),
                ),
              ),
            ),
          ),
        ),
      );

      await tester.pumpAndSettle();
      await captureAndSave(tester, '${goldensDir.path}/canvas_complex_scene.png');
    });

    testWidgets('canvas_text_basic', (WidgetTester tester) async {
      await tester.binding.setSurfaceSize(const Size(kFidelityWidth, kFidelityHeight));
      await tester.pumpWidget(
        MaterialApp(
          debugShowCheckedModeBanner: false,
          theme: ThemeData(fontFamily: kTestFontFamily),
          home: Scaffold(
            backgroundColor: Colors.white,
            body: SizedBox(
              width: kFidelityWidth,
              height: kFidelityHeight,
              child: RepaintBoundary(
                child: CustomPaint(
                  size: const Size(kFidelityWidth, kFidelityHeight),
                  painter: TextBasicPainter(),
                ),
              ),
            ),
          ),
        ),
      );
      await tester.pumpAndSettle();
      await captureAndSave(tester, '${goldensDir.path}/canvas_text_basic.png');
    });

    testWidgets('canvas_transforms', (WidgetTester tester) async {
      await tester.binding.setSurfaceSize(const Size(kFidelityWidth, kFidelityHeight));
      await tester.pumpWidget(
        MaterialApp(
          debugShowCheckedModeBanner: false,
          home: Scaffold(
            backgroundColor: Colors.white,
            body: SizedBox(
              width: kFidelityWidth,
              height: kFidelityHeight,
              child: RepaintBoundary(
                child: CustomPaint(
                  size: const Size(kFidelityWidth, kFidelityHeight),
                  painter: TransformsPainter(),
                ),
              ),
            ),
          ),
        ),
      );
      await tester.pumpAndSettle();
      await captureAndSave(tester, '${goldensDir.path}/canvas_transforms.png');
    });

    testWidgets('canvas_rotate', (WidgetTester tester) async {
      await tester.binding.setSurfaceSize(const Size(kFidelityWidth, kFidelityHeight));
      await tester.pumpWidget(
        MaterialApp(
          debugShowCheckedModeBanner: false,
          home: Scaffold(
            backgroundColor: Colors.white,
            body: SizedBox(
              width: kFidelityWidth,
              height: kFidelityHeight,
              child: RepaintBoundary(
                child: CustomPaint(
                  size: const Size(kFidelityWidth, kFidelityHeight),
                  painter: RotatePainter(),
                ),
              ),
            ),
          ),
        ),
      );
      await tester.pumpAndSettle();
      await captureAndSave(tester, '${goldensDir.path}/canvas_rotate.png');
    });

    testWidgets('canvas_api_arcs', (WidgetTester tester) async {
      await tester.binding.setSurfaceSize(const Size(kFidelityWidth, kFidelityHeight));
      await tester.pumpWidget(
        MaterialApp(
          debugShowCheckedModeBanner: false,
          home: Scaffold(
            backgroundColor: Colors.white,
            body: SizedBox(
              width: kFidelityWidth,
              height: kFidelityHeight,
              child: RepaintBoundary(
                child: CustomPaint(
                  size: const Size(kFidelityWidth, kFidelityHeight),
                  painter: ArcsPainter(),
                ),
              ),
            ),
          ),
        ),
      );
      await tester.pumpAndSettle();
      await captureAndSave(tester, '${goldensDir.path}/canvas_api_arcs.png');
    });

    testWidgets('canvas_api_paths', (WidgetTester tester) async {
      await tester.binding.setSurfaceSize(const Size(kFidelityWidth, kFidelityHeight));
      await tester.pumpWidget(
        MaterialApp(
          debugShowCheckedModeBanner: false,
          home: Scaffold(
            backgroundColor: Colors.white,
            body: SizedBox(
              width: kFidelityWidth,
              height: kFidelityHeight,
              child: RepaintBoundary(
                child: CustomPaint(
                  size: const Size(kFidelityWidth, kFidelityHeight),
                  painter: PathsPainter(),
                ),
              ),
            ),
          ),
        ),
      );
      await tester.pumpAndSettle();
      await captureAndSave(tester, '${goldensDir.path}/canvas_api_paths.png');
    });

    testWidgets('canvas_api_points', (WidgetTester tester) async {
      await tester.binding.setSurfaceSize(const Size(kFidelityWidth, kFidelityHeight));
      await tester.pumpWidget(
        MaterialApp(
          debugShowCheckedModeBanner: false,
          home: Scaffold(
            backgroundColor: Colors.white,
            body: SizedBox(
              width: kFidelityWidth,
              height: kFidelityHeight,
              child: RepaintBoundary(
                child: CustomPaint(
                  size: const Size(kFidelityWidth, kFidelityHeight),
                  painter: PointsPainter(),
                ),
              ),
            ),
          ),
        ),
      );
      await tester.pumpAndSettle();
      await captureAndSave(tester, '${goldensDir.path}/canvas_api_points.png');
    });

    testWidgets('canvas_api_clipping', (WidgetTester tester) async {
      await tester.binding.setSurfaceSize(const Size(kFidelityWidth, kFidelityHeight));
      await tester.pumpWidget(
        MaterialApp(
          debugShowCheckedModeBanner: false,
          home: Scaffold(
            backgroundColor: Colors.white,
            body: SizedBox(
              width: kFidelityWidth,
              height: kFidelityHeight,
              child: RepaintBoundary(
                child: CustomPaint(
                  size: const Size(kFidelityWidth, kFidelityHeight),
                  painter: ClippingPainter(),
                ),
              ),
            ),
          ),
        ),
      );
      await tester.pumpAndSettle();
      await captureAndSave(tester, '${goldensDir.path}/canvas_api_clipping.png');
    });

    testWidgets('canvas_api_paint_styles', (WidgetTester tester) async {
      await tester.binding.setSurfaceSize(const Size(kFidelityWidth, kFidelityHeight));
      await tester.pumpWidget(
        MaterialApp(
          debugShowCheckedModeBanner: false,
          home: Scaffold(
            backgroundColor: Colors.white,
            body: SizedBox(
              width: kFidelityWidth,
              height: kFidelityHeight,
              child: RepaintBoundary(
                child: CustomPaint(
                  size: const Size(kFidelityWidth, kFidelityHeight),
                  painter: PaintStylesPainter(),
                ),
              ),
            ),
          ),
        ),
      );
      await tester.pumpAndSettle();
      await captureAndSave(tester, '${goldensDir.path}/canvas_api_paint_styles.png');
    });

    testWidgets('canvas_api_save_layer', (WidgetTester tester) async {
      await tester.binding.setSurfaceSize(const Size(kFidelityWidth, kFidelityHeight));
      await tester.pumpWidget(
        MaterialApp(
          debugShowCheckedModeBanner: false,
          home: Scaffold(
            backgroundColor: Colors.white,
            body: SizedBox(
              width: kFidelityWidth,
              height: kFidelityHeight,
              child: RepaintBoundary(
                child: CustomPaint(
                  size: const Size(kFidelityWidth, kFidelityHeight),
                  painter: SaveLayerPainter(),
                ),
              ),
            ),
          ),
        ),
      );
      await tester.pumpAndSettle();
      await captureAndSave(tester, '${goldensDir.path}/canvas_api_save_layer.png');
    });

    testWidgets('canvas_api_draw_color', (WidgetTester tester) async {
      await tester.binding.setSurfaceSize(const Size(kFidelityWidth, kFidelityHeight));
      await tester.pumpWidget(
        MaterialApp(
          debugShowCheckedModeBanner: false,
          home: Scaffold(
            backgroundColor: Colors.white,
            body: SizedBox(
              width: kFidelityWidth,
              height: kFidelityHeight,
              child: RepaintBoundary(
                child: CustomPaint(
                  size: const Size(kFidelityWidth, kFidelityHeight),
                  painter: DrawColorPainter(),
                ),
              ),
            ),
          ),
        ),
      );
      await tester.pumpAndSettle();
      await captureAndSave(tester, '${goldensDir.path}/canvas_api_draw_color.png');
    });

    testWidgets('canvas_api_skew', (WidgetTester tester) async {
      await tester.binding.setSurfaceSize(const Size(kFidelityWidth, kFidelityHeight));
      await tester.pumpWidget(
        MaterialApp(
          debugShowCheckedModeBanner: false,
          home: Scaffold(
            backgroundColor: Colors.white,
            body: SizedBox(
              width: kFidelityWidth,
              height: kFidelityHeight,
              child: RepaintBoundary(
                child: CustomPaint(
                  size: const Size(kFidelityWidth, kFidelityHeight),
                  painter: SkewPainter(),
                ),
              ),
            ),
          ),
        ),
      );
      await tester.pumpAndSettle();
      await captureAndSave(tester, '${goldensDir.path}/canvas_api_skew.png');
    });

    testWidgets('canvas_api_path_arc', (WidgetTester tester) async {
      await tester.binding.setSurfaceSize(const Size(kFidelityWidth, kFidelityHeight));
      await tester.pumpWidget(
        MaterialApp(
          debugShowCheckedModeBanner: false,
          home: Scaffold(
            backgroundColor: Colors.white,
            body: SizedBox(
              width: kFidelityWidth,
              height: kFidelityHeight,
              child: RepaintBoundary(
                child: CustomPaint(
                  size: const Size(kFidelityWidth, kFidelityHeight),
                  painter: PathArcPainter(),
                ),
              ),
            ),
          ),
        ),
      );
      await tester.pumpAndSettle();
      await captureAndSave(tester, '${goldensDir.path}/canvas_api_path_arc.png');
    });

    testWidgets('canvas_api_draw_paint', (WidgetTester tester) async {
      await tester.binding.setSurfaceSize(const Size(kFidelityWidth, kFidelityHeight));
      await tester.pumpWidget(
        MaterialApp(
          debugShowCheckedModeBanner: false,
          home: Scaffold(
            backgroundColor: Colors.white,
            body: SizedBox(
              width: kFidelityWidth,
              height: kFidelityHeight,
              child: RepaintBoundary(
                child: CustomPaint(
                  size: const Size(kFidelityWidth, kFidelityHeight),
                  painter: DrawPaintPainter(),
                ),
              ),
            ),
          ),
        ),
      );
      await tester.pumpAndSettle();
      await captureAndSave(tester, '${goldensDir.path}/canvas_api_draw_paint.png');
    });

    testWidgets('canvas_api_rrect_complex', (WidgetTester tester) async {
      await tester.binding.setSurfaceSize(const Size(kFidelityWidth, kFidelityHeight));
      await tester.pumpWidget(
        MaterialApp(
          debugShowCheckedModeBanner: false,
          home: Scaffold(
            backgroundColor: Colors.white,
            body: SizedBox(
              width: kFidelityWidth,
              height: kFidelityHeight,
              child: RepaintBoundary(
                child: CustomPaint(
                  size: const Size(kFidelityWidth, kFidelityHeight),
                  painter: RRectComplexPainter(),
                ),
              ),
            ),
          ),
        ),
      );
      await tester.pumpAndSettle();
      await captureAndSave(tester, '${goldensDir.path}/canvas_api_rrect_complex.png');
    });

    testWidgets('canvas_api_blend_modes', (WidgetTester tester) async {
      await tester.binding.setSurfaceSize(const Size(kFidelityWidth, kFidelityHeight));
      await tester.pumpWidget(
        MaterialApp(
          debugShowCheckedModeBanner: false,
          home: Scaffold(
            backgroundColor: Colors.white,
            body: SizedBox(
              width: kFidelityWidth,
              height: kFidelityHeight,
              child: RepaintBoundary(
                child: CustomPaint(
                  size: const Size(kFidelityWidth, kFidelityHeight),
                  painter: BlendModesPainter(),
                ),
              ),
            ),
          ),
        ),
      );
      await tester.pumpAndSettle();
      await captureAndSave(tester, '${goldensDir.path}/canvas_api_blend_modes.png');
    });

    testWidgets('canvas_api_blend_modes_extended', (WidgetTester tester) async {
      await tester.binding.setSurfaceSize(const Size(kFidelityWidth, kFidelityHeight));
      await tester.pumpWidget(
        MaterialApp(
          debugShowCheckedModeBanner: false,
          home: Scaffold(
            backgroundColor: Colors.white,
            body: SizedBox(
              width: kFidelityWidth,
              height: kFidelityHeight,
              child: RepaintBoundary(
                child: CustomPaint(
                  size: const Size(kFidelityWidth, kFidelityHeight),
                  painter: BlendModesExtendedPainter(),
                ),
              ),
            ),
          ),
        ),
      );
      await tester.pumpAndSettle();
      await captureAndSave(tester, '${goldensDir.path}/canvas_api_blend_modes_extended.png');
    });

    testWidgets('canvas_api_clip_oval', (WidgetTester tester) async {
      await tester.binding.setSurfaceSize(const Size(kFidelityWidth, kFidelityHeight));
      await tester.pumpWidget(
        MaterialApp(
          debugShowCheckedModeBanner: false,
          home: Scaffold(
            backgroundColor: Colors.white,
            body: SizedBox(
              width: kFidelityWidth,
              height: kFidelityHeight,
              child: RepaintBoundary(
                child: CustomPaint(
                  size: const Size(kFidelityWidth, kFidelityHeight),
                  painter: ClipOvalPainter(),
                ),
              ),
            ),
          ),
        ),
      );
      await tester.pumpAndSettle();
      await captureAndSave(tester, '${goldensDir.path}/canvas_api_clip_oval.png');
    });
  });

  // Widget tests (layout + transform + text)
  group('Visual Golden PNGs - Widget Tests', () {

    testWidgets('widget_text_column', (WidgetTester tester) async {
      await tester.binding.setSurfaceSize(const Size(kFidelityWidth, kFidelityHeight));
      await tester.pumpWidget(
        MaterialApp(
          debugShowCheckedModeBanner: false,
          theme: ThemeData(fontFamily: kTestFontFamily),
          home: Scaffold(
            backgroundColor: Colors.white,
            body: SizedBox(
              width: kFidelityWidth,
              height: kFidelityHeight,
              child: RepaintBoundary(
                child: Column(
                  crossAxisAlignment: CrossAxisAlignment.start,
                  children: [
                    Padding(
                      padding: const EdgeInsets.symmetric(vertical: 20, horizontal: 40),
                      child: Text('Widget Text Test',
                        style: const TextStyle(
                          color: Color(0xFF2196F3),
                          fontSize: 48,
                          fontWeight: FontWeight.bold,
                          fontFamily: kTestFontFamily,
                        ),
                      ),
                    ),
                    Padding(
                      padding: const EdgeInsets.symmetric(vertical: 8, horizontal: 40),
                      child: Text('Subtitle in regular weight',
                        style: const TextStyle(
                          color: Colors.black,
                          fontSize: 28,
                          fontFamily: kTestFontFamily,
                        ),
                      ),
                    ),
                    Padding(
                      padding: const EdgeInsets.symmetric(vertical: 8, horizontal: 40),
                      child: Text('Body text at 20px in grey color for readability',
                        style: const TextStyle(
                          color: Color(0xFF777777),
                          fontSize: 20,
                          fontFamily: kTestFontFamily,
                        ),
                      ),
                    ),
                    Padding(
                      padding: const EdgeInsets.symmetric(vertical: 8, horizontal: 40),
                      child: Text('Bold red accent text',
                        style: const TextStyle(
                          color: Color(0xFFF44336),
                          fontSize: 24,
                          fontWeight: FontWeight.bold,
                          fontFamily: kTestFontFamily,
                        ),
                      ),
                    ),
                    Padding(
                      padding: const EdgeInsets.symmetric(vertical: 8, horizontal: 40),
                      child: Text('Small caption — 14px Roboto',
                        style: const TextStyle(
                          color: Color(0xFF999999),
                          fontSize: 14,
                          fontFamily: kTestFontFamily,
                        ),
                      ),
                    ),
                  ],
                ),
              ),
            ),
          ),
        ),
      );
      await tester.pumpAndSettle();
      await captureAndSave(tester, '${goldensDir.path}/widget_text_column.png');
    });

    testWidgets('widget_transform', (WidgetTester tester) async {
      await tester.binding.setSurfaceSize(const Size(kFidelityWidth, kFidelityHeight));
      await tester.pumpWidget(
        MaterialApp(
          debugShowCheckedModeBanner: false,
          home: Scaffold(
            backgroundColor: Colors.white,
            body: SizedBox(
              width: kFidelityWidth,
              height: kFidelityHeight,
              child: RepaintBoundary(
                child: Column(
                  crossAxisAlignment: CrossAxisAlignment.center,
                  children: [
                    // Row 1: no transform (reference)
                    Padding(
                      padding: const EdgeInsets.all(20),
                      child: Container(width: 300, height: 80,
                        color: const Color(0xFFE0E0E0)),
                    ),
                    // Row 2: translate right 60px
                    Padding(
                      padding: const EdgeInsets.all(20),
                      child: Transform.translate(
                        offset: const Offset(60, 0),
                        child: Container(width: 300, height: 80,
                          color: const Color(0xFF2196F3)),
                      ),
                    ),
                    // Row 3: scale 0.6× (center alignment)
                    Padding(
                      padding: const EdgeInsets.all(20),
                      child: Transform.scale(
                        scale: 0.6,
                        child: Container(width: 300, height: 80,
                          color: const Color(0xFFF44336)),
                      ),
                    ),
                    // Row 4: non-uniform scale (1.4×, 0.7×)
                    Padding(
                      padding: const EdgeInsets.all(20),
                      child: Transform.scale(
                        scaleX: 1.4,
                        scaleY: 0.7,
                        child: Container(width: 300, height: 80,
                          color: const Color(0xFF4CAF50)),
                      ),
                    ),
                    // Row 5: translate -40px + scale 1.2× (nested)
                    Padding(
                      padding: const EdgeInsets.all(20),
                      child: Transform.translate(
                        offset: const Offset(-40, 0),
                        child: Transform.scale(
                          scale: 1.2,
                          child: Container(width: 300, height: 80,
                            color: const Color(0xFFFF9800)),
                        ),
                      ),
                    ),
                  ],
                ),
              ),
            ),
          ),
        ),
      );
      await tester.pumpAndSettle();
      await captureAndSave(tester, '${goldensDir.path}/widget_transform.png');
    });
  });

  // Image widget tests
  group('Visual Golden PNGs - Image Widget Tests', () {
    
    testWidgets('image_explicit_size', (WidgetTester tester) async {
      await tester.binding.setSurfaceSize(const Size(kFidelityWidth, kFidelityHeight));
      await tester.pumpWidget(
        MaterialApp(
          debugShowCheckedModeBanner: false,
          home: Scaffold(
            backgroundColor: Colors.white,
            body: SizedBox(
              width: kFidelityWidth,
              height: kFidelityHeight,
              child: RepaintBoundary(
                child: Container(
                  color: Colors.grey.shade200,
                  child: Center(
                    child: Container(
                      width: 400,
                      height: 300,
                      color: Colors.blue,
                    ),
                  ),
                ),
              ),
            ),
          ),
        ),
      );
      
      await tester.pumpAndSettle();
      await captureAndSave(tester, '${goldensDir.path}/image_explicit_size.png');
    });

    testWidgets('image_fit_contain', (WidgetTester tester) async {
      await tester.binding.setSurfaceSize(const Size(kFidelityWidth, kFidelityHeight));
      await tester.pumpWidget(
        MaterialApp(
          debugShowCheckedModeBanner: false,
          home: Scaffold(
            backgroundColor: Colors.white,
            body: SizedBox(
              width: kFidelityWidth,
              height: kFidelityHeight,
              child: RepaintBoundary(
                child: Padding(
                  padding: const EdgeInsets.all(50.0),
                  child: Container(
                    color: Colors.grey.shade200,
                    child: Center(
                      child: Container(
                        width: 826.67,
                        height: 620,
                        color: Colors.green,
                      ),
                    ),
                  ),
                ),
              ),
            ),
          ),
        ),
      );
      
      await tester.pumpAndSettle();
      await captureAndSave(tester, '${goldensDir.path}/image_fit_contain.png');
    });

    testWidgets('image_fit_cover', (WidgetTester tester) async {
      await tester.binding.setSurfaceSize(const Size(kFidelityWidth, kFidelityHeight));
      await tester.pumpWidget(
        MaterialApp(
          debugShowCheckedModeBanner: false,
          home: Scaffold(
            backgroundColor: Colors.white,
            body: SizedBox(
              width: kFidelityWidth,
              height: kFidelityHeight,
              child: RepaintBoundary(
                child: Padding(
                  padding: const EdgeInsets.all(50.0),
                  child: Container(
                    color: Colors.red,
                    child: Center(
                      child: Container(
                        width: 885,
                        height: 620,
                        color: Colors.blue,
                      ),
                    ),
                  ),
                ),
              ),
            ),
          ),
        ),
      );
      
      await tester.pumpAndSettle();
      await captureAndSave(tester, '${goldensDir.path}/image_fit_cover.png');
    });

    testWidgets('image_fit_fill', (WidgetTester tester) async {
      await tester.binding.setSurfaceSize(const Size(kFidelityWidth, kFidelityHeight));
      await tester.pumpWidget(
        MaterialApp(
          debugShowCheckedModeBanner: false,
          home: Scaffold(
            backgroundColor: Colors.white,
            body: SizedBox(
              width: kFidelityWidth,
              height: kFidelityHeight,
              child: RepaintBoundary(
                child: Padding(
                  padding: const EdgeInsets.all(50.0),
                  child: Container(
                    color: Colors.purple,
                  ),
                ),
              ),
            ),
          ),
        ),
      );
      
      await tester.pumpAndSettle();
      await captureAndSave(tester, '${goldensDir.path}/image_fit_fill.png');
    });

    testWidgets('image_grid', (WidgetTester tester) async {
      await tester.binding.setSurfaceSize(const Size(kFidelityWidth, kFidelityHeight));
      await tester.pumpWidget(
        MaterialApp(
          debugShowCheckedModeBanner: false,
          home: Scaffold(
            backgroundColor: Colors.white,
            body: SizedBox(
              width: kFidelityWidth,
              height: kFidelityHeight,
              child: RepaintBoundary(
                child: Column(
                  children: [
                    Expanded(
                      child: Row(
                        children: [
                          Expanded(child: Padding(padding: const EdgeInsets.all(10), child: Container(color: Colors.red))),
                          Expanded(child: Padding(padding: const EdgeInsets.all(10), child: Container(color: Colors.green))),
                          Expanded(child: Padding(padding: const EdgeInsets.all(10), child: Container(color: Colors.blue))),
                        ],
                      ),
                    ),
                    Expanded(
                      child: Row(
                        children: [
                          Expanded(child: Padding(padding: const EdgeInsets.all(10), child: Container(color: Colors.orange))),
                          Expanded(child: Padding(padding: const EdgeInsets.all(10), child: Container(color: Colors.purple))),
                          Expanded(child: Padding(padding: const EdgeInsets.all(10), child: Container(color: Colors.cyan))),
                        ],
                      ),
                    ),
                  ],
                ),
              ),
            ),
          ),
        ),
      );
      
      await tester.pumpAndSettle();
      await captureAndSave(tester, '${goldensDir.path}/image_grid.png');
    });

    testWidgets('image_with_opacity', (WidgetTester tester) async {
      await tester.binding.setSurfaceSize(const Size(kFidelityWidth, kFidelityHeight));
      await tester.pumpWidget(
        MaterialApp(
          debugShowCheckedModeBanner: false,
          home: Scaffold(
            backgroundColor: Colors.white,
            body: SizedBox(
              width: kFidelityWidth,
              height: kFidelityHeight,
              child: RepaintBoundary(
                child: Stack(
                  children: [
                    Container(color: const Color(0xFF333333)),
                    Center(
                      child: Opacity(
                        opacity: 0.5,
                        child: Container(
                          width: 600,
                          height: 400,
                          color: Colors.blue,
                        ),
                      ),
                    ),
                  ],
                ),
              ),
            ),
          ),
        ),
      );
      
      await tester.pumpAndSettle();
      await captureAndSave(tester, '${goldensDir.path}/image_with_opacity.png');
    });

    testWidgets('image_alignment_variations', (WidgetTester tester) async {
      await tester.binding.setSurfaceSize(const Size(kFidelityWidth, kFidelityHeight));
      await tester.pumpWidget(
        MaterialApp(
          debugShowCheckedModeBanner: false,
          home: Scaffold(
            backgroundColor: Colors.white,
            body: SizedBox(
              width: kFidelityWidth,
              height: kFidelityHeight,
              child: RepaintBoundary(
                child: Column(
                  children: [
                    Expanded(
                      child: Row(
                        children: [
                          Expanded(
                            child: Container(
                              color: Colors.grey.shade200,
                              child: Align(
                                alignment: Alignment.topLeft,
                                child: Container(width: 150, height: 100, color: Colors.red),
                              ),
                            ),
                          ),
                          Expanded(
                            child: Container(
                              color: Colors.grey.shade200,
                              child: Align(
                                alignment: Alignment.topCenter,
                                child: Container(width: 150, height: 100, color: Colors.red),
                              ),
                            ),
                          ),
                          Expanded(
                            child: Container(
                              color: Colors.grey.shade200,
                              child: Align(
                                alignment: Alignment.topRight,
                                child: Container(width: 150, height: 100, color: Colors.red),
                              ),
                            ),
                          ),
                        ],
                      ),
                    ),
                    Expanded(
                      child: Row(
                        children: [
                          Expanded(
                            child: Container(
                              color: Colors.grey.shade200,
                              child: Align(
                                alignment: Alignment.centerLeft,
                                child: Container(width: 150, height: 100, color: Colors.green),
                              ),
                            ),
                          ),
                          Expanded(
                            child: Container(
                              color: Colors.grey.shade200,
                              child: Align(
                                alignment: Alignment.center,
                                child: Container(width: 150, height: 100, color: Colors.green),
                              ),
                            ),
                          ),
                          Expanded(
                            child: Container(
                              color: Colors.grey.shade200,
                              child: Align(
                                alignment: Alignment.centerRight,
                                child: Container(width: 150, height: 100, color: Colors.green),
                              ),
                            ),
                          ),
                        ],
                      ),
                    ),
                  ],
                ),
              ),
            ),
          ),
        ),
      );
      
      await tester.pumpAndSettle();
      await captureAndSave(tester, '${goldensDir.path}/image_alignment_variations.png');
    });
  });

  // Real Image tests with actual downloaded images
  group('Visual Golden PNGs - Real Image Tests', () {
    
    // Image.asset decoding runs on a background isolate outside the normal
    // frame pipeline, so pumpAndSettle() alone does not reliably wait for
    // the *first* decode of a given asset — the very first golden in this
    // group would intermittently capture before the image finished loading,
    // painting only the placeholder Container background. Precache every
    // sample image once, up front, so all tests below hit an already-warm
    // ImageCache (which persists for the lifetime of this test process).
    testWidgets('warm up image cache', (WidgetTester tester) async {
      await tester.pumpWidget(const MaterialApp(home: SizedBox()));
      final context = tester.element(find.byType(SizedBox));
      // flutter test runs widget code in a fake-async zone that cannot
      // progress genuine I/O (asset-bundle reads, JPEG decode) — awaiting
      // precacheImage() directly here would hang forever. runAsync() steps
      // out into the real async zone so the Future can actually complete.
      await tester.runAsync(() async {
        for (final asset in ['sample1.jpg', 'sample2.jpg', 'sample3.jpg', 'mountains.jpg']) {
          await precacheImage(AssetImage('assets/images/$asset'), context);
        }
      });
      await tester.pumpAndSettle();
    });

    testWidgets('real_image_explicit_size', (WidgetTester tester) async {
      await tester.binding.setSurfaceSize(const Size(kFidelityWidth, kFidelityHeight));

      await tester.pumpWidget(
        MaterialApp(
          debugShowCheckedModeBanner: false,
          home: Scaffold(
            backgroundColor: Colors.white,
            body: SizedBox(
              width: kFidelityWidth,
              height: kFidelityHeight,
              child: RepaintBoundary(
                child: Padding(
                  padding: const EdgeInsets.fromLTRB(440, 210, 440, 210),
                  child: Container(
                    color: Colors.grey.shade300,
                    child: Image.asset(
                      'assets/images/sample1.jpg',
                      fit: BoxFit.fill,
                    ),
                  ),
                ),
              ),
            ),
          ),
        ),
      );
      
      // Longer wait for image loading
      await tester.pumpAndSettle(const Duration(seconds: 5));
      await captureAndSave(tester, '${goldensDir.path}/real_image_explicit_size.png');
    });

    testWidgets('real_image_fit_contain', (WidgetTester tester) async {
      await tester.binding.setSurfaceSize(const Size(kFidelityWidth, kFidelityHeight));
      
      await tester.pumpWidget(
        MaterialApp(
          debugShowCheckedModeBanner: false,
          home: Scaffold(
            backgroundColor: Colors.white,
            body: SizedBox(
              width: kFidelityWidth,
              height: kFidelityHeight,
              child: RepaintBoundary(
                child: Padding(
                  padding: const EdgeInsets.all(50.0),
                  child: Container(
                    color: Colors.grey.shade300,
                    child: Image.asset(
                      'assets/images/sample1.jpg',
                      fit: BoxFit.contain,
                    ),
                  ),
                ),
              ),
            ),
          ),
        ),
      );
      
      // Wait for image to load - critical for asset images
      await tester.pump();
      await tester.pumpAndSettle(const Duration(seconds: 3));
      await captureAndSave(tester, '${goldensDir.path}/real_image_fit_contain.png');
    });

    testWidgets('real_image_fit_cover', (WidgetTester tester) async {
      await tester.binding.setSurfaceSize(const Size(kFidelityWidth, kFidelityHeight));
      
      await tester.pumpWidget(
        MaterialApp(
          debugShowCheckedModeBanner: false,
          home: Scaffold(
            backgroundColor: Colors.white,
            body: SizedBox(
              width: kFidelityWidth,
              height: kFidelityHeight,
              child: RepaintBoundary(
                child: Padding(
                  padding: const EdgeInsets.all(50.0),
                  child: Container(
                    color: Colors.grey.shade300,
                    child: Image.asset(
                      'assets/images/sample1.jpg',  // Use sample1.jpg like other tests
                      fit: BoxFit.cover,
                    ),
                  ),
                ),
              ),
            ),
          ),
        ),
      );
      
      // Wait for image to load - use multiple pumps to ensure async image loading completes
      for (int i = 0; i < 10; i++) {
        await tester.pump(const Duration(milliseconds: 200));
      }
      await captureAndSave(tester, '${goldensDir.path}/real_image_fit_cover.png');
    });

    testWidgets('real_image_gallery', (WidgetTester tester) async {
      await tester.binding.setSurfaceSize(const Size(kFidelityWidth, kFidelityHeight));
      
      await tester.pumpWidget(
        MaterialApp(
          debugShowCheckedModeBanner: false,
          home: Scaffold(
            backgroundColor: Colors.white,
            body: SizedBox(
              width: kFidelityWidth,
              height: kFidelityHeight,
              child: RepaintBoundary(
                child: Row(
                  children: [
                    Expanded(
                      child: Padding(
                        padding: const EdgeInsets.all(20.0),
                        child: Image.asset(
                          'assets/images/sample1.jpg',
                          fit: BoxFit.cover,
                        ),
                      ),
                    ),
                    Expanded(
                      child: Padding(
                        padding: const EdgeInsets.all(20.0),
                        child: Image.asset(
                          'assets/images/sample2.jpg',
                          fit: BoxFit.cover,
                        ),
                      ),
                    ),
                    Expanded(
                      child: Padding(
                        padding: const EdgeInsets.all(20.0),
                        child: Image.asset(
                          'assets/images/sample3.jpg',
                          fit: BoxFit.cover,
                        ),
                      ),
                    ),
                  ],
                ),
              ),
            ),
          ),
        ),
      );
      
      // Wait for images to load
      await tester.pumpAndSettle(const Duration(seconds: 2));
      await captureAndSave(tester, '${goldensDir.path}/real_image_gallery.png');
    });

    testWidgets('real_image_with_opacity', (WidgetTester tester) async {
      await tester.binding.setSurfaceSize(const Size(kFidelityWidth, kFidelityHeight));
      
      await tester.pumpWidget(
        MaterialApp(
          debugShowCheckedModeBanner: false,
          home: Scaffold(
            backgroundColor: Colors.white,
            body: SizedBox(
              width: kFidelityWidth,
              height: kFidelityHeight,
              child: RepaintBoundary(
                child: Stack(
                  children: [
                    Container(color: const Color(0xFF334455)),
                    Center(
                      child: Opacity(
                        opacity: 0.6,
                        child: Image.asset(
                          'assets/images/sample3.jpg',
                          width: 600,
                          height: 450,
                          fit: BoxFit.fill,
                        ),
                      ),
                    ),
                  ],
                ),
              ),
            ),
          ),
        ),
      );
      
      // Wait for image to load
      await tester.pumpAndSettle(const Duration(seconds: 2));
      await captureAndSave(tester, '${goldensDir.path}/real_image_with_opacity.png');
    });

    testWidgets('real_image_boxfit_modes', (WidgetTester tester) async {
      await tester.binding.setSurfaceSize(const Size(kFidelityWidth, kFidelityHeight));

      // sample1.jpg is 400x300 (4:3) — a 260x180 box (also ~4:3 but not
      // identical, and smaller than the source in both axes) is small
      // enough to force real scaling/cropping/clipping decisions for every
      // fit mode, unlike a box that happens to share the image's exact
      // aspect ratio (which would make several modes look identical).
      Widget cell(double left, double top, BoxFit fit) {
        return Positioned(
          left: left,
          top: top,
          width: 260,
          height: 180,
          child: Container(
            color: Colors.grey.shade300,
            child: ClipRect(
              child: Image.asset(
                'assets/images/sample1.jpg',
                fit: fit,
              ),
            ),
          ),
        );
      }

      await tester.pumpWidget(
        MaterialApp(
          debugShowCheckedModeBanner: false,
          home: Scaffold(
            backgroundColor: Colors.white,
            body: SizedBox(
              width: kFidelityWidth,
              height: kFidelityHeight,
              child: RepaintBoundary(
                child: Stack(
                  children: [
                    cell(40, 40, BoxFit.fill),
                    cell(340, 40, BoxFit.contain),
                    cell(640, 40, BoxFit.cover),
                    cell(940, 40, BoxFit.fitWidth),
                    cell(40, 280, BoxFit.fitHeight),
                    cell(340, 280, BoxFit.none),
                    cell(640, 280, BoxFit.scaleDown),
                  ],
                ),
              ),
            ),
          ),
        ),
      );

      await tester.pumpAndSettle(const Duration(seconds: 2));
      await captureAndSave(tester, '${goldensDir.path}/real_image_boxfit_modes.png');
    });

    testWidgets('real_image_boxfit_scaledown_cap', (WidgetTester tester) async {
      await tester.binding.setSurfaceSize(const Size(kFidelityWidth, kFidelityHeight));

      // sample1.jpg is 400x300 — a 550x450 box is larger than the source
      // in both axes, so BoxFit.contain will upscale it (filling the box
      // uniformly) while BoxFit.scaleDown must not — this is the one
      // behavioral difference between the two modes, and the only way to
      // exercise it (a box smaller than the source makes them identical).
      Widget cell(double left, double top, BoxFit fit) {
        return Positioned(
          left: left,
          top: top,
          width: 550,
          height: 450,
          child: Container(
            color: Colors.grey.shade300,
            child: ClipRect(
              child: Image.asset(
                'assets/images/sample1.jpg',
                fit: fit,
              ),
            ),
          ),
        );
      }

      await tester.pumpWidget(
        MaterialApp(
          debugShowCheckedModeBanner: false,
          home: Scaffold(
            backgroundColor: Colors.white,
            body: SizedBox(
              width: kFidelityWidth,
              height: kFidelityHeight,
              child: RepaintBoundary(
                child: Stack(
                  children: [
                    cell(40, 100, BoxFit.contain),
                    cell(650, 100, BoxFit.scaleDown),
                  ],
                ),
              ),
            ),
          ),
        ),
      );

      await tester.pumpAndSettle(const Duration(seconds: 2));
      await captureAndSave(tester, '${goldensDir.path}/real_image_boxfit_scaledown_cap.png');
    });

    testWidgets('real_image_boxfit_gallery_replica', (WidgetTester tester) async {
      await tester.binding.setSurfaceSize(const Size(kFidelityWidth, kFidelityHeight));

      // Mirrors examples/gallery/gallery_app.cpp's fitSample() exactly:
      // a 120x120 Container (same background color), an 8px ClipRRect,
      // and mountains.jpg (960x642) as the source — not the smaller,
      // more generic sample1.jpg used by the other BoxFit goldens above.
      const boxSize = 120.0;
      Widget fitSample(double left, double top, BoxFit fit) {
        return Positioned(
          left: left,
          top: top,
          width: boxSize,
          height: boxSize,
          child: ClipRRect(
            borderRadius: BorderRadius.circular(8.0),
            child: Container(
              width: boxSize,
              height: boxSize,
              color: const Color.fromRGBO(230, 230, 237, 1.0),
              child: Image.asset(
                'assets/images/mountains.jpg',
                width: boxSize,
                height: boxSize,
                fit: fit,
              ),
            ),
          ),
        );
      }

      await tester.pumpWidget(
        MaterialApp(
          debugShowCheckedModeBanner: false,
          home: Scaffold(
            backgroundColor: Colors.white,
            body: SizedBox(
              width: kFidelityWidth,
              height: kFidelityHeight,
              child: RepaintBoundary(
                child: Stack(
                  children: [
                    fitSample(40, 40, BoxFit.fill),
                    fitSample(220, 40, BoxFit.contain),
                    fitSample(400, 40, BoxFit.cover),
                    fitSample(580, 40, BoxFit.fitWidth),
                    fitSample(760, 40, BoxFit.fitHeight),
                    fitSample(940, 40, BoxFit.none),
                    fitSample(1120, 40, BoxFit.scaleDown),
                  ],
                ),
              ),
            ),
          ),
        ),
      );

      await tester.pumpAndSettle(const Duration(seconds: 2));
      await captureAndSave(tester, '${goldensDir.path}/real_image_boxfit_gallery_replica.png');
    });
  });
}

/// Captures the first RepaintBoundary in the widget tree and saves as PNG.
Future<void> captureAndSave(WidgetTester tester, String outputPath) async {
  // Find the RepaintBoundary
  final boundary = tester.renderObject<RenderRepaintBoundary>(
    find.byType(RepaintBoundary).first,
  );

  // Capture to image - use runAsync to prevent test hanging
  await tester.runAsync(() async {
    final ui.Image image = await boundary.toImage(pixelRatio: 1.0);
    final ByteData? byteData = await image.toByteData(format: ui.ImageByteFormat.png);

    if (byteData == null) {
      image.dispose();
      throw Exception('Failed to encode image as PNG');
    }

    final bytes = byteData.buffer.asUint8List();
    image.dispose();
    
    // Write to file using synchronous operations
    final file = File(outputPath);
    final raf = file.openSync(mode: FileMode.write);
    try {
      raf.writeFromSync(bytes);
      raf.flushSync();
    } finally {
      raf.closeSync();
    }
    
    print('Generated PNG: $outputPath (${bytes.length} bytes)');
  });
}

// Custom Painters

class BasicShapesPainter extends CustomPainter {
  @override
  void paint(Canvas canvas, Size size) {
    // Scale coordinates for 1280x720 canvas
    final scaleX = size.width / 400.0;
    final scaleY = size.height / 400.0;
    
    canvas.drawRect(Rect.fromLTWH(20 * scaleX, 20 * scaleY, 100 * scaleX, 80 * scaleY), Paint()..color = Colors.blue);
    canvas.drawCircle(Offset(200 * scaleX, 60 * scaleY), 40 * scaleX, Paint()..color = Colors.red);
    canvas.drawOval(Rect.fromLTWH(280 * scaleX, 20 * scaleY, 100 * scaleX, 80 * scaleY), Paint()..color = Colors.green);
    
    final purpleStroke = Paint()
      ..color = Colors.purple
      ..style = PaintingStyle.stroke
      ..strokeWidth = 4 * scaleX;
    canvas.drawRect(Rect.fromLTWH(20 * scaleX, 120 * scaleY, 100 * scaleX, 80 * scaleY), purpleStroke);
  }

  @override
  bool shouldRepaint(covariant CustomPainter oldDelegate) => false;
}

class RoundedRectsPainter extends CustomPainter {
  @override
  void paint(Canvas canvas, Size size) {
    // Scale coordinates for 1280x720 canvas
    final scaleX = size.width / 400.0;
    final scaleY = size.height / 400.0;
    
    final rrect = RRect.fromRectAndRadius(
      Rect.fromLTWH(20 * scaleX, 20 * scaleY, 150 * scaleX, 100 * scaleY),
      Radius.circular(20 * scaleX),
    );
    canvas.drawRRect(rrect, Paint()..color = Colors.blue);

    final greenRRect = RRect.fromRectAndRadius(
      Rect.fromLTWH(200 * scaleX, 20 * scaleY, 150 * scaleX, 100 * scaleY),
      Radius.circular(30 * scaleX),
    );
    canvas.drawRRect(greenRRect, Paint()..color = Colors.green);

    final outerRRect = RRect.fromRectAndRadius(
      Rect.fromLTWH(20 * scaleX, 150 * scaleY, 150 * scaleX, 100 * scaleY),
      Radius.circular(25 * scaleX),
    );
    final innerRRect = RRect.fromRectAndRadius(
      Rect.fromLTWH(35 * scaleX, 165 * scaleY, 120 * scaleX, 70 * scaleY),
      Radius.circular(15 * scaleX),
    );
    canvas.drawDRRect(outerRRect, innerRRect, Paint()..color = Colors.purple);

    final strokeRRect = RRect.fromRectAndRadius(
      Rect.fromLTWH(200 * scaleX, 150 * scaleY, 150 * scaleX, 100 * scaleY),
      Radius.circular(15 * scaleX),
    );
    final redStroke = Paint()
      ..color = Colors.red
      ..style = PaintingStyle.stroke
      ..strokeWidth = 5 * scaleX;
    canvas.drawRRect(strokeRRect, redStroke);
  }

  @override
  bool shouldRepaint(covariant CustomPainter oldDelegate) => false;
}

class LinesPainter extends CustomPainter {
  @override
  void paint(Canvas canvas, Size size) {
    // Scale coordinates for 1280x720 canvas
    final scaleX = size.width / 400.0;
    final scaleY = size.height / 400.0;
    
    final linePaint = Paint()
      ..color = Colors.black
      ..strokeWidth = 2 * scaleX;
    canvas.drawLine(Offset(20 * scaleX, 20 * scaleY), Offset(150 * scaleX, 100 * scaleY), linePaint);
    canvas.drawLine(Offset(200 * scaleX, 20 * scaleY), Offset(350 * scaleX, 150 * scaleY), linePaint);

    final thickPaint = Paint()
      ..color = Colors.orange
      ..strokeWidth = 8 * scaleX;
    canvas.drawLine(Offset(50 * scaleX, 200 * scaleY), Offset(350 * scaleX, 250 * scaleY), thickPaint);

    for (int i = 0; i < 4; i++) {
      final width = (i + 1) * 3.0 * scaleX;
      final paint = Paint()
        ..color = Colors.green
        ..strokeWidth = width;
      final y = 300.0 * scaleY + i * 25 * scaleY;
      canvas.drawLine(Offset(20 * scaleX, y), Offset(150 * scaleX, y), paint);
    }
  }

  @override
  bool shouldRepaint(covariant CustomPainter oldDelegate) => false;
}

class ComplexScenePainter extends CustomPainter {
  @override
  void paint(Canvas canvas, Size size) {
    // Scale coordinates for 1280x720 canvas
    final scaleX = size.width / 400.0;
    final scaleY = size.height / 400.0;

    final bgColors = [Colors.blue.shade100, Colors.purple.shade100];
    for (int i = 0; i < 5; i++) {
      for (int j = 0; j < 5; j++) {
        final paint = Paint()
          ..color = ((i + j) % 2 == 0 ? bgColors[0] : bgColors[1]).withAlpha(76);
        final cx = 40.0 * scaleX + i * 80 * scaleX;
        final cy = 40.0 * scaleY + j * 80 * scaleY;
        canvas.drawCircle(Offset(cx, cy), 50 * scaleX, paint);
      }
    }

    final cardRRect = RRect.fromRectAndRadius(
      Rect.fromLTWH(80 * scaleX, 120 * scaleY, 240 * scaleX, 160 * scaleY),
      Radius.circular(20 * scaleX),
    );

    // Draw inner circles first, then the card on top so the rounded rect
    // visually clips them without relying on translated clipRRect.
    final circleColors = [
      const Color(0xFFF44336),
      const Color(0xFF4CAF50),
      const Color(0xFFFF9800),
    ];
    for (int i = 0; i < 3; i++) {
      final paint = Paint()
        ..color = circleColors[i].withAlpha(153);
      final x = 80.0 * scaleX + 60.0 * scaleX + i * 60 * scaleX;
      final y = 120.0 * scaleY + 80.0 * scaleY;
      canvas.drawCircle(Offset(x, y), 35 * scaleX, paint);
    }

    canvas.drawRRect(cardRRect, Paint()..color = const Color(0xE6FFFFFF));

    final borderPaint = Paint()
      ..color = const Color(0xFF64B5F6)
      ..style = PaintingStyle.stroke
      ..strokeWidth = 3 * scaleX;
    canvas.drawRRect(cardRRect, borderPaint);
  }

  @override
  bool shouldRepaint(covariant CustomPainter oldDelegate) => false;
}

class TextBasicPainter extends CustomPainter {
  @override
  void paint(Canvas canvas, Size size) {
    _drawText(canvas, 'Hello, World!',
      const TextStyle(color: Color(0xFF2196F3), fontSize: 48,
        fontFamily: kTestFontFamily),
      const Offset(50, 50));

    _drawText(canvas, 'Bold text sample',
      const TextStyle(color: Color(0xFFF44336), fontSize: 36,
        fontWeight: FontWeight.bold, fontFamily: kTestFontFamily),
      const Offset(50, 140));

    _drawText(canvas, 'Regular green text',
      const TextStyle(color: Color(0xFF4CAF50), fontSize: 30,
        fontFamily: kTestFontFamily),
      const Offset(50, 230));

    _drawText(canvas, 'Small caption text (18px)',
      const TextStyle(color: Colors.black, fontSize: 18,
        fontFamily: kTestFontFamily),
      const Offset(50, 315));

    _drawText(canvas, 'Large Heading',
      const TextStyle(color: Color(0xFF9C27B0), fontSize: 64,
        fontWeight: FontWeight.bold, fontFamily: kTestFontFamily),
      const Offset(50, 370));
  }

  void _drawText(Canvas canvas, String text, TextStyle style, Offset offset) {
    final tp = TextPainter(
      text: TextSpan(text: text, style: style),
      textDirection: TextDirection.ltr,
    )..layout();
    tp.paint(canvas, offset);
  }

  @override
  bool shouldRepaint(covariant CustomPainter oldDelegate) => false;
}

class TransformsPainter extends CustomPainter {
  @override
  void paint(Canvas canvas, Size size) {
    // Reference circle (no transform)
    canvas.drawCircle(const Offset(100, 130), 60,
      Paint()..color = const Color(0xFFE0E0E0));

    // Translate right 250
    canvas.save();
    canvas.translate(250, 0);
    canvas.drawCircle(const Offset(100, 130), 60,
      Paint()..color = const Color(0xFF2196F3));
    canvas.restore();

    // Translate right + down
    canvas.save();
    canvas.translate(500, 80);
    canvas.drawCircle(const Offset(100, 130), 60,
      Paint()..color = const Color(0xFFF44336));
    canvas.restore();

    // Scale 2× around (900, 130)
    canvas.save();
    canvas.translate(900, 130);
    canvas.scale(2.0);
    canvas.drawRRect(
      RRect.fromRectAndRadius(const Rect.fromLTWH(-40, -40, 80, 80),
        const Radius.circular(10)),
      Paint()..color = const Color(0xFF4CAF50));
    canvas.restore();

    // Scale 0.5× around (1150, 130)
    canvas.save();
    canvas.translate(1150, 130);
    canvas.scale(0.5);
    canvas.drawRRect(
      RRect.fromRectAndRadius(const Rect.fromLTWH(-80, -80, 160, 160),
        const Radius.circular(20)),
      Paint()..color = const Color(0xFFFF9800));
    canvas.restore();

    // Combined: translate + non-uniform scale, circle
    canvas.save();
    canvas.translate(200, 380);
    canvas.scale(1.5, 1.0);
    canvas.drawCircle(Offset.zero, 50,
      Paint()..color = const Color(0xFF9C27B0));
    canvas.restore();

    // Combined: translate + non-uniform scale, rrect
    canvas.save();
    canvas.translate(600, 380);
    canvas.scale(1.0, 1.5);
    canvas.drawRRect(
      RRect.fromRectAndRadius(const Rect.fromLTWH(-50, -40, 100, 80),
        const Radius.circular(12)),
      Paint()..color = const Color(0xFF2196F3));
    canvas.restore();

    // Lines with translate
    canvas.save();
    canvas.translate(900, 450);
    final lp = Paint()..color = Colors.black..strokeWidth = 4;
    canvas.drawLine(const Offset(-80, 0), const Offset(80, 0), lp);
    canvas.drawLine(const Offset(0, -60), const Offset(0, 60), lp);
    canvas.restore();
  }

  @override
  bool shouldRepaint(covariant CustomPainter oldDelegate) => false;
}

class RotatePainter extends CustomPainter {
  static const double pi = 3.14159265358979;

  @override
  void paint(Canvas canvas, Size size) {
    final cx = size.width * 0.5;
    final cy = size.height * 0.5;

    const colors = [
      Color(0xFFF44336), Color(0xFF4CAF50), Color(0xFFFF9800), Color(0xFF9C27B0),
      Color(0xFF2196F3), Color(0xFFF44336), Color(0xFF4CAF50), Color(0xFFFF9800),
    ];

    // Hub circle at center
    canvas.drawCircle(Offset(cx, cy), 20,
      Paint()..color = const Color(0xFF2196F3));

    // 8 spokes + outer circles
    for (int i = 0; i < 8; i++) {
      final angle = i * (pi / 4.0);
      canvas.save();
      canvas.translate(cx, cy);
      canvas.rotate(angle);
      canvas.drawLine(Offset.zero, const Offset(220, 0),
        Paint()..color = colors[i]..strokeWidth = 3);
      canvas.drawCircle(const Offset(240, 0), 28,
        Paint()..color = colors[i]);
      canvas.restore();
    }

    // Inner ring (rotated 22.5°)
    canvas.save();
    canvas.translate(cx, cy);
    canvas.rotate(pi / 8.0);
    for (int i = 0; i < 8; i++) {
      canvas.save();
      canvas.rotate(i * (pi / 4.0));
      canvas.drawCircle(const Offset(110, 0), 14,
        Paint()..color = const Color(0x662196F3));
      canvas.restore();
    }
    canvas.restore();
  }

  @override
  bool shouldRepaint(covariant CustomPainter oldDelegate) => false;
}

class ArcsPainter extends CustomPainter {
  @override
  void paint(Canvas canvas, Size size) {
    final scaleX = size.width / 400.0;
    final scaleY = size.height / 400.0;

    // Blue pie slice (matches C++ Color::fromRGB(0.1294, 0.5882, 0.9529))
    canvas.drawArc(
      Rect.fromLTWH(20 * scaleX, 20 * scaleY, 150 * scaleX, 150 * scaleY),
      0.0, 1.5, true,
      Paint()..color = const Color(0xFF2196F3),
    );

    // Green stroke arc (matches C++ Color::fromRGB(0.2980, 0.6863, 0.3137))
    final greenStroke = Paint()
      ..color = const Color(0xFF4CAF50)
      ..style = PaintingStyle.stroke
      ..strokeWidth = 10 * scaleX;
    canvas.drawArc(
      Rect.fromLTWH(200 * scaleX, 20 * scaleY, 150 * scaleX, 150 * scaleY),
      0.5, 2.0, false,
      greenStroke,
    );

    // Pie chart (matches C++ pure red/orange/yellow/green)
    final colors = [
      const Color(0xFFFF0000),
      const Color(0xFFFFA500),
      const Color(0xFFFFFF00),
      const Color(0xFF00FF00),
    ];
    final values = [0.3, 0.25, 0.2, 0.25];
    var currentAngle = -math.pi / 2.0;
    for (int i = 0; i < 4; i++) {
      final sweep = values[i] * 2.0 * math.pi;
      final cx = 100.0 * scaleX;
      final cy = 280.0 * scaleY;
      final w = 70.0 * scaleX;
      final h = 70.0 * scaleY;
      canvas.drawArc(
        Rect.fromLTWH(cx - w, cy - h, w * 2, h * 2),
        currentAngle, sweep, true,
        Paint()..color = colors[i],
      );
      currentAngle += sweep;
    }
  }

  @override
  bool shouldRepaint(covariant CustomPainter oldDelegate) => false;
}

class PathsPainter extends CustomPainter {
  @override
  void paint(Canvas canvas, Size size) {
    final scaleX = size.width / 400.0;
    final scaleY = size.height / 400.0;

    // Filled triangle (matches C++ Color::fromRGBA(0, 0, 1, 0.7))
    final triangle = Path()
      ..moveTo(100 * scaleX, 50 * scaleY)
      ..lineTo(50 * scaleX, 150 * scaleY)
      ..lineTo(150 * scaleX, 150 * scaleY)
      ..close();
    canvas.drawPath(triangle, Paint()..color = const Color(0xB30000FF));

    // Stroked quadratic (matches C++ Color::fromRGB(0, 1, 0))
    final quad = Path()
      ..moveTo(200 * scaleX, 150 * scaleY)
      ..quadraticBezierTo(250 * scaleX, 50 * scaleY, 300 * scaleX, 150 * scaleY);
    final greenStroke = Paint()
      ..color = const Color(0xFF00FF00)
      ..style = PaintingStyle.stroke
      ..strokeWidth = 3 * scaleX;
    canvas.drawPath(quad, greenStroke);

    // Stroked cubic (matches C++ Color::fromRGB(0.5, 0, 0.5))
    final cubic = Path()
      ..moveTo(50 * scaleX, 250 * scaleY)
      ..cubicTo(
        100 * scaleX, 200 * scaleY,
        150 * scaleX, 300 * scaleY,
        200 * scaleX, 250 * scaleY,
      );
    final purpleStroke = Paint()
      ..color = const Color(0xFF800080)
      ..style = PaintingStyle.stroke
      ..strokeWidth = 4 * scaleX;
    canvas.drawPath(cubic, purpleStroke);
  }

  @override
  bool shouldRepaint(covariant CustomPainter oldDelegate) => false;
}

class PointsPainter extends CustomPainter {
  @override
  void paint(Canvas canvas, Size size) {
    final scaleX = size.width / 400.0;
    final scaleY = size.height / 400.0;

    // Points (matches C++ Color::fromRGB(1, 0, 0))
    final pointPaint = Paint()
      ..color = const Color(0xFFFF0000)
      ..strokeWidth = 8 * scaleX;
    final points = [
      Offset(100 * scaleX, 100 * scaleY),
      Offset(150 * scaleX, 120 * scaleY),
      Offset(200 * scaleX, 110 * scaleY),
      Offset(250 * scaleX, 130 * scaleY),
      Offset(300 * scaleX, 100 * scaleY),
    ];
    canvas.drawPoints(ui.PointMode.points, points, pointPaint);

    // Lines (matches C++ Color::fromRGB(0, 0, 1))
    final linePaint = Paint()
      ..color = const Color(0xFF0000FF)
      ..strokeWidth = 4 * scaleX;
    final linePoints = [
      Offset(50 * scaleX, 200 * scaleY),
      Offset(150 * scaleX, 280 * scaleY),
      Offset(250 * scaleX, 220 * scaleY),
      Offset(350 * scaleX, 300 * scaleY),
    ];
    canvas.drawPoints(ui.PointMode.lines, linePoints, linePaint);

    // Polygon (matches C++ Color::fromRGB(0, 0.5, 0))
    final polyPaint = Paint()
      ..color = const Color(0xFF008000)
      ..strokeWidth = 3 * scaleX;
    final polyPoints = [
      Offset(200 * scaleX, 350 * scaleY),
      Offset(250 * scaleX, 450 * scaleY),
      Offset(150 * scaleX, 450 * scaleY),
    ];
    canvas.drawPoints(ui.PointMode.polygon, polyPoints, polyPaint);
  }

  @override
  bool shouldRepaint(covariant CustomPainter oldDelegate) => false;
}

class ClippingPainter extends CustomPainter {
  @override
  void paint(Canvas canvas, Size size) {
    final scaleX = size.width / 400.0;
    final scaleY = size.height / 400.0;

    // Background (matches C++ Color::fromRGB(1, 0, 0))
    canvas.drawRect(
      Rect.fromLTWH(0, 0, size.width, size.height),
      Paint()..color = const Color(0xFFFF0000),
    );

    // Rect clip (matches C++ Color::fromRGB(0, 0, 1))
    canvas.save();
    canvas.clipRect(Rect.fromLTWH(20 * scaleX, 20 * scaleY, 150 * scaleX, 150 * scaleY));
    canvas.drawCircle(
      Offset(120 * scaleX, 120 * scaleY),
      80 * scaleX,
      Paint()..color = const Color(0xFF0000FF),
    );
    canvas.restore();

    // RRect clip (matches C++ Color::fromRGB(0, 1, 0))
    canvas.save();
    canvas.clipRRect(RRect.fromRectAndRadius(
      Rect.fromLTWH(200 * scaleX, 20 * scaleY, 150 * scaleX, 150 * scaleY),
      Radius.circular(30 * scaleX),
    ));
    canvas.drawCircle(
      Offset(280 * scaleX, 110 * scaleY),
      80 * scaleX,
      Paint()..color = const Color(0xFF00FF00),
    );
    canvas.restore();

    // Path clip (matches C++ Color::fromRGB(1, 1, 0))
    canvas.save();
    final triangle = Path()
      ..moveTo(100 * scaleX, 220 * scaleY)
      ..lineTo(50 * scaleX, 350 * scaleY)
      ..lineTo(150 * scaleX, 350 * scaleY)
      ..close();
    canvas.clipPath(triangle);
    canvas.drawCircle(
      Offset(100 * scaleX, 300 * scaleY),
      80 * scaleX,
      Paint()..color = const Color(0xFFFFFF00),
    );
    canvas.restore();
  }

  @override
  bool shouldRepaint(covariant CustomPainter oldDelegate) => false;
}

class PaintStylesPainter extends CustomPainter {
  @override
  void paint(Canvas canvas, Size size) {
    final scaleX = size.width / 400.0;
    final scaleY = size.height / 400.0;

    // Filled rect (matches C++ Color::fromRGB(0, 0, 1))
    canvas.drawRect(
      Rect.fromLTWH(20 * scaleX, 20 * scaleY, 150 * scaleX, 100 * scaleY),
      Paint()..color = const Color(0xFF0000FF),
    );

    // Stroked rect (matches C++ Color::fromRGB(1, 0, 0))
    final strokePaint = Paint()
      ..color = const Color(0xFFFF0000)
      ..style = PaintingStyle.stroke
      ..strokeWidth = 8 * scaleX;
    canvas.drawRect(
      Rect.fromLTWH(200 * scaleX, 20 * scaleY, 150 * scaleX, 100 * scaleY),
      strokePaint,
    );

    // Thick vs thin strokes (matches C++ Color::fromRGB(0, 0.5, 0))
    for (int i = 0; i < 4; i++) {
      final paint = Paint()
        ..color = const Color(0xFF008000)
        ..style = PaintingStyle.stroke
        ..strokeWidth = (i + 1) * 3.0 * scaleX;
      final y = 150.0 * scaleY + i * 30.0 * scaleY;
      canvas.drawLine(
        Offset(20 * scaleX, y),
        Offset(370 * scaleX, y),
        paint,
      );
    }

    // Opacity (matches C++ Color::fromRGBA(1, 0, 1, 0.5))
    final opacityPaint = Paint()
      ..color = const Color(0x80FF00FF);
    canvas.drawCircle(
      Offset(100 * scaleX, 320 * scaleY),
      50 * scaleX,
      opacityPaint,
    );
  }

  @override
  bool shouldRepaint(covariant CustomPainter oldDelegate) => false;
}

class SaveLayerPainter extends CustomPainter {
  @override
  void paint(Canvas canvas, Size size) {
    final scaleX = size.width / 400.0;
    final scaleY = size.height / 400.0;

    // Background (matches C++ Color::fromRGB(0.9, 0.9, 0.9))
    canvas.drawRect(
      Rect.fromLTWH(0, 0, size.width, size.height),
      Paint()..color = const Color(0xFFE6E6E6),
    );

    // SaveLayer with 50% opacity (matches C++ Color::fromRGBA(1, 1, 1, 0.5))
    canvas.saveLayer(
      Rect.fromLTWH(50 * scaleX, 50 * scaleY, 300 * scaleX, 300 * scaleY),
      Paint()..color = const Color(0x80FFFFFF),
    );

    canvas.drawRect(
      Rect.fromLTWH(50 * scaleX, 50 * scaleY, 150 * scaleX, 150 * scaleY),
      Paint()..color = const Color(0xFFFF0000),
    );

    canvas.drawCircle(
      Offset(250 * scaleX, 200 * scaleY),
      60 * scaleX,
      Paint()..color = const Color(0xFF0000FF),
    );

    canvas.restore();
  }

  @override
  bool shouldRepaint(covariant CustomPainter oldDelegate) => false;
}

class DrawColorPainter extends CustomPainter {
  @override
  void paint(Canvas canvas, Size size) {
    final scaleX = size.width / 400.0;
    final scaleY = size.height / 400.0;

    // Left half: solid red fill via drawColor
    canvas.save();
    canvas.clipRect(Rect.fromLTWH(0, 0, size.width * 0.5, size.height));
    canvas.drawColor(const Color(0xFFFF0000), BlendMode.srcOver);
    canvas.restore();

    // Right half: blue with a green rect drawn on top
    canvas.save();
    canvas.clipRect(Rect.fromLTWH(size.width * 0.5, 0, size.width * 0.5, size.height));
    canvas.drawColor(const Color(0xFF0000FF), BlendMode.srcOver);
    canvas.drawRect(
      Rect.fromLTWH(80 * scaleX, 80 * scaleY, 120 * scaleX, 120 * scaleY),
      Paint()..color = const Color(0xFF00FF00),
    );
    canvas.restore();
  }

  @override
  bool shouldRepaint(covariant CustomPainter oldDelegate) => false;
}

class SkewPainter extends CustomPainter {
  @override
  void paint(Canvas canvas, Size size) {
    final scaleX = size.width / 400.0;
    final scaleY = size.height / 400.0;

    // Horizontal skew
    canvas.save();
    canvas.translate(80 * scaleX, 100 * scaleY);
    canvas.skew(0.3, 0.0);
    canvas.drawRect(
      Rect.fromLTWH(0, 0, 100 * scaleX, 100 * scaleY),
      Paint()..color = const Color(0xFF2196F3),
    );
    canvas.restore();

    // Vertical skew
    canvas.save();
    canvas.translate(220 * scaleX, 80 * scaleY);
    canvas.skew(0.0, 0.25);
    canvas.drawRect(
      Rect.fromLTWH(0, 0, 80 * scaleX, 120 * scaleY),
      Paint()..color = const Color(0xFF4CAF50),
    );
    canvas.restore();

    // Matrix4 rotation + scale via transform()
    canvas.save();
    canvas.translate(320 * scaleX, 200 * scaleY);
    final matrix = Matrix4.rotationZ(0.523599)..scale(1.2, 1.2, 1.0);
    canvas.transform(matrix.storage);
    canvas.drawRect(
      Rect.fromLTWH(-40 * scaleX, -40 * scaleY, 80 * scaleX, 80 * scaleY),
      Paint()..color = const Color(0xFFFF9800),
    );
    canvas.restore();
  }

  @override
  bool shouldRepaint(covariant CustomPainter oldDelegate) => false;
}

class PathArcPainter extends CustomPainter {
  @override
  void paint(Canvas canvas, Size size) {
    final scaleX = size.width / 400.0;
    final scaleY = size.height / 400.0;

    // Filled path built with arcTo (pie slice)
    final pie = Path()
      ..moveTo(100 * scaleX, 100 * scaleY)
      ..arcTo(
        Rect.fromLTWH(50 * scaleX, 50 * scaleY, 100 * scaleX, 100 * scaleY),
        -math.pi / 2.0,
        math.pi * 0.75,
        false,
      )
      ..close();
    canvas.drawPath(pie, Paint()..color = const Color(0xFF2196F3));

    // Stroked open arc
    final openArc = Path()
      ..moveTo(250 * scaleX, 150 * scaleY)
      ..arcTo(
        Rect.fromLTWH(200 * scaleX, 50 * scaleY, 150 * scaleX, 150 * scaleY),
        0.0,
        math.pi * 1.25,
        false,
      );
    canvas.drawPath(
      openArc,
      Paint()
        ..color = const Color(0xFFFF0000)
        ..style = PaintingStyle.stroke
        ..strokeWidth = 6 * scaleX,
    );

    // Full oval built with addArc
    final oval = Path()
      ..addArc(
        Rect.fromLTWH(80 * scaleX, 200 * scaleY, 120 * scaleX, 80 * scaleY),
        0.0,
        math.pi * 2.0,
      );
    canvas.drawPath(
      oval,
      Paint()
        ..color = const Color(0xFF4CAF50)
        ..style = PaintingStyle.stroke
        ..strokeWidth = 4 * scaleX,
    );
  }

  @override
  bool shouldRepaint(covariant CustomPainter oldDelegate) => false;
}

class DrawPaintPainter extends CustomPainter {
  @override
  void paint(Canvas canvas, Size size) {
    final scaleX = size.width / 400.0;
    final scaleY = size.height / 400.0;

    // Left half: cyan fill via drawPaint
    canvas.save();
    canvas.clipRect(Rect.fromLTWH(0, 0, size.width * 0.5, size.height));
    canvas.drawPaint(Paint()..color = const Color(0xFF00FFFF));
    canvas.restore();

    // Right half: yellow fill with a magenta rect on top
    canvas.save();
    canvas.clipRect(Rect.fromLTWH(size.width * 0.5, 0, size.width * 0.5, size.height));
    canvas.drawPaint(Paint()..color = const Color(0xFFFFFF00));
    canvas.drawRect(
      Rect.fromLTWH(80 * scaleX, 80 * scaleY, 120 * scaleX, 120 * scaleY),
      Paint()..color = const Color(0xFFFF00FF),
    );
    canvas.restore();
  }

  @override
  bool shouldRepaint(covariant CustomPainter oldDelegate) => false;
}

class RRectComplexPainter extends CustomPainter {
  @override
  void paint(Canvas canvas, Size size) {
    final scaleX = size.width / 400.0;
    final scaleY = size.height / 400.0;

    final rrect = RRect.fromRectAndCorners(
      Rect.fromLTWH(40 * scaleX, 40 * scaleY, 160 * scaleX, 120 * scaleY),
      topLeft: Radius.circular(30 * scaleX),
      topRight: Radius.circular(20 * scaleX),
      bottomLeft: Radius.circular(25 * scaleX),
      bottomRight: Radius.circular(40 * scaleX),
    );
    canvas.drawRRect(
      rrect,
      Paint()..color = const Color(0xFF2196F3),
    );

    final strokeRRect = RRect.fromRectAndCorners(
      Rect.fromLTWH(220 * scaleX, 40 * scaleY, 160 * scaleX, 120 * scaleY),
      topLeft: Radius.circular(10 * scaleX),
      topRight: Radius.circular(40 * scaleX),
      bottomLeft: Radius.circular(40 * scaleX),
      bottomRight: Radius.circular(10 * scaleX),
    );
    canvas.drawRRect(
      strokeRRect,
      Paint()
        ..color = const Color(0xFFFF0000)
        ..style = PaintingStyle.stroke
        ..strokeWidth = 6 * scaleX,
    );

    // Smaller filled complex rrect in the lower area
    final smallRRect = RRect.fromRectAndCorners(
      Rect.fromLTWH(130 * scaleX, 200 * scaleY, 140 * scaleX, 90 * scaleY),
      topLeft: Radius.zero,
      topRight: Radius.circular(25 * scaleX),
      bottomLeft: Radius.circular(25 * scaleX),
      bottomRight: Radius.zero,
    );
    canvas.drawRRect(
      smallRRect,
      Paint()..color = const Color(0xFF4CAF50),
    );
  }

  @override
  bool shouldRepaint(covariant CustomPainter oldDelegate) => false;
}

class BlendModesPainter extends CustomPainter {
  @override
  void paint(Canvas canvas, Size size) {
    final scaleX = size.width / 400.0;
    final scaleY = size.height / 400.0;

    // Background: three grey rectangles
    for (int i = 0; i < 3; i++) {
      canvas.drawRect(
        Rect.fromLTWH((40 + i * 110) * scaleX, 60 * scaleY, 80 * scaleX, 180 * scaleY),
        Paint()..color = const Color(0xFFAAAAAA),
      );
    }

    // Row 1: srcOver (semi-transparent red, green, blue circles)
    const srcOverColors = [
      Color(0x80FF0000),
      Color(0x8000FF00),
      Color(0x800000FF),
    ];
    for (int i = 0; i < 3; i++) {
      canvas.drawCircle(
        Offset((80 + i * 110) * scaleX, 100 * scaleY),
        35 * scaleX,
        Paint()
          ..color = srcOverColors[i]
          ..blendMode = BlendMode.srcOver,
      );
    }

    // Row 2: modulate (opaque red/green/blue)
    const modulateColors = [
      Colors.red,
      Colors.green,
      Colors.blue,
    ];
    for (int i = 0; i < 3; i++) {
      canvas.drawCircle(
        Offset((80 + i * 110) * scaleX, 170 * scaleY),
        35 * scaleX,
        Paint()
          ..color = modulateColors[i]
          ..blendMode = BlendMode.modulate,
      );
    }

    // Row 3: plus (opaque red/green/blue)
    const plusColors = [
      Colors.red,
      Colors.green,
      Colors.blue,
    ];
    for (int i = 0; i < 3; i++) {
      canvas.drawCircle(
        Offset((80 + i * 110) * scaleX, 240 * scaleY),
        35 * scaleX,
        Paint()
          ..color = plusColors[i]
          ..blendMode = BlendMode.plus,
      );
    }
  }

  @override
  bool shouldRepaint(covariant CustomPainter oldDelegate) => false;
}

class BlendModesExtendedPainter extends CustomPainter {
  @override
  void paint(Canvas canvas, Size size) {
    final scaleX = size.width / 400.0;
    final scaleY = size.height / 400.0;

    // The remaining BlendMode values not covered by BlendModesPainter
    // (srcOver, plus, and modulate are tested there).
    const modes = [
      BlendMode.clear,
      BlendMode.src,
      BlendMode.dst,
      BlendMode.dstOver,
      BlendMode.srcIn,
      BlendMode.dstIn,
      BlendMode.srcOut,
      BlendMode.dstOut,
      BlendMode.srcATop,
      BlendMode.dstATop,
      BlendMode.xor,
    ];

    // 4x3 grid of cells, one blend mode per cell: an opaque orange
    // "destination" square with a semi-transparent blue "source" circle
    // composited on top using the tested blend mode.
    const cellW = 100.0;
    const cellH = 130.0;

    for (int i = 0; i < modes.length; i++) {
      final col = i % 4;
      final row = i ~/ 4;
      final originX = col * cellW;
      final originY = row * cellH;

      canvas.drawRect(
        Rect.fromLTWH(
            (originX + 10) * scaleX, (originY + 15) * scaleY, 55 * scaleX, 55 * scaleY),
        Paint()..color = const Color(0xFFFF9800),
      );

      canvas.drawCircle(
        Offset((originX + 65) * scaleX, (originY + 70) * scaleY),
        28 * scaleX,
        Paint()
          ..color = const Color(0x990000FF)
          ..blendMode = modes[i],
      );
    }
  }

  @override
  bool shouldRepaint(covariant CustomPainter oldDelegate) => false;
}

class ClipOvalPainter extends CustomPainter {
  @override
  void paint(Canvas canvas, Size size) {
    final scaleX = size.width / 400.0;
    final scaleY = size.height / 400.0;

    canvas.drawRect(
      Rect.fromLTWH(0, 0, size.width, size.height),
      Paint()..color = const Color(0xFFE6E6E6),
    );

    // Wide ellipse clip, filled with blue (matches C++ Color::fromRGB(0.1294, 0.5882, 0.9529)).
    canvas.save();
    canvas.clipPath(Path()
      ..addOval(Rect.fromLTWH(20 * scaleX, 20 * scaleY, 360 * scaleX, 120 * scaleY)));
    canvas.drawRect(
      Rect.fromLTWH(0, 0, size.width, size.height),
      Paint()..color = const Color(0xFF2196F3),
    );
    canvas.restore();

    // Tall ellipse clip, filled with green (matches C++ Color::fromRGB(0.2980, 0.6863, 0.3137)).
    canvas.save();
    canvas.clipPath(Path()
      ..addOval(Rect.fromLTWH(30 * scaleX, 180 * scaleY, 120 * scaleX, 200 * scaleY)));
    canvas.drawRect(
      Rect.fromLTWH(0, 0, size.width, size.height),
      Paint()..color = const Color(0xFF4CAF50),
    );
    canvas.restore();

    // Circular clip (equal radii), filled with orange plus a nested circle.
    canvas.save();
    canvas.clipPath(Path()
      ..addOval(Rect.fromLTWH(220 * scaleX, 180 * scaleY, 150 * scaleX, 150 * scaleY)));
    canvas.drawRect(
      Rect.fromLTWH(0, 0, size.width, size.height),
      Paint()..color = const Color(0xFFFF9800),
    );
    canvas.drawCircle(
      Offset(295 * scaleX, 255 * scaleY),
      40 * scaleX,
      Paint()..color = const Color(0xFF800080),
    );
    canvas.restore();
  }

  @override
  bool shouldRepaint(covariant CustomPainter oldDelegate) => false;
}
