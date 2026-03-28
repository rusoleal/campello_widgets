// Standalone Flutter app to generate visual golden PNGs
// Run with: flutter run -d flutter_tester bin/generate_pngs.dart

import 'dart:io';
import 'dart:ui' as ui;
import 'dart:typed_data';
import 'package:flutter/material.dart';
import 'package:flutter/rendering.dart';

void main() async {
  WidgetsFlutterBinding.ensureInitialized();
  
  final goldensDir = Directory('../tests/visual_fidelity/flutter_goldens');
  if (!goldensDir.existsSync()) {
    goldensDir.createSync(recursive: true);
  }

  print('Generating Flutter visual golden files...');
  print('Output: ${goldensDir.absolute.path}');
  print('');

  int generated = 0;

  // Define all test cases
  final testCases = [
    _TestCase(
      name: 'simple_column',
      width: 400,
      height: 600,
      builder: (context) => Column(
        children: [
          Padding(
            padding: const EdgeInsets.all(16.0),
            child: Container(width: 100, height: 100, color: Colors.blue),
          ),
          Expanded(child: Container(color: Colors.green.withAlpha(128))),
        ],
      ),
    ),
    _TestCase(
      name: 'simple_row',
      width: 400,
      height: 100,
      builder: (context) => Row(
        children: [
          Container(width: 80, height: 50, color: Colors.red),
          Expanded(child: Container(color: Colors.green)),
          Container(width: 80, height: 50, color: Colors.blue),
        ],
      ),
    ),
    _TestCase(
      name: 'align_center',
      width: 400,
      height: 400,
      builder: (context) => Center(
        child: Container(width: 100, height: 100, color: Colors.orange),
      ),
    ),
    _TestCase(
      name: 'nested_padding',
      width: 400,
      height: 400,
      builder: (context) => const Padding(
        padding: EdgeInsets.all(24.0),
        child: Padding(
          padding: EdgeInsets.symmetric(horizontal: 16.0, vertical: 8.0),
          child: ColoredBox(
            color: Colors.purple,
            child: SizedBox(width: 100, height: 100),
          ),
        ),
      ),
    ),
    _TestCase(
      name: 'flex_expanded',
      width: 400,
      height: 600,
      builder: (context) => Column(
        children: [
          Container(height: 50, color: Colors.red),
          Expanded(flex: 2, child: Container(color: Colors.green)),
          Expanded(flex: 1, child: Container(color: Colors.blue)),
        ],
      ),
    ),
    _TestCase(
      name: 'stack_positioned',
      width: 400,
      height: 400,
      builder: (context) => Stack(
        children: [
          Container(color: Colors.grey.shade300),
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
            child: Container(width: 100, height: 100, color: Colors.green.withAlpha(178)),
          ),
        ],
      ),
    ),
    _TestCase(
      name: 'canvas_basic_shapes',
      width: 400,
      height: 400,
      builder: (context) => CustomPaint(
        size: const Size(400, 400),
        painter: BasicShapesPainter(),
      ),
    ),
    _TestCase(
      name: 'canvas_rounded_rects',
      width: 400,
      height: 400,
      builder: (context) => CustomPaint(
        size: const Size(400, 400),
        painter: RoundedRectsPainter(),
      ),
    ),
    _TestCase(
      name: 'canvas_lines',
      width: 400,
      height: 400,
      builder: (context) => CustomPaint(
        size: const Size(400, 400),
        painter: LinesPainter(),
      ),
    ),
    _TestCase(
      name: 'canvas_complex_scene',
      width: 400,
      height: 400,
      builder: (context) => CustomPaint(
        size: const Size(400, 400),
        painter: ComplexScenePainter(),
      ),
    ),
  ];

  for (final testCase in testCases) {
    try {
      await generatePng(
        goldensDir: goldensDir,
        testCase: testCase,
      );
      generated++;
    } catch (e, stackTrace) {
      print('ERROR generating ${testCase.name}: $e');
      if (Platform.environment['VERBOSE'] == '1') {
        print(stackTrace);
      }
    }
  }

  print('');
  print('========================================');
  print('Generated: $generated / ${testCases.length} files');
  print('========================================');
  
  exit(0);
}

class _TestCase {
  final String name;
  final double width;
  final double height;
  final WidgetBuilder builder;

  _TestCase({
    required this.name,
    required this.width,
    required this.height,
    required this.builder,
  });
}

Future<void> generatePng({
  required Directory goldensDir,
  required _TestCase testCase,
}) async {
  final outputPath = '${goldensDir.path}/${testCase.name}.png';
  
  // Create a global key for the repaint boundary
  final boundaryKey = GlobalKey();

  // Create the widget
  final widget = MaterialApp(
    debugShowCheckedModeBanner: false,
    home: Scaffold(
      backgroundColor: Colors.white,
      body: RepaintBoundary(
        key: boundaryKey,
        child: SizedBox(
          width: testCase.width,
          height: testCase.height,
          child: Builder(builder: testCase.builder),
        ),
      ),
    ),
  );

  // Run the app in a headless manner
  final binding = WidgetsFlutterBinding.ensureInitialized();
  
  // Attach the widget tree
  runApp(widget);
  
  // Wait for frame to render
  await binding.endOfFrame;
  
  // Find the render object
  final context = boundaryKey.currentContext;
  if (context == null) {
    throw Exception('Could not find context for ${testCase.name}');
  }
  
  final renderObject = context.findRenderObject() as RenderRepaintBoundary;
  
  // Capture the image
  final image = await renderObject.toImage(pixelRatio: 1.0);
  final byteData = await image.toByteData(format: ui.ImageByteFormat.png);
  
  if (byteData == null) {
    image.dispose();
    throw Exception('Failed to encode image');
  }
  
  final bytes = byteData.buffer.asUint8List();
  image.dispose();
  
  // Write to file
  final file = File(outputPath);
  final raf = file.openSync(mode: FileMode.write);
  try {
    raf.writeFromSync(bytes);
    raf.flushSync();
  } finally {
    raf.closeSync();
  }
  
  print('Generated: ${testCase.name}.png (${bytes.length} bytes)');
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
          ..color = ((i + j) % 2 == 0 ? bgColors[0] : bgColors[1]).withAlpha(76);
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
    canvas.drawRRect(cardRRect, Paint()..color = Colors.white.withAlpha(229));

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
        ..color = circleColors[i].withAlpha(153);
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
      
      final paint = Paint()..color = Colors.purple.withAlpha(127);
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
