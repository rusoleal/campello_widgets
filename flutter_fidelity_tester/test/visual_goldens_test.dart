import 'dart:io';
import 'dart:ui' as ui;
import 'dart:typed_data';
import 'package:flutter/material.dart';
import 'package:flutter/rendering.dart';
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

    canvas.save();
    canvas.translate(200 * scaleX, 200 * scaleY);

    final cardRRect = RRect.fromRectAndRadius(
      Rect.fromLTWH(-120 * scaleX, -80 * scaleY, 240 * scaleX, 160 * scaleY),
      Radius.circular(20 * scaleX),
    );
    canvas.drawRRect(cardRRect, Paint()..color = Colors.white.withAlpha(229));

    final borderPaint = Paint()
      ..color = Colors.blue.shade300
      ..style = PaintingStyle.stroke
      ..strokeWidth = 3 * scaleX;
    canvas.drawRRect(cardRRect, borderPaint);

    canvas.save();
    canvas.clipRRect(cardRRect);

    final circleColors = [Colors.red, Colors.green, Colors.orange];
    for (int i = 0; i < 3; i++) {
      final paint = Paint()
        ..color = circleColors[i].withAlpha(153);
      final x = -60.0 * scaleX + i * 60 * scaleX;
      canvas.drawCircle(Offset(x, 0), 35 * scaleX, paint);
    }

    canvas.restore();
    canvas.restore();
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
