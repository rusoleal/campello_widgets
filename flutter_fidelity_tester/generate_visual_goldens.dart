#!/usr/bin/env dart
import 'dart:io';
import 'dart:ui' as ui;
import 'dart:typed_data';
import 'package:flutter/material.dart';
import 'package:flutter/rendering.dart';
import 'package:flutter_test/flutter_test.dart';

/// Standalone script to generate visual golden PNG files.
/// Run with: flutter run -d flutter_tester generate_visual_goldens.dart

void main() async {
  final goldensDir = Directory('../tests/visual_fidelity/flutter_goldens');
  
  if (!goldensDir.existsSync()) {
    goldensDir.createSync(recursive: true);
  }

  print('Generating Flutter visual golden files...');
  print('Output directory: ${goldensDir.absolute.path}');
  print('');

  // TestWidgetsFlutterBinding is required for widget tests
  final binding = AutomatedTestWidgetsFlutterBinding.ensureInitialized();
  binding.defaultTestTimeout = Timeout.none;
  
  int generated = 0;
  int failed = 0;

  // Test 1: simple_column
  try {
    await binding.runTest(() async {
      final tester = WidgetTester(binding);
      await tester.binding.setSurfaceSize(const Size(400, 600));
      
      await tester.pumpWidget(
        MaterialApp(
          debugShowCheckedModeBanner: false,
          home: Scaffold(
            backgroundColor: Colors.white,
            body: SizedBox(
              width: 400,
              height: 600,
              child: Column(
                children: [
                  Padding(
                    padding: const EdgeInsets.all(16.0),
                    child: Container(
                      width: 100,
                      height: 100,
                      color: Colors.blue,
                    ),
                  ),
                  Expanded(
                    child: Container(
                      color: Colors.green.withOpacity(0.5),
                    ),
                  ),
                ],
              ),
            ),
          ),
        ),
        const Duration(milliseconds: 100),
      );

      await saveScreenshot(
        tester: tester,
        outputPath: '${goldensDir.path}/simple_column.png',
      );
      generated++;
    }, {});
  } catch (e) {
    print('Error generating simple_column: $e');
    failed++;
  }

  // Test 2: simple_row
  try {
    await binding.runTest(() async {
      final tester = WidgetTester(binding);
      await tester.binding.setSurfaceSize(const Size(400, 100));
      
      await tester.pumpWidget(
        MaterialApp(
          debugShowCheckedModeBanner: false,
          home: Scaffold(
            backgroundColor: Colors.white,
            body: SizedBox(
              width: 400,
              height: 100,
              child: Row(
                children: [
                  Container(width: 80, height: 50, color: Colors.red),
                  Expanded(child: Container(color: Colors.green)),
                  Container(width: 80, height: 50, color: Colors.blue),
                ],
              ),
            ),
          ),
        ),
        const Duration(milliseconds: 100),
      );

      await saveScreenshot(
        tester: tester,
        outputPath: '${goldensDir.path}/simple_row.png',
      );
      generated++;
    }, {});
  } catch (e) {
    print('Error generating simple_row: $e');
    failed++;
  }

  // Test 3: align_center
  try {
    await binding.runTest(() async {
      final tester = WidgetTester(binding);
      await tester.binding.setSurfaceSize(const Size(400, 400));
      
      await tester.pumpWidget(
        MaterialApp(
          debugShowCheckedModeBanner: false,
          home: Scaffold(
            backgroundColor: Colors.white,
            body: SizedBox(
              width: 400,
              height: 400,
              child: Center(
                child: Container(width: 100, height: 100, color: Colors.orange),
              ),
            ),
          ),
        ),
        const Duration(milliseconds: 100),
      );

      await saveScreenshot(
        tester: tester,
        outputPath: '${goldensDir.path}/align_center.png',
      );
      generated++;
    }, {});
  } catch (e) {
    print('Error generating align_center: $e');
    failed++;
  }

  // Test 4: nested_padding
  try {
    await binding.runTest(() async {
      final tester = WidgetTester(binding);
      await tester.binding.setSurfaceSize(const Size(400, 400));
      
      await tester.pumpWidget(
        MaterialApp(
          debugShowCheckedModeBanner: false,
          home: Scaffold(
            backgroundColor: Colors.white,
            body: SizedBox(
              width: 400,
              height: 400,
              child: Padding(
                padding: const EdgeInsets.all(24.0),
                child: Padding(
                  padding: const EdgeInsets.symmetric(horizontal: 16.0, vertical: 8.0),
                  child: Container(width: 100, height: 100, color: Colors.purple),
                ),
              ),
            ),
          ),
        ),
        const Duration(milliseconds: 100),
      );

      await saveScreenshot(
        tester: tester,
        outputPath: '${goldensDir.path}/nested_padding.png',
      );
      generated++;
    }, {});
  } catch (e) {
    print('Error generating nested_padding: $e');
    failed++;
  }

  // Test 5: flex_expanded
  try {
    await binding.runTest(() async {
      final tester = WidgetTester(binding);
      await tester.binding.setSurfaceSize(const Size(400, 600));
      
      await tester.pumpWidget(
        MaterialApp(
          debugShowCheckedModeBanner: false,
          home: Scaffold(
            backgroundColor: Colors.white,
            body: SizedBox(
              width: 400,
              height: 600,
              child: Column(
                children: [
                  Container(height: 50, color: Colors.red),
                  Expanded(flex: 2, child: Container(color: Colors.green)),
                  Expanded(flex: 1, child: Container(color: Colors.blue)),
                ],
              ),
            ),
          ),
        ),
        const Duration(milliseconds: 100),
      );

      await saveScreenshot(
        tester: tester,
        outputPath: '${goldensDir.path}/flex_expanded.png',
      );
      generated++;
    }, {});
  } catch (e) {
    print('Error generating flex_expanded: $e');
    failed++;
  }

  // Test 6: stack_positioned
  try {
    await binding.runTest(() async {
      final tester = WidgetTester(binding);
      await tester.binding.setSurfaceSize(const Size(400, 400));
      
      await tester.pumpWidget(
        MaterialApp(
          debugShowCheckedModeBanner: false,
          home: Scaffold(
            backgroundColor: Colors.white,
            body: SizedBox(
              width: 400,
              height: 400,
              child: Stack(
                children: [
                  Container(width: 400, height: 400, color: Colors.grey.shade300),
                  Positioned(
                    left: 50,
                    top: 50,
                    child: Container(width: 100, height: 100, color: Colors.red),
                  ),
                  Positioned(
                    right: 50,
                    bottom: 50,
                    child: Container(width: 100, height: 100, color: Colors.blue),
                  ),
                  Positioned(
                    left: 150,
                    top: 150,
                    child: Container(
                      width: 100,
                      height: 100,
                      color: Colors.green.withOpacity(0.7),
                    ),
                  ),
                ],
              ),
            ),
          ),
        ),
        const Duration(milliseconds: 100),
      );

      await saveScreenshot(
        tester: tester,
        outputPath: '${goldensDir.path}/stack_positioned.png',
      );
      generated++;
    }, {});
  } catch (e) {
    print('Error generating stack_positioned: $e');
    failed++;
  }

  // Canvas tests with CustomPaint
  
  // Test 7: canvas_basic_shapes
  try {
    await binding.runTest(() async {
      final tester = WidgetTester(binding);
      await tester.binding.setSurfaceSize(const Size(400, 400));
      
      await tester.pumpWidget(
        MaterialApp(
          debugShowCheckedModeBanner: false,
          home: Scaffold(
            backgroundColor: Colors.white,
            body: CustomPaint(
              size: const Size(400, 400),
              painter: BasicShapesPainter(),
            ),
          ),
        ),
        const Duration(milliseconds: 100),
      );

      await saveScreenshot(
        tester: tester,
        outputPath: '${goldensDir.path}/canvas_basic_shapes.png',
      );
      generated++;
    }, {});
  } catch (e) {
    print('Error generating canvas_basic_shapes: $e');
    failed++;
  }

  // Test 8: canvas_rounded_rects
  try {
    await binding.runTest(() async {
      final tester = WidgetTester(binding);
      await tester.binding.setSurfaceSize(const Size(400, 400));
      
      await tester.pumpWidget(
        MaterialApp(
          debugShowCheckedModeBanner: false,
          home: Scaffold(
            backgroundColor: Colors.white,
            body: CustomPaint(
              size: const Size(400, 400),
              painter: RoundedRectsPainter(),
            ),
          ),
        ),
        const Duration(milliseconds: 100),
      );

      await saveScreenshot(
        tester: tester,
        outputPath: '${goldensDir.path}/canvas_rounded_rects.png',
      );
      generated++;
    }, {});
  } catch (e) {
    print('Error generating canvas_rounded_rects: $e');
    failed++;
  }

  // Test 9: canvas_lines
  try {
    await binding.runTest(() async {
      final tester = WidgetTester(binding);
      await tester.binding.setSurfaceSize(const Size(400, 400));
      
      await tester.pumpWidget(
        MaterialApp(
          debugShowCheckedModeBanner: false,
          home: Scaffold(
            backgroundColor: Colors.white,
            body: CustomPaint(
              size: const Size(400, 400),
              painter: LinesPainter(),
            ),
          ),
        ),
        const Duration(milliseconds: 100),
      );

      await saveScreenshot(
        tester: tester,
        outputPath: '${goldensDir.path}/canvas_lines.png',
      );
      generated++;
    }, {});
  } catch (e) {
    print('Error generating canvas_lines: $e');
    failed++;
  }

  // Test 10: canvas_complex_scene
  try {
    await binding.runTest(() async {
      final tester = WidgetTester(binding);
      await tester.binding.setSurfaceSize(const Size(400, 400));
      
      await tester.pumpWidget(
        MaterialApp(
          debugShowCheckedModeBanner: false,
          home: Scaffold(
            backgroundColor: Colors.white,
            body: CustomPaint(
              size: const Size(400, 400),
              painter: ComplexScenePainter(),
            ),
          ),
        ),
        const Duration(milliseconds: 100),
      );

      await saveScreenshot(
        tester: tester,
        outputPath: '${goldensDir.path}/canvas_complex_scene.png',
      );
      generated++;
    }, {});
  } catch (e) {
    print('Error generating canvas_complex_scene: $e');
    failed++;
  }

  print('');
  print('========================================');
  print('Generated: $generated files');
  print('Failed: $failed files');
  print('========================================');
  
  // Exit explicitly
  exit(failed > 0 ? 1 : 0);
}

