import 'dart:convert';
import 'package:flutter/rendering.dart';
import 'package:flutter/material.dart';

/// Serializable representation of a RenderObject node
class RenderNodeSnapshot {
  final String type;
  final String? id;
  final double width;
  final double height;
  final double offsetX;
  final double offsetY;
  final double constraintMinW;
  final double constraintMaxW;
  final double constraintMinH;
  final double constraintMaxH;
  final List<RenderNodeSnapshot> children;
  final Map<String, String> properties;

  RenderNodeSnapshot({
    required this.type,
    this.id,
    required this.width,
    required this.height,
    required this.offsetX,
    required this.offsetY,
    required this.constraintMinW,
    required this.constraintMaxW,
    required this.constraintMinH,
    required this.constraintMaxH,
    this.children = const [],
    this.properties = const {},
  });

  Map<String, dynamic> toJson() {
    final json = <String, dynamic>{
      'type': type,
      'size': {
        'width': _formatFloat(width),
        'height': _formatFloat(height),
      },
      'offset': {
        'x': _formatFloat(offsetX),
        'y': _formatFloat(offsetY),
      },
      'constraints': {
        'min_width': _formatFloat(constraintMinW),
        'max_width': constraintMaxW.isInfinite ? 'Infinity' : _formatFloat(constraintMaxW),
        'min_height': _formatFloat(constraintMinH),
        'max_height': constraintMaxH.isInfinite ? 'Infinity' : _formatFloat(constraintMaxH),
      },
    };

    if (id != null) {
      json['id'] = id;
    }

    if (properties.isNotEmpty) {
      json['properties'] = properties;
    }

    if (children.isNotEmpty) {
      json['children'] = children.map((c) => c.toJson()).toList();
    }

    return json;
  }

  static String _formatFloat(double f) {
    if (f.isInfinite) return f > 0 ? 'Infinity' : '-Infinity';
    if (f.isNaN) return 'NaN';
    // Format to 6 decimal places, then trim trailing zeros
    String result = f.toStringAsFixed(6);
    result = result.replaceAll(RegExp(r'0+$'), '');
    if (result.endsWith('.')) result = '${result}0';
    return result;
  }
}

/// Serializable representation of a draw command
class DrawCommandSnapshot {
  final String type;
  final Map<String, dynamic> data;

  DrawCommandSnapshot({
    required this.type,
    this.data = const {},
  });

  Map<String, dynamic> toJson() {
    final json = <String, dynamic>{
      'type': type,
    };
    json.addAll(data);
    return json;
  }
}

/// Complete fidelity snapshot
class FidelitySnapshot {
  final double viewportWidth;
  final double viewportHeight;
  final RenderNodeSnapshot layout;
  final List<DrawCommandSnapshot> paintCommands;
  final String? description;

  FidelitySnapshot({
    required this.viewportWidth,
    required this.viewportHeight,
    required this.layout,
    this.paintCommands = const [],
    this.description,
  });

  Map<String, dynamic> toJson() {
    final json = <String, dynamic>{
      'viewport': {
        'width': viewportWidth,
        'height': viewportHeight,
      },
      'layout': layout.toJson(),
    };

    if (description != null) {
      json['description'] = description;
    }

    if (paintCommands.isNotEmpty) {
      json['paint_commands'] = paintCommands.map((c) => c.toJson()).toList();
    }

    return json;
  }

  String toJsonString({int indent = 2}) {
    final encoder = JsonEncoder.withIndent(' ' * indent);
    return encoder.convert(toJson());
  }
}

