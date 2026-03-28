import 'dart:io';
import 'dart:math';
import 'package:flutter/material.dart';
import 'package:flutter/rendering.dart';
import 'package:flutter_test/flutter_test.dart';
import '../lib/render_tree_capture.dart';

/// Generates golden files for campello_widgets fidelity testing.
/// Run with: flutter test test/fidelity_goldens_test.dart
void main() {
  final goldensDir = Directory('../tests/goldens');
  
  setUpAll(() {
    if (!goldensDir.existsSync()) {
      goldensDir.createSync(recursive: true);
    }
  });

  group('Simple Widget Goldens', () {
    testWidgets('simple_column', (WidgetTester tester) async {
      // Build a simple column with padding and colored box
      final widget = Column(
        children: [
          Padding(
            padding: EdgeInsets.all(8.0),
            child: Container(
              width: 100,
              height: 100,
              color: Colors.blue,
            ),
          ),
          Expanded(
            child: Container(),
          ),
        ],
      );

      await tester.pumpWidget(
        MaterialApp(
          home: Scaffold(
            body: SizedBox(
              width: 400,
              height: 600,
              child: widget,
            ),
          ),
        ),
      );

      await tester.pumpAndSettle();

      // Find the Column's render object
      final columnFinder = find.byType(Column);
      final columnElement = tester.element(columnFinder);
      final columnRenderObject = columnElement.renderObject as RenderFlex;

      // Capture the render tree
      final snapshot = RenderTreeCapture.captureRenderTree(
        columnRenderObject,
        parentConstraints: BoxConstraints.tight(Size(400, 600)),
      );

      final fidelitySnapshot = FidelitySnapshot(
        viewportWidth: 400,
        viewportHeight: 600,
        layout: snapshot,
        description: 'Simple Column with Padding and ColoredBox - Flutter reference',
      );

      // Write golden file
      final goldenFile = File('${goldensDir.path}/simple_column_flutter.json');
      goldenFile.writeAsStringSync(fidelitySnapshot.toJsonString());
      
      print('Generated: ${goldenFile.path}');
    });

    testWidgets('simple_row', (WidgetTester tester) async {
      final widget = Row(
        children: [
          Container(
            width: 80,
            height: 50,
            color: Colors.red,
          ),
          Expanded(
            child: Container(
              color: Colors.green,
            ),
          ),
          Container(
            width: 80,
            height: 50,
            color: Colors.blue,
          ),
        ],
      );

      await tester.pumpWidget(
        MaterialApp(
          home: Scaffold(
            body: SizedBox(
              width: 400,
              height: 100,
              child: widget,
            ),
          ),
        ),
      );

      await tester.pumpAndSettle();

      final rowFinder = find.byType(Row);
      final rowElement = tester.element(rowFinder);
      final rowRenderObject = rowElement.renderObject as RenderFlex;

      final snapshot = RenderTreeCapture.captureRenderTree(
        rowRenderObject,
        parentConstraints: BoxConstraints.tight(Size(400, 100)),
      );

      final fidelitySnapshot = FidelitySnapshot(
        viewportWidth: 400,
        viewportHeight: 100,
        layout: snapshot,
        description: 'Simple Row with Expanded - Flutter reference',
      );

      final goldenFile = File('${goldensDir.path}/simple_row_flutter.json');
      goldenFile.writeAsStringSync(fidelitySnapshot.toJsonString());
      
      print('Generated: ${goldenFile.path}');
    });

    testWidgets('nested_padding', (WidgetTester tester) async {
      final widget = Padding(
        padding: EdgeInsets.all(16.0),
        child: Padding(
          padding: EdgeInsets.symmetric(horizontal: 8.0, vertical: 4.0),
          child: Container(
            width: 100,
            height: 100,
            color: Colors.purple,
          ),
        ),
      );

      await tester.pumpWidget(
        MaterialApp(
          home: Scaffold(
            body: SizedBox(
              width: 400,
              height: 400,
              child: widget,
            ),
          ),
        ),
      );

      await tester.pumpAndSettle();

      final paddingFinder = find.byWidgetPredicate(
        (widget) => widget is Padding && widget.padding == EdgeInsets.all(16.0),
      );
      final paddingElement = tester.element(paddingFinder);
      final paddingRenderObject = paddingElement.renderObject as RenderPadding;

      final snapshot = RenderTreeCapture.captureRenderTree(
        paddingRenderObject,
        parentConstraints: BoxConstraints.tight(Size(400, 400)),
      );

      final fidelitySnapshot = FidelitySnapshot(
        viewportWidth: 400,
        viewportHeight: 400,
        layout: snapshot,
        description: 'Nested Padding widgets - Flutter reference',
      );

      final goldenFile = File('${goldensDir.path}/nested_padding_flutter.json');
      goldenFile.writeAsStringSync(fidelitySnapshot.toJsonString());
      
      print('Generated: ${goldenFile.path}');
    });

    testWidgets('align_center', (WidgetTester tester) async {
      final widget = Center(
        child: Container(
          width: 100,
          height: 100,
          color: Colors.orange,
        ),
      );

      await tester.pumpWidget(
        MaterialApp(
          home: Scaffold(
            body: SizedBox(
              width: 400,
              height: 400,
              child: widget,
            ),
          ),
        ),
      );

      await tester.pumpAndSettle();

      final centerFinder = find.byType(Center);
      final centerElement = tester.element(centerFinder);
      final centerRenderObject = centerElement.renderObject as RenderPositionedBox;

      final snapshot = RenderTreeCapture.captureRenderTree(
        centerRenderObject,
        parentConstraints: BoxConstraints.tight(Size(400, 400)),
      );

      final fidelitySnapshot = FidelitySnapshot(
        viewportWidth: 400,
        viewportHeight: 400,
        layout: snapshot,
        description: 'Center Align widget - Flutter reference',
      );

      final goldenFile = File('${goldensDir.path}/align_center_flutter.json');
      goldenFile.writeAsStringSync(fidelitySnapshot.toJsonString());
      
      print('Generated: ${goldenFile.path}');
    });

    testWidgets('sized_box_constraints', (WidgetTester tester) async {
      final widget = SizedBox(
        width: 200,
        height: 150,
        child: Container(
          color: Colors.teal,
        ),
      );

      await tester.pumpWidget(
        MaterialApp(
          home: Scaffold(
            body: SizedBox(
              width: 400,
              height: 400,
              child: widget,
            ),
          ),
        ),
      );

      await tester.pumpAndSettle();

      final sizedBoxFinder = find.byType(SizedBox);
      // Get the first SizedBox (our wrapper)
      final sizedBoxElement = tester.element(sizedBoxFinder.first);
      final sizedBoxRenderObject = sizedBoxElement.renderObject as RenderConstrainedBox;

      final snapshot = RenderTreeCapture.captureRenderTree(
        sizedBoxRenderObject,
        parentConstraints: BoxConstraints.tight(Size(400, 400)),
      );

      final fidelitySnapshot = FidelitySnapshot(
        viewportWidth: 400,
        viewportHeight: 400,
        layout: snapshot,
        description: 'SizedBox with explicit dimensions - Flutter reference',
      );

      final goldenFile = File('${goldensDir.path}/sized_box_flutter.json');
      goldenFile.writeAsStringSync(fidelitySnapshot.toJsonString());
      
      print('Generated: ${goldenFile.path}');
    });

    testWidgets('flex_expanded_multiple', (WidgetTester tester) async {
      final widget = Column(
        children: [
          Container(
            height: 50,
            color: Colors.red,
          ),
          Expanded(
            flex: 2,
            child: Container(
              color: Colors.green,
            ),
          ),
          Expanded(
            flex: 1,
            child: Container(
              color: Colors.blue,
            ),
          ),
        ],
      );

      await tester.pumpWidget(
        MaterialApp(
          home: Scaffold(
            body: SizedBox(
              width: 400,
              height: 600,
              child: widget,
            ),
          ),
        ),
      );

      await tester.pumpAndSettle();

      final columnFinder = find.byType(Column);
      final columnElement = tester.element(columnFinder);
      final columnRenderObject = columnElement.renderObject as RenderFlex;

      final snapshot = RenderTreeCapture.captureRenderTree(
        columnRenderObject,
        parentConstraints: BoxConstraints.tight(Size(400, 600)),
      );

      final fidelitySnapshot = FidelitySnapshot(
        viewportWidth: 400,
        viewportHeight: 600,
        layout: snapshot,
        description: 'Column with multiple Expanded children - Flutter reference',
      );

      final goldenFile = File('${goldensDir.path}/flex_expanded_flutter.json');
      goldenFile.writeAsStringSync(fidelitySnapshot.toJsonString());
      
      print('Generated: ${goldenFile.path}');
    });

    testWidgets('stack_positioned', (WidgetTester tester) async {
      final widget = Stack(
        children: [
          Container(
            width: 400,
            height: 400,
            color: Colors.grey,
          ),
          Positioned(
            left: 50,
            top: 50,
            child: Container(
              width: 100,
              height: 100,
              color: Colors.red,
            ),
          ),
          Positioned(
            right: 50,
            bottom: 50,
            child: Container(
              width: 100,
              height: 100,
              color: Colors.blue,
            ),
          ),
        ],
      );

      await tester.pumpWidget(
        MaterialApp(
          home: Scaffold(
            body: SizedBox(
              width: 400,
              height: 400,
              child: widget,
            ),
          ),
        ),
      );

      await tester.pumpAndSettle();

      // Find the Stack by looking for the specific Stack that contains grey container
      final stackFinder = find.byWidgetPredicate(
        (widget) => widget is Stack && widget.children.length == 3,
      );
      final stackElement = tester.element(stackFinder);
      final stackRenderObject = stackElement.renderObject as RenderStack;

      final snapshot = RenderTreeCapture.captureRenderTree(
        stackRenderObject,
        parentConstraints: BoxConstraints.tight(Size(400, 400)),
      );

      final fidelitySnapshot = FidelitySnapshot(
        viewportWidth: 400,
        viewportHeight: 400,
        layout: snapshot,
        description: 'Stack with Positioned children - Flutter reference',
      );

      final goldenFile = File('${goldensDir.path}/stack_positioned_flutter.json');
      goldenFile.writeAsStringSync(fidelitySnapshot.toJsonString());
      
      print('Generated: ${goldenFile.path}');
    });
  });

  group('CustomPaint / Canvas API Goldens', () {
    testWidgets('canvas_basic_shapes', (WidgetTester tester) async {
      // Tests: drawRect, drawCircle, drawOval
      final painter = _BasicShapesPainter();
      
      await tester.pumpWidget(
        MaterialApp(
          home: Scaffold(
            body: CustomPaint(
              size: Size(400, 400),
              painter: painter,
            ),
          ),
        ),
      );

      await tester.pumpAndSettle();

      final customPaintFinder = find.byType(CustomPaint);
      final customPaintElement = tester.firstElement(customPaintFinder);
      final renderObject = customPaintElement.renderObject as RenderCustomPaint;

      final snapshot = RenderTreeCapture.captureRenderTree(
        renderObject,
        parentConstraints: BoxConstraints.tight(Size(400, 400)),
      );

      final fidelitySnapshot = FidelitySnapshot(
        viewportWidth: 400,
        viewportHeight: 400,
        layout: snapshot,
        paintCommands: painter.capturedCommands,
        description: 'Canvas basic shapes: rect, circle, oval - Flutter reference',
      );

      final goldenFile = File('${goldensDir.path}/canvas_basic_shapes_flutter.json');
      goldenFile.writeAsStringSync(fidelitySnapshot.toJsonString());
      print('Generated: ${goldenFile.path}');
    });

    testWidgets('canvas_lines_and_points', (WidgetTester tester) async {
      // Tests: drawLine, drawPoints
      final painter = _LinesAndPointsPainter();
      
      await tester.pumpWidget(
        MaterialApp(
          home: Scaffold(
            body: CustomPaint(
              size: Size(400, 400),
              painter: painter,
            ),
          ),
        ),
      );

      await tester.pumpAndSettle();

      final customPaintFinder = find.byType(CustomPaint);
      final customPaintElement = tester.firstElement(customPaintFinder);
      final renderObject = customPaintElement.renderObject as RenderCustomPaint;

      final snapshot = RenderTreeCapture.captureRenderTree(
        renderObject,
        parentConstraints: BoxConstraints.tight(Size(400, 400)),
      );

      final fidelitySnapshot = FidelitySnapshot(
        viewportWidth: 400,
        viewportHeight: 400,
        layout: snapshot,
        paintCommands: painter.capturedCommands,
        description: 'Canvas lines and points - Flutter reference',
      );

      final goldenFile = File('${goldensDir.path}/canvas_lines_points_flutter.json');
      goldenFile.writeAsStringSync(fidelitySnapshot.toJsonString());
      print('Generated: ${goldenFile.path}');
    });

    testWidgets('canvas_paths', (WidgetTester tester) async {
      // Tests: Path with moveTo, lineTo, cubicTo, quadraticTo, close
      final painter = _PathsPainter();
      
      await tester.pumpWidget(
        MaterialApp(
          home: Scaffold(
            body: CustomPaint(
              size: Size(400, 400),
              painter: painter,
            ),
          ),
        ),
      );

      await tester.pumpAndSettle();

      final customPaintFinder = find.byType(CustomPaint);
      final customPaintElement = tester.firstElement(customPaintFinder);
      final renderObject = customPaintElement.renderObject as RenderCustomPaint;

      final snapshot = RenderTreeCapture.captureRenderTree(
        renderObject,
        parentConstraints: BoxConstraints.tight(Size(400, 400)),
      );

      final fidelitySnapshot = FidelitySnapshot(
        viewportWidth: 400,
        viewportHeight: 400,
        layout: snapshot,
        paintCommands: painter.capturedCommands,
        description: 'Canvas paths with curves - Flutter reference',
      );

      final goldenFile = File('${goldensDir.path}/canvas_paths_flutter.json');
      goldenFile.writeAsStringSync(fidelitySnapshot.toJsonString());
      print('Generated: ${goldenFile.path}');
    });

    testWidgets('canvas_rounded_rects', (WidgetTester tester) async {
      // Tests: drawRRect, drawDRRect
      final painter = _RoundedRectsPainter();
      
      await tester.pumpWidget(
        MaterialApp(
          home: Scaffold(
            body: CustomPaint(
              size: Size(400, 400),
              painter: painter,
            ),
          ),
        ),
      );

      await tester.pumpAndSettle();

      final customPaintFinder = find.byType(CustomPaint);
      final customPaintElement = tester.firstElement(customPaintFinder);
      final renderObject = customPaintElement.renderObject as RenderCustomPaint;

      final snapshot = RenderTreeCapture.captureRenderTree(
        renderObject,
        parentConstraints: BoxConstraints.tight(Size(400, 400)),
      );

      final fidelitySnapshot = FidelitySnapshot(
        viewportWidth: 400,
        viewportHeight: 400,
        layout: snapshot,
        paintCommands: painter.capturedCommands,
        description: 'Canvas rounded rectangles - Flutter reference',
      );

      final goldenFile = File('${goldensDir.path}/canvas_rounded_rects_flutter.json');
      goldenFile.writeAsStringSync(fidelitySnapshot.toJsonString());
      print('Generated: ${goldenFile.path}');
    });

    testWidgets('canvas_arcs', (WidgetTester tester) async {
      // Tests: drawArc
      final painter = _ArcsPainter();
      
      await tester.pumpWidget(
        MaterialApp(
          home: Scaffold(
            body: CustomPaint(
              size: Size(400, 400),
              painter: painter,
            ),
          ),
        ),
      );

      await tester.pumpAndSettle();

      final customPaintFinder = find.byType(CustomPaint);
      final customPaintElement = tester.firstElement(customPaintFinder);
      final renderObject = customPaintElement.renderObject as RenderCustomPaint;

      final snapshot = RenderTreeCapture.captureRenderTree(
        renderObject,
        parentConstraints: BoxConstraints.tight(Size(400, 400)),
      );

      final fidelitySnapshot = FidelitySnapshot(
        viewportWidth: 400,
        viewportHeight: 400,
        layout: snapshot,
        paintCommands: painter.capturedCommands,
        description: 'Canvas arcs - Flutter reference',
      );

      final goldenFile = File('${goldensDir.path}/canvas_arcs_flutter.json');
      goldenFile.writeAsStringSync(fidelitySnapshot.toJsonString());
      print('Generated: ${goldenFile.path}');
    });

    testWidgets('canvas_transforms', (WidgetTester tester) async {
      // Tests: translate, rotate, scale, save, restore
      final painter = _TransformsPainter();
      
      await tester.pumpWidget(
        MaterialApp(
          home: Scaffold(
            body: CustomPaint(
              size: Size(400, 400),
              painter: painter,
            ),
          ),
        ),
      );

      await tester.pumpAndSettle();

      final customPaintFinder = find.byType(CustomPaint);
      final customPaintElement = tester.firstElement(customPaintFinder);
      final renderObject = customPaintElement.renderObject as RenderCustomPaint;

      final snapshot = RenderTreeCapture.captureRenderTree(
        renderObject,
        parentConstraints: BoxConstraints.tight(Size(400, 400)),
      );

      final fidelitySnapshot = FidelitySnapshot(
        viewportWidth: 400,
        viewportHeight: 400,
        layout: snapshot,
        paintCommands: painter.capturedCommands,
        description: 'Canvas transforms (translate, rotate, scale) - Flutter reference',
      );

      final goldenFile = File('${goldensDir.path}/canvas_transforms_flutter.json');
      goldenFile.writeAsStringSync(fidelitySnapshot.toJsonString());
      print('Generated: ${goldenFile.path}');
    });

    testWidgets('canvas_clipping', (WidgetTester tester) async {
      // Tests: clipRect, clipPath, clipRRect
      final painter = _ClippingPainter();
      
      await tester.pumpWidget(
        MaterialApp(
          home: Scaffold(
            body: CustomPaint(
              size: Size(400, 400),
              painter: painter,
            ),
          ),
        ),
      );

      await tester.pumpAndSettle();

      final customPaintFinder = find.byType(CustomPaint);
      final customPaintElement = tester.firstElement(customPaintFinder);
      final renderObject = customPaintElement.renderObject as RenderCustomPaint;

      final snapshot = RenderTreeCapture.captureRenderTree(
        renderObject,
        parentConstraints: BoxConstraints.tight(Size(400, 400)),
      );

      final fidelitySnapshot = FidelitySnapshot(
        viewportWidth: 400,
        viewportHeight: 400,
        layout: snapshot,
        paintCommands: painter.capturedCommands,
        description: 'Canvas clipping operations - Flutter reference',
      );

      final goldenFile = File('${goldensDir.path}/canvas_clipping_flutter.json');
      goldenFile.writeAsStringSync(fidelitySnapshot.toJsonString());
      print('Generated: ${goldenFile.path}');
    });

    testWidgets('canvas_paint_styles', (WidgetTester tester) async {
      // Tests: Paint styles (fill, stroke), strokeWidth, blend modes
      final painter = _PaintStylesPainter();
      
      await tester.pumpWidget(
        MaterialApp(
          home: Scaffold(
            body: CustomPaint(
              size: Size(400, 400),
              painter: painter,
            ),
          ),
        ),
      );

      await tester.pumpAndSettle();

      final customPaintFinder = find.byType(CustomPaint);
      final customPaintElement = tester.firstElement(customPaintFinder);
      final renderObject = customPaintElement.renderObject as RenderCustomPaint;

      final snapshot = RenderTreeCapture.captureRenderTree(
        renderObject,
        parentConstraints: BoxConstraints.tight(Size(400, 400)),
      );

      final fidelitySnapshot = FidelitySnapshot(
        viewportWidth: 400,
        viewportHeight: 400,
        layout: snapshot,
        paintCommands: painter.capturedCommands,
        description: 'Canvas paint styles (fill, stroke, blend modes) - Flutter reference',
      );

      final goldenFile = File('${goldensDir.path}/canvas_paint_styles_flutter.json');
      goldenFile.writeAsStringSync(fidelitySnapshot.toJsonString());
      print('Generated: ${goldenFile.path}');
    });

    testWidgets('canvas_complex_scene', (WidgetTester tester) async {
      // Tests: Complex scene combining multiple canvas operations
      final painter = _ComplexScenePainter();
      
      await tester.pumpWidget(
        MaterialApp(
          home: Scaffold(
            body: CustomPaint(
              size: Size(400, 400),
              painter: painter,
            ),
          ),
        ),
      );

      await tester.pumpAndSettle();

      final customPaintFinder = find.byType(CustomPaint);
      final customPaintElement = tester.firstElement(customPaintFinder);
      final renderObject = customPaintElement.renderObject as RenderCustomPaint;

      final snapshot = RenderTreeCapture.captureRenderTree(
        renderObject,
        parentConstraints: BoxConstraints.tight(Size(400, 400)),
      );

      final fidelitySnapshot = FidelitySnapshot(
        viewportWidth: 400,
        viewportHeight: 400,
        layout: snapshot,
        paintCommands: painter.capturedCommands,
        description: 'Complex canvas scene with multiple operations - Flutter reference',
      );

      final goldenFile = File('${goldensDir.path}/canvas_complex_scene_flutter.json');
      goldenFile.writeAsStringSync(fidelitySnapshot.toJsonString());
      print('Generated: ${goldenFile.path}');
    });
  });
}