Future<void> saveScreenshot({
  required WidgetTester tester,
  required String outputPath,
}) async {
  final boundary = tester.renderObject<RenderRepaintBoundary>(
    find.byType(RepaintBoundary).first,
  );

  final ui.Image image = await boundary.toImage(pixelRatio: 1.0);
  final ByteData? byteData = await image.toByteData(format: ui.ImageByteFormat.png);

  if (byteData == null) {
    throw Exception('Failed to encode image as PNG');
  }

  final file = File(outputPath);
  final bytes = byteData.buffer.asUint8List();
  
  final raf = file.openSync(mode: FileMode.write);
  try {
    raf.writeFromSync(bytes);
    raf.flushSync();
  } finally {
    raf.closeSync();
  }
  
  image.dispose();
  
  print('Generated: $outputPath (${bytes.length} bytes)');
}

// Custom Painters

class BasicShapesPainter extends CustomPainter {
  @override
  void paint(Canvas canvas, Size size) {
    canvas.drawRect(const Rect.fromLTWH(20, 20, 100, 80), Paint()..color = Colors.blue);
    canvas.drawCircle(const Offset(200, 60), 40, Paint()..color = Colors.red);
    canvas.drawOval(const Rect.fromLTWH(280, 20, 100, 80), Paint()..color = Colors.green);
    
    final purpleStroke = Paint()
      ..color = Colors.purple
      ..style = PaintingStyle.stroke
      ..strokeWidth = 4;
    canvas.drawRect(const Rect.fromLTWH(20, 120, 100, 80), purpleStroke);
  }