/// Captures the render tree from a RenderObject
class RenderTreeCapture {
  /// Captures a snapshot of the render tree
  static RenderNodeSnapshot captureRenderTree(
    RenderObject renderObject, {
    Offset offset = Offset.zero,
    BoxConstraints? parentConstraints,
  }) {
    final String type = renderObject.runtimeType.toString();
    
    // Get size
    final Size size = renderObject is RenderBox 
        ? renderObject.size 
        : Size.zero;

    // Get constraints
    final BoxConstraints constraints = parentConstraints ?? BoxConstraints.tight(size);

    // Build properties map
    final properties = <String, String>{};

    // Handle specific render object types
    if (renderObject is RenderFlex) {
      properties['axis'] = renderObject.direction == Axis.horizontal ? 'horizontal' : 'vertical';
      properties['main_axis_size'] = renderObject.mainAxisSize == MainAxisSize.max ? 'max' : 'min';
      properties['main_axis_alignment'] = _mainAxisAlignmentToString(renderObject.mainAxisAlignment);
      properties['cross_axis_alignment'] = _crossAxisAlignmentToString(renderObject.crossAxisAlignment);
    }

    if (renderObject is RenderPadding) {
      final padding = renderObject.padding as EdgeInsets;
      properties['padding_left'] = padding.left.toString();
      properties['padding_top'] = padding.top.toString();
      properties['padding_right'] = padding.right.toString();
      properties['padding_bottom'] = padding.bottom.toString();
    }

    if (renderObject is RenderDecoratedBox) {
      final decoration = renderObject.decoration as BoxDecoration;
      if (decoration.color != null) {
        properties['color'] = _colorToHex(decoration.color!);
      }
    }

    if (renderObject is RenderConstrainedBox) {
      final boxConstraints = renderObject.constraints as BoxConstraints;
      if (boxConstraints.minWidth == boxConstraints.maxWidth) {
        properties['width'] = boxConstraints.minWidth.toString();
      }
      if (boxConstraints.minHeight == boxConstraints.maxHeight) {
        properties['height'] = boxConstraints.minHeight.toString();
      }
    }

    if (renderObject is RenderPositionedBox) {
      if (renderObject.widthFactor != null) {
        properties['width_factor'] = renderObject.widthFactor.toString();
      }
      if (renderObject.heightFactor != null) {
        properties['height_factor'] = renderObject.heightFactor.toString();
      }
      properties['alignment'] = _alignmentToString(renderObject.alignment as AlignmentGeometry);
    }

    // Capture children
    final children = <RenderNodeSnapshot>[];
    
    if (renderObject is RenderBox) {
      // Visit children
      RenderBox? child = renderObject is RenderProxyBox 
          ? (renderObject as RenderProxyBox).child 
          : null;
      
      if (child != null) {
        // For single-child widgets
        final childOffset = _getChildOffset(renderObject, child);
        children.add(captureRenderTree(
          child,
          offset: offset + childOffset,
          parentConstraints: _getChildConstraints(renderObject, child),
        ));
      } else if (renderObject is RenderFlex) {
        // For multi-child widgets (Flex, Stack, etc.)
        RenderBox? child = renderObject.firstChild;
        while (child != null) {
          final childParentData = child.parentData as FlexParentData;
          final childOffset = childParentData.offset;
          children.add(captureRenderTree(
            child,
            offset: offset + (childOffset ?? Offset.zero),
            parentConstraints: _getChildConstraints(renderObject, child),
          ));
          child = childParentData.nextSibling;
        }
      } else if (renderObject is RenderStack) {
        RenderBox? child = renderObject.firstChild;
        while (child != null) {
          final childParentData = child.parentData as StackParentData;
          final childOffset = childParentData.offset;
          children.add(captureRenderTree(
            child,
            offset: offset + (childOffset ?? Offset.zero),
            parentConstraints: _getChildConstraints(renderObject, child),
          ));
          child = childParentData.nextSibling;
        }
      }
    }

    return RenderNodeSnapshot(
      type: type,
      width: size.width,
      height: size.height,
      offsetX: offset.dx,
      offsetY: offset.dy,
      constraintMinW: constraints.minWidth,
      constraintMaxW: constraints.maxWidth,
      constraintMinH: constraints.minHeight,
      constraintMaxH: constraints.maxHeight,
      children: children,
      properties: properties,
    );
  }

  // Helper methods
  static Offset _getChildOffset(RenderObject parent, RenderBox child) {
    final parentData = child.parentData;
    if (parentData is BoxParentData) {
      return parentData.offset;
    }
    return Offset.zero;
  }

  static BoxConstraints _getChildConstraints(RenderObject parent, RenderBox child) {
    // This is an approximation - exact constraints would require access to
    // internal layout state
    return BoxConstraints.tight(child.size);
  }

  static String _mainAxisAlignmentToString(MainAxisAlignment alignment) {
    switch (alignment) {
      case MainAxisAlignment.start:
        return 'start';
      case MainAxisAlignment.end:
        return 'end';
      case MainAxisAlignment.center:
        return 'center';
      case MainAxisAlignment.spaceBetween:
        return 'space_between';
      case MainAxisAlignment.spaceAround:
        return 'space_around';
      case MainAxisAlignment.spaceEvenly:
        return 'space_evenly';
    }
  }

  static String _crossAxisAlignmentToString(CrossAxisAlignment alignment) {
    switch (alignment) {
      case CrossAxisAlignment.start:
        return 'start';
      case CrossAxisAlignment.end:
        return 'end';
      case CrossAxisAlignment.center:
        return 'center';
      case CrossAxisAlignment.stretch:
        return 'stretch';
      case CrossAxisAlignment.baseline:
        return 'baseline';
    }
  }

  static String _alignmentToString(AlignmentGeometry alignment) {
    if (alignment is Alignment) {
      if (alignment == Alignment.topLeft) return 'top_left';
      if (alignment == Alignment.topCenter) return 'top_center';
      if (alignment == Alignment.topRight) return 'top_right';
      if (alignment == Alignment.centerLeft) return 'center_left';
      if (alignment == Alignment.center) return 'center';
      if (alignment == Alignment.centerRight) return 'center_right';
      if (alignment == Alignment.bottomLeft) return 'bottom_left';
      if (alignment == Alignment.bottomCenter) return 'bottom_center';
      if (alignment == Alignment.bottomRight) return 'bottom_right';
    }
    return alignment.toString();
  }

  static String _colorToHex(Color color) {
    return '#${color.alpha.toRadixString(16).padLeft(2, '0')}'
           '${color.red.toRadixString(16).padLeft(2, '0')}'
           '${color.green.toRadixString(16).padLeft(2, '0')}'
           '${color.blue.toRadixString(16).padLeft(2, '0')}';
  }
}