// ============================================================================
// Custom Painters for Canvas API Tests
// ============================================================================

abstract class _CapturePainter extends CustomPainter {
  List<DrawCommandSnapshot> capturedCommands = [];

  @override
  void paint(Canvas canvas, Size size);

  void _capture(String type, Map<String, dynamic> data) {
    capturedCommands.add(DrawCommandSnapshot(type: type, data: data));
  }

  void _captureRect(String name, Rect rect, {String? color, double? strokeWidth}) {
    final data = <String, dynamic>{
      'name': name,
      'left': rect.left,
      'top': rect.top,
      'right': rect.right,
      'bottom': rect.bottom,
    };
    if (color != null) data['color'] = color;
    if (strokeWidth != null) data['strokeWidth'] = strokeWidth;
    _capture('drawRect', data);
  }

  void _captureCircle(String name, Offset center, double radius, {String? color}) {
    final data = <String, dynamic>{
      'name': name,
      'centerX': center.dx,
      'centerY': center.dy,
      'radius': radius,
    };
    if (color != null) data['color'] = color;
    _capture('drawCircle', data);
  }

  void _captureOval(String name, Rect rect, {String? color}) {
    final data = <String, dynamic>{
      'name': name,
      'left': rect.left,
      'top': rect.top,
      'right': rect.right,
      'bottom': rect.bottom,
    };
    if (color != null) data['color'] = color;
    _capture('drawOval', data);
  }

