import 'dart:io';
import 'dart:ui' as ui;
import 'dart:typed_data';
import 'package:flutter/material.dart';
import 'package:flutter/rendering.dart';
import 'package:flutter_test/flutter_test.dart';

/// Generates visual golden PNG files from Flutter for pixel-perfect comparison.
/// 
/// These tests render widgets offscreen and save PNG files that serve as the
/// "ground truth" for C++ visual fidelity testing.
///
/// Run with: flutter test test/visual_goldens_test.dart
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

    final unevenRRect = RRect.fromRectAndCorners(
      Rect.fromLTWH(200 * scaleX, 20 * scaleY, 150 * scaleX, 100 * scaleY),
      topLeft: Radius.circular(30 * scaleX),
      topRight: Radius.circular(10 * scaleX),
      bottomLeft: Radius.circular(5 * scaleX),
      bottomRight: Radius.circular(25 * scaleX),
    );
    canvas.drawRRect(unevenRRect, Paint()..color = Colors.green);

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
    canvas.rotate(0.2);

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

    final corners = [
      Offset(30 * scaleX, 30 * scaleY),
      Offset(370 * scaleX, 30 * scaleY),
      Offset(30 * scaleX, 370 * scaleY),
      Offset(370 * scaleX, 370 * scaleY),
    ];
    
    for (int i = 0; i < corners.length; i++) {
      canvas.save();
      canvas.translate(corners[i].dx, corners[i].dy);
      canvas.rotate(i * 1.5708);
      
      final paint = Paint()..color = Colors.purple.withAlpha(127);
      final path = Path()
        ..moveTo(0, -15 * scaleX)
        ..lineTo(10 * scaleX, 0)
        ..lineTo(0, 15 * scaleX)
        ..close();
      canvas.drawPath(path, paint);
      
      canvas.restore();
    }
  }

  @override
  bool shouldRepaint(covariant CustomPainter oldDelegate) => false;
}