  @override
  bool shouldRepaint(covariant CustomPainter oldDelegate) => false;
}

class RoundedRectsPainter extends CustomPainter {
  @override
  void paint(Canvas canvas, Size size) {
    final rrect = RRect.fromRectAndRadius(
      const Rect.fromLTWH(20, 20, 150, 100),
      const Radius.circular(20),
    );
    canvas.drawRRect(rrect, Paint()..color = Colors.blue);

    final unevenRRect = RRect.fromRectAndCorners(
      const Rect.fromLTWH(200, 20, 150, 100),
      topLeft: const Radius.circular(30),
      topRight: const Radius.circular(10),
      bottomLeft: const Radius.circular(5),
      bottomRight: const Radius.circular(25),
    );
    canvas.drawRRect(unevenRRect, Paint()..color = Colors.green);

    final outerRRect = RRect.fromRectAndRadius(
      const Rect.fromLTWH(20, 150, 150, 100),
      const Radius.circular(25),
    );
    final innerRRect = RRect.fromRectAndRadius(
      const Rect.fromLTWH(35, 165, 120, 70),
      const Radius.circular(15),
    );
    canvas.drawDRRect(outerRRect, innerRRect, Paint()..color = Colors.purple);

    final strokeRRect = RRect.fromRectAndRadius(
      const Rect.fromLTWH(200, 150, 150, 100),
      const Radius.circular(15),
    );
    final redStroke = Paint()
      ..color = Colors.red
      ..style = PaintingStyle.stroke
      ..strokeWidth = 5;
    canvas.drawRRect(strokeRRect, redStroke);
  }