  void _captureLine(String name, Offset p1, Offset p2, {String? color, double? strokeWidth}) {
    final data = <String, dynamic>{
      'name': name,
      'x1': p1.dx,
      'y1': p1.dy,
      'x2': p2.dx,
      'y2': p2.dy,
    };
    if (color != null) data['color'] = color;
    if (strokeWidth != null) data['strokeWidth'] = strokeWidth;
    _capture('drawLine', data);
  }

  void _captureArc(String name, Rect rect, double startAngle, double sweepAngle, 
      {String? color, bool? useCenter}) {
    final data = <String, dynamic>{
      'name': name,
      'left': rect.left,
      'top': rect.top,
      'right': rect.right,
      'bottom': rect.bottom,
      'startAngle': startAngle,
      'sweepAngle': sweepAngle,
    };
    if (color != null) data['color'] = color;
    if (useCenter != null) data['useCenter'] = useCenter;
    _capture('drawArc', data);
  }

  void _capturePath(String name, Path path, {String? color, bool? fill}) {
    // Capture path commands
    final commands = <Map<String, dynamic>>[];
    // Note: Flutter's Path doesn't expose internal commands directly,
    // so we capture the bounding box and path type
    final bounds = path.getBounds();
    final data = <String, dynamic>{
      'name': name,
      'bounds_left': bounds.left,
      'bounds_top': bounds.top,
      'bounds_right': bounds.right,
      'bounds_bottom': bounds.bottom,
      'commands': commands,
    };
    if (color != null) data['color'] = color;
    if (fill != null) data['fill'] = fill;
    _capture('drawPath', data);
  }

  void _captureRRect(String name, RRect rrect, {String? color}) {
    final data = <String, dynamic>{
      'name': name,
      'left': rrect.left,
      'top': rrect.top,
      'right': rrect.right,
      'bottom': rrect.bottom,
      'tlRadiusX': rrect.tlRadiusX,
      'tlRadiusY': rrect.tlRadiusY,
      'trRadiusX': rrect.trRadiusX,
      'trRadiusY': rrect.trRadiusY,
      'blRadiusX': rrect.blRadiusX,
      'blRadiusY': rrect.blRadiusY,
      'brRadiusX': rrect.brRadiusX,
      'brRadiusY': rrect.brRadiusY,
    };
    if (color != null) data['color'] = color;
    _capture('drawRRect', data);
  }

  void _captureTransform(String name, Matrix4 matrix, {String? type}) {
    final data = <String, dynamic>{
      'name': name,
      'matrix': matrix.storage,
    };
    if (type != null) data['type'] = type;
    _capture('transform', data);
  }

  void _captureClip(String name, String clipType, {Rect? rect, RRect? rrect}) {
    final data = <String, dynamic>{
      'name': name,
      'clipType': clipType,
    };
    if (rect != null) {
      data['left'] = rect.left;
      data['top'] = rect.top;
      data['right'] = rect.right;
      data['bottom'] = rect.bottom;
    }
    if (rrect != null) {
      data['left'] = rrect.left;
      data['top'] = rrect.top;
      data['right'] = rrect.right;
      data['bottom'] = rrect.bottom;
      data['tlRadiusX'] = rrect.tlRadiusX;
      data['tlRadiusY'] = rrect.tlRadiusY;
    }
    _capture('clip', data);
  }