  @override
  bool shouldRepaint(covariant CustomPainter oldDelegate) => false;
}

class LinesPainter extends CustomPainter {
  @override
  void paint(Canvas canvas, Size size) {
    final linePaint = Paint()
      ..color = Colors.black
      ..strokeWidth = 2;
    canvas.drawLine(const Offset(20, 20), const Offset(150, 100), linePaint);
    canvas.drawLine(const Offset(200, 20), const Offset(350, 150), linePaint);

    final thickPaint = Paint()
      ..color = Colors.orange
      ..strokeWidth = 8;
    canvas.drawLine(const Offset(50, 200), const Offset(350, 250), thickPaint);

    for (int i = 0; i < 4; i++) {
      final width = (i + 1) * 3.0;
      final paint = Paint()
        ..color = Colors.green
        ..strokeWidth = width;
      final y = 300.0 + i * 25;
      canvas.drawLine(Offset(20, y), Offset(150, y), paint);
    }
  }

  @override
  bool shouldRepaint(covariant CustomPainter oldDelegate) => false;
}

class ComplexScenePainter extends CustomPainter {
  @override
  void paint(Canvas canvas, Size size) {
    final bgColors = [Colors.blue.shade100, Colors.purple.shade100];
    for (int i = 0; i < 5; i++) {
      for (int j = 0; j < 5; j++) {
        final paint = Paint()
          ..color = ((i + j) % 2 == 0 ? bgColors[0] : bgColors[1]).withOpacity(0.3);
        final cx = 40.0 + i * 80;
        final cy = 40.0 + j * 80;
        canvas.drawCircle(Offset(cx, cy), 50, paint);
      }
    }

    canvas.save();
    canvas.translate(200, 200);
    canvas.rotate(0.2);

    final cardRRect = RRect.fromRectAndRadius(
      const Rect.fromLTWH(-120, -80, 240, 160),
      const Radius.circular(20),
    );
    canvas.drawRRect(cardRRect, Paint()..color = Colors.white.withOpacity(0.9));

    final borderPaint = Paint()
      ..color = Colors.blue.shade300
      ..style = PaintingStyle.stroke
      ..strokeWidth = 3;
    canvas.drawRRect(cardRRect, borderPaint);

    canvas.save();
    canvas.clipRRect(cardRRect);

    final circleColors = [Colors.red, Colors.green, Colors.orange];
    for (int i = 0; i < 3; i++) {
      final paint = Paint()
        ..color = circleColors[i].withOpacity(0.6);
      final x = -60.0 + i * 60;
      canvas.drawCircle(Offset(x, 0), 35, paint);
    }

    canvas.restore();
    canvas.restore();

    final corners = [
      const Offset(30, 30),
      const Offset(370, 30),
      const Offset(30, 370),
      const Offset(370, 370),
    ];
    
    for (int i = 0; i < corners.length; i++) {
      canvas.save();
      canvas.translate(corners[i].dx, corners[i].dy);
      canvas.rotate(i * 1.5708);
      
      final paint = Paint()..color = Colors.purple.withOpacity(0.5);
      final path = Path()
        ..moveTo(0, -15)
        ..lineTo(10, 0)
        ..lineTo(0, 15)
        ..close();
      canvas.drawPath(path, paint);
      
      canvas.restore();
    }
  }

  @override
  bool shouldRepaint(covariant CustomPainter oldDelegate) => false;
}