  void _captureSave(String name) {
    _capture('save', {'name': name});
  }

  void _captureRestore(String name) {
    _capture('restore', {'name': name});
  }

  @override
  bool shouldRepaint(covariant CustomPainter oldDelegate) => false;
}

class _BasicShapesPainter extends _CapturePainter {
  @override
  void paint(Canvas canvas, Size size) {
    capturedCommands.clear();

    // Draw filled rectangle
    final rectPaint = Paint()..color = Colors.blue;
    final rect = Rect.fromLTWH(20, 20, 100, 80);
    _captureRect('blue_rect', rect, color: '#FF0000FF');
    canvas.drawRect(rect, rectPaint);

    // Draw circle
    final circlePaint = Paint()..color = Colors.red;
    final center = Offset(200, 60);
    _captureCircle('red_circle', center, 40, color: '#FFFF0000');
    canvas.drawCircle(center, 40, circlePaint);

    // Draw oval
    final ovalPaint = Paint()..color = Colors.green;
    final ovalRect = Rect.fromLTWH(280, 20, 100, 80);
    _captureOval('green_oval', ovalRect, color: '#FF00FF00');
    canvas.drawOval(ovalRect, ovalPaint);

    // Draw stroked rectangle
    final strokePaint = Paint()
      ..color = Colors.purple
      ..style = PaintingStyle.stroke
      ..strokeWidth = 4;
    final strokeRect = Rect.fromLTWH(20, 120, 100, 80);
    _captureRect('purple_stroke_rect', strokeRect, 
        color: '#FF800080', strokeWidth: 4);
    canvas.drawRect(strokeRect, strokePaint);
  }
}

class _LinesAndPointsPainter extends _CapturePainter {
  @override
  void paint(Canvas canvas, Size size) {
    capturedCommands.clear();

    // Draw lines
    final linePaint = Paint()
      ..color = Colors.black
      ..strokeWidth = 2;

    final p1 = Offset(20, 20);
    final p2 = Offset(150, 100);
    _captureLine('line1', p1, p2, color: '#FF000000', strokeWidth: 2);
    canvas.drawLine(p1, p2, linePaint);

    final p3 = Offset(200, 20);
    final p4 = Offset(350, 150);
    _captureLine('line2', p3, p4, color: '#FF000000', strokeWidth: 2);
    canvas.drawLine(p3, p4, linePaint);

    // Draw thick line
    final thickPaint = Paint()
      ..color = Colors.orange
      ..strokeWidth = 8;
    final p5 = Offset(50, 200);
    final p6 = Offset(350, 250);
    _captureLine('thick_line', p5, p6, color: '#FFFFA500', strokeWidth: 8);
    canvas.drawLine(p5, p6, thickPaint);

    // Draw points
    final pointPaint = Paint()..color = Colors.red;
    final points = [
      Offset(100, 300),
      Offset(150, 320),
      Offset(200, 310),
      Offset(250, 330),
      Offset(300, 300),
    ];
    
    _capture('drawPoints', {
      'name': 'red_points',
      'mode': 'points',
      'count': points.length,
      'points': points.map((p) => {'x': p.dx, 'y': p.dy}).toList(),
      'color': '#FFFF0000',
    });
    
    for (final point in points) {
      canvas.drawCircle(point, 5, pointPaint);
    }
  }
}

class _PathsPainter extends _CapturePainter {
  @override
  void paint(Canvas canvas, Size size) {
    capturedCommands.clear();

    // Draw triangle path
    final trianglePath = Path()
      ..moveTo(100, 50)
      ..lineTo(50, 150)
      ..lineTo(150, 150)
      ..close();
    
    final trianglePaint = Paint()..color = Colors.blue.withOpacity(0.7);
    _capturePath('blue_triangle', trianglePath, color: '#B30000FF', fill: true);
    canvas.drawPath(trianglePath, trianglePaint);

    // Draw bezier curve path
    final curvePath = Path()
      ..moveTo(200, 150)
      ..quadraticBezierTo(250, 50, 300, 150);
    
    final curvePaint = Paint()
      ..color = Colors.green
      ..style = PaintingStyle.stroke
      ..strokeWidth = 3;
    _capturePath('green_curve', curvePath, color: '#FF00FF00', fill: false);
    canvas.drawPath(curvePath, curvePaint);

    // Draw cubic bezier
    final cubicPath = Path()
      ..moveTo(50, 250)
      ..cubicTo(100, 200, 150, 300, 200, 250);
    
    final cubicPaint = Paint()
      ..color = Colors.purple
      ..style = PaintingStyle.stroke
      ..strokeWidth = 4;
    _capturePath('purple_cubic', cubicPath, color: '#FF800080', fill: false);
    canvas.drawPath(cubicPath, cubicPaint);

    // Draw star polygon
    final starPath = Path();
    final centerX = 300.0;
    final centerY = 280.0;
    final outerRadius = 60.0;
    final innerRadius = 25.0;
    
    for (int i = 0; i < 10; i++) {
      final angle = (i * 36 - 90) * 3.14159 / 180;
      final radius = i.isEven ? outerRadius : innerRadius;
      final x = centerX + radius * cos(angle);
      final y = centerY + radius * sin(angle);
      if (i == 0) {
        starPath.moveTo(x, y);
      } else {
        starPath.lineTo(x, y);
      }
    }
    starPath.close();
    
    final starPaint = Paint()..color = Colors.orange.withOpacity(0.8);
    _capturePath('orange_star', starPath, color: '#CCFFA500', fill: true);
    canvas.drawPath(starPath, starPaint);
  }
}

class _RoundedRectsPainter extends _CapturePainter {
  @override
  void paint(Canvas canvas, Size size) {
    capturedCommands.clear();

    // Draw single rounded rectangle
    final rrect = RRect.fromRectAndRadius(
      Rect.fromLTWH(20, 20, 150, 100),
      Radius.circular(20),
    );
    final rrectPaint = Paint()..color = Colors.blue;
    _captureRRect('blue_rrect', rrect, color: '#FF0000FF');
    canvas.drawRRect(rrect, rrectPaint);

    // Draw different corner radii
    final unevenRRect = RRect.fromRectAndCorners(
      Rect.fromLTWH(200, 20, 150, 100),
      topLeft: Radius.circular(30),
      topRight: Radius.circular(10),
      bottomLeft: Radius.circular(5),
      bottomRight: Radius.circular(25),
    );
    final unevenPaint = Paint()..color = Colors.green;
    _captureRRect('green_uneven_rrect', unevenRRect, color: '#FF00FF00');
    canvas.drawRRect(unevenRRect, unevenPaint);

    // Draw double rounded rect (border effect)
    final outerRRect = RRect.fromRectAndRadius(
      Rect.fromLTWH(20, 150, 150, 100),
      Radius.circular(25),
    );
    final innerRRect = RRect.fromRectAndRadius(
      Rect.fromLTWH(35, 165, 120, 70),
      Radius.circular(15),
    );
    final drrectPaint = Paint()..color = Colors.purple;
    _captureRRect('purple_outer', outerRRect, color: '#FF800080');
    _captureRRect('purple_inner', innerRRect, color: '#FF800080');
    _capture('drawDRRect', {
      'name': 'purple_border',
      'outer': {
        'left': outerRRect.left,
        'top': outerRRect.top,
        'right': outerRRect.right,
        'bottom': outerRRect.bottom,
        'radius': 25,
      },
      'inner': {
        'left': innerRRect.left,
        'top': innerRRect.top,
        'right': innerRRect.right,
        'bottom': innerRRect.bottom,
        'radius': 15,
      },
      'color': '#FF800080',
    });
    canvas.drawDRRect(outerRRect, innerRRect, drrectPaint);

    // Stroked rounded rect
    final strokeRRect = RRect.fromRectAndRadius(
      Rect.fromLTWH(200, 150, 150, 100),
      Radius.circular(15),
    );
    final strokePaint = Paint()
      ..color = Colors.red
      ..style = PaintingStyle.stroke
      ..strokeWidth = 5;
    _captureRRect('red_stroke_rrect', strokeRRect, color: '#FFFF0000');
    canvas.drawRRect(strokeRRect, strokePaint);
  }
}

class _ArcsPainter extends _CapturePainter {
  @override
  void paint(Canvas canvas, Size size) {
    capturedCommands.clear();

    // Draw pie slice (useCenter = true)
    final pieRect = Rect.fromLTWH(20, 20, 150, 150);
    final piePaint = Paint()..color = Colors.blue;
    _captureArc('blue_pie', pieRect, 0, 1.5, 
        color: '#FF0000FF', useCenter: true);
    canvas.drawArc(pieRect, 0, 1.5, true, piePaint);

    // Draw arc segment (useCenter = false)
    final arcRect = Rect.fromLTWH(200, 20, 150, 150);
    final arcPaint = Paint()
      ..color = Colors.green
      ..style = PaintingStyle.stroke
      ..strokeWidth = 10;
    _captureArc('green_arc', arcRect, 0.5, 2.0, 
        color: '#FF00FF00', useCenter: false);
    canvas.drawArc(arcRect, 0.5, 2.0, false, arcPaint);

    // Multiple arcs forming a chart
    final chartCenter = Offset(100, 280);
    final chartRadius = 70.0;
    final chartRect = Rect.fromCenter(
      center: chartCenter,
      width: chartRadius * 2,
      height: chartRadius * 2,
    );

    final colors = [Colors.red, Colors.orange, Colors.yellow, Colors.green];
    final values = [0.3, 0.25, 0.2, 0.25]; // Percentages
    
    double currentAngle = -1.5708; // Start at top (-90 degrees)
    for (int i = 0; i < colors.length; i++) {
      final sweep = values[i] * 2 * 3.14159;
      final arcPaint = Paint()..color = colors[i];
      _captureArc('chart_segment_$i', chartRect, currentAngle, sweep,
          color: '#FF${colors[i].value.toRadixString(16).padLeft(8, '0').substring(2)}', 
          useCenter: true);
      canvas.drawArc(chartRect, currentAngle, sweep, true, arcPaint);
      currentAngle += sweep;
    }
  }
}

class _TransformsPainter extends _CapturePainter {
  @override
  void paint(Canvas canvas, Size size) {
    capturedCommands.clear();

    // Save initial state
    _captureSave('initial');
    canvas.save();

    // Translate and draw
    _captureSave('translated');
    canvas.save();
    canvas.translate(50, 50);
    _captureTransform('translate_50_50', Matrix4.translationValues(50, 50, 0),
        type: 'translate');
    
    final rectPaint = Paint()..color = Colors.blue;
    _captureRect('blue_translated', Rect.fromLTWH(0, 0, 80, 60), 
        color: '#FF0000FF');
    canvas.drawRect(Rect.fromLTWH(0, 0, 80, 60), rectPaint);
    
    _captureRestore('translated');
    canvas.restore();

    // Rotate around center
    _captureSave('rotated');
    canvas.save();
    final rotationMatrix = Matrix4.identity()
      ..translate(200.0, 100.0)
      ..rotateZ(0.785398) // 45 degrees
      ..translate(-40.0, -30.0);
    canvas.transform(rotationMatrix.storage);
    _captureTransform('rotate_45_center', rotationMatrix, type: 'rotate');
    
    final rotatedPaint = Paint()..color = Colors.red;
    _captureRect('red_rotated', Rect.fromLTWH(0, 0, 80, 60), 
        color: '#FFFF0000');
    canvas.drawRect(Rect.fromLTWH(0, 0, 80, 60), rotatedPaint);
    
    _captureRestore('rotated');
    canvas.restore();

    // Scale
    _captureSave('scaled');
    canvas.save();
    final scaleMatrix = Matrix4.identity()
      ..translate(350.0, 50.0)
      ..scale(1.5, 1.5);
    canvas.transform(scaleMatrix.storage);
    _captureTransform('scale_1.5', scaleMatrix, type: 'scale');
    
    final scaledPaint = Paint()..color = Colors.green;
    _captureRect('green_scaled', Rect.fromLTWH(0, 0, 60, 40), 
        color: '#FF00FF00');
    canvas.drawRect(Rect.fromLTWH(0, 0, 60, 40), scaledPaint);
    
    _captureRestore('scaled');
    canvas.restore();

    // Combined transforms
    _captureSave('combined');
    canvas.save();
    canvas.translate(100, 250);
    canvas.rotate(0.3);
    canvas.scale(1.2, 0.8);
    
    final combinedMatrix = Matrix4.identity()
      ..translate(100.0, 250.0)
      ..rotateZ(0.3)
      ..scale(1.2, 0.8);
    _captureTransform('combined_transform', combinedMatrix, 
        type: 'translate_rotate_scale');
    
    final combinedPaint = Paint()..color = Colors.purple;
    _captureRect('purple_combined', Rect.fromLTWH(0, 0, 100, 80), 
        color: '#FF800080');
    canvas.drawRect(Rect.fromLTWH(0, 0, 100, 80), combinedPaint);
    
    _captureRestore('combined');
    canvas.restore();

    _captureRestore('initial');
    canvas.restore();
  }
}

class _ClippingPainter extends _CapturePainter {
  @override
  void paint(Canvas canvas, Size size) {
    capturedCommands.clear();

    // Clip rect
    _captureSave('clip_rect');
    canvas.save();
    final clipRect = Rect.fromLTWH(20, 20, 150, 150);
    _captureClip('rect_clip', 'clipRect', rect: clipRect);
    canvas.clipRect(clipRect);
    
    // Draw large circle that gets clipped
    final circlePaint = Paint()..color = Colors.blue;
    _captureCircle('blue_circle_clipped', Offset(120, 120), 100, 
        color: '#FF0000FF');
    canvas.drawCircle(Offset(120, 120), 100, circlePaint);
    _captureRestore('clip_rect');
    canvas.restore();

    // Clip rounded rect
    _captureSave('clip_rrect');
    canvas.save();
    final clipRRect = RRect.fromRectAndRadius(
      Rect.fromLTWH(200, 20, 150, 150),
      Radius.circular(30),
    );
    _captureClip('rrect_clip', 'clipRRect', rrect: clipRRect);
    canvas.clipRRect(clipRRect);
    
    final rrectPaint = Paint()..color = Colors.red;
    _captureRect('red_rect_clipped', Rect.fromLTWH(180, 0, 200, 200), 
        color: '#FFFF0000');
    canvas.drawRect(Rect.fromLTWH(180, 0, 200, 200), rrectPaint);
    _captureRestore('clip_rrect');
    canvas.restore();

    // Clip path (circle shape)
    _captureSave('clip_path');
    canvas.save();
    final clipPath = Path()
      ..addOval(Rect.fromLTWH(50, 220, 120, 120));
    _capture('clipPath', {
      'name': 'circle_clip',
      'type': 'oval',
      'centerX': 110.0,
      'centerY': 280.0,
      'radiusX': 60.0,
      'radiusY': 60.0,
    });
    canvas.clipPath(clipPath);
    
    final pathPaint = Paint()..color = Colors.green;
    _captureRect('green_rect_clipped', Rect.fromLTWH(30, 200, 160, 160), 
        color: '#FF00FF00');
    canvas.drawRect(Rect.fromLTWH(30, 200, 160, 160), pathPaint);
    _captureRestore('clip_path');
    canvas.restore();
  }
}

class _PaintStylesPainter extends _CapturePainter {
  @override
  void paint(Canvas canvas, Size size) {
    capturedCommands.clear();

    // Fill style
    final fillPaint = Paint()
      ..color = Colors.blue
      ..style = PaintingStyle.fill;
    _captureRect('blue_fill', Rect.fromLTWH(20, 20, 100, 80), 
        color: '#FF0000FF');
    canvas.drawRect(Rect.fromLTWH(20, 20, 100, 80), fillPaint);

    // Stroke style
    final strokePaint = Paint()
      ..color = Colors.red
      ..style = PaintingStyle.stroke
      ..strokeWidth = 5;
    _captureRect('red_stroke', Rect.fromLTWH(140, 20, 100, 80), 
        color: '#FFFF0000', strokeWidth: 5);
    canvas.drawRect(Rect.fromLTWH(140, 20, 100, 80), strokePaint);

    // Different stroke widths
    for (int i = 0; i < 4; i++) {
      final width = (i + 1) * 3.0;
      final paint = Paint()
        ..color = Colors.green
        ..style = PaintingStyle.stroke
        ..strokeWidth = width;
      final y = 140.0 + i * 35;
      _captureLine('line_width_${width.toInt()}', 
          Offset(20, y), Offset(150, y),
          color: '#FF00FF00', strokeWidth: width);
      canvas.drawLine(Offset(20, y), Offset(150, y), paint);
    }

    // Blend modes (simplified - just draw overlapping shapes)
    final blendColors = [Colors.red, Colors.green, Colors.blue];
    final blendModes = [
      BlendMode.srcOver,
      BlendMode.multiply,
      BlendMode.screen,
    ];
    
    for (int i = 0; i < 3; i++) {
      final paint = Paint()
        ..color = blendColors[i].withOpacity(0.7)
        ..blendMode = blendModes[i];
      final x = 200.0 + i * 60;
      _captureCircle('blend_circle_$i', Offset(x, 150), 40,
          color: '#B3${blendColors[i].value.toRadixString(16).padLeft(8, '0').substring(2)}');
      canvas.drawCircle(Offset(x, 150), 40, paint);
    }
  }
}

class _ComplexScenePainter extends _CapturePainter {
  @override
  void paint(Canvas canvas, Size size) {
    capturedCommands.clear();

    _captureSave('scene');
    canvas.save();

    // Background with gradient-like effect using overlapping circles
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
    _capture('background', {'type': 'pattern', 'circles': 25});

    // Central transformed group
    _captureSave('central_group');
    canvas.save();
    canvas.translate(200, 200);
    canvas.rotate(0.2);
    _captureTransform('central_transform', 
        Matrix4.translationValues(200, 200, 0)..rotateZ(0.2),
        type: 'translate_rotate');

    // Draw rounded rect background
    final cardRRect = RRect.fromRectAndRadius(
      Rect.fromLTWH(-120, -80, 240, 160),
      Radius.circular(20),
    );
    final cardPaint = Paint()..color = Colors.white.withOpacity(0.9);
    _captureRRect('card', cardRRect, color: '#E6FFFFFF');
    canvas.drawRRect(cardRRect, cardPaint);

    // Card border
    final borderPaint = Paint()
      ..color = Colors.blue.shade300
      ..style = PaintingStyle.stroke
      ..strokeWidth = 3;
    canvas.drawRRect(cardRRect, borderPaint);

    // Inner content - clipped to card
    _captureSave('clipped_content');
    canvas.save();
    canvas.clipRRect(cardRRect);
    _captureClip('card_clip', 'clipRRect', rrect: cardRRect);

    // Draw some circles inside
    for (int i = 0; i < 3; i++) {
      final paint = Paint()
        ..color = [Colors.red, Colors.green, Colors.orange][i].withOpacity(0.6);
      final x = -60.0 + i * 60;
      _captureCircle('inner_circle_$i', Offset(x, 0), 35,
          color: '#99${[Colors.red, Colors.green, Colors.orange][i].value.toRadixString(16).padLeft(8, '0').substring(2)}');
      canvas.drawCircle(Offset(x, 0), 35, paint);
    }

    _captureRestore('clipped_content');
    canvas.restore();
    _captureRestore('central_group');
    canvas.restore();

    // Corner decorations
    final corners = [
      Offset(30, 30),
      Offset(370, 30),
      Offset(30, 370),
      Offset(370, 370),
    ];
    
    for (int i = 0; i < corners.length; i++) {
      _captureSave('corner_$i');
      canvas.save();
      canvas.translate(corners[i].dx, corners[i].dy);
      canvas.rotate(i * 1.5708); // 90 degree increments
      
      final paint = Paint()..color = Colors.purple.withOpacity(0.5);
      final path = Path()
        ..moveTo(0, -15)
        ..lineTo(10, 0)
        ..lineTo(0, 15)
        ..close();
      _capturePath('corner_shape_$i', path, color: '#80FF00FF', fill: true);
      canvas.drawPath(path, paint);
      
      _captureRestore('corner_$i');
      canvas.restore();
    }

    _captureRestore('scene');
    canvas.restore();
  }
}
