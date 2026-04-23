# Proposal: Widget Debugging & Inspection Toolkit for campello_widgets

## Executive Summary

Flutter's debugging ecosystem is one of its biggest competitive advantages. The combination of **structured diagnostics**, **visual debug overlays**, **runtime tree inspection**, and **DevTools integration** makes hunting layout bugs orders of magnitude faster than in traditional UI frameworks.

 campello_widgets currently has a solid foundation (`DebugFlags` with `paintSizeEnabled`, `repaintRainbowEnabled`, `showPerformanceOverlay`, `showDebugBanner`) but lacks the *structured introspection* layer that makes Flutter's inspector so powerful. When a tab renders blank, you currently have to reason backwards from symptoms. Flutter's tooling lets you reason forwards from data.

This proposal analyzes how Flutter's debugging architecture works and presents a phased implementation plan to bring equivalent capabilities to campello_widgets.

---

## Part 1: How Flutter's Debugging Architecture Works

### 1.1 The Three-Layer Diagnostic Model

Flutter separates debug information into three cooperating layers:

```
┌─────────────────────────────────────────────────────────────┐
│  LAYER 3: External Tools (DevTools, IDE plugins)            │
│  - Widget Inspector panel                                   │
│  - Layout Explorer                                          │
│  - Performance overlay                                      │
│  - Network profiler                                         │
└─────────────────────────────────────────────────────────────┘
                              ▲
                              │ JSON over VM Service Protocol
                              │ (ext.flutter.inspector.*)
┌─────────────────────────────────────────────────────────────┐
│  LAYER 2: Inspector Service (WidgetInspectorService)        │
│  - Registers service extensions                             │
│  - Maintains selection state                                │
│  - Serializes DiagnosticsNode trees to JSON                 │
│  - Handles "select widget mode" hit-testing                 │
└─────────────────────────────────────────────────────────────┘
                              ▲
                              │ Queries
┌─────────────────────────────────────────────────────────────┐
│  LAYER 1: Diagnostics Framework (foundation/diagnostics.dart)│
│  - DiagnosticableTree mixin                                 │
│  - DiagnosticsNode hierarchy                                │
│  - debugFillProperties() hooks                              │
│  - debugDescribeChildren() hooks                            │
│  - Typed property classes (DiagnosticsProperty, IntProperty,│
│    DoubleProperty, EnumProperty, FlagProperty, etc.)        │
└─────────────────────────────────────────────────────────────┘
```

**Key insight:** Layer 1 is pure framework code with zero dependencies on the VM or DevTools. Layer 2 bridges Layer 1 to the Dart VM's service protocol. Layer 3 is entirely external. This separation means the diagnostics layer is always available in debug builds, even if no debugger is attached.

### 1.2 The `DiagnosticableTree` Mixin

Every significant object in Flutter implements `DiagnosticableTree` (or `Diagnosticable` for leaf objects):

```dart
abstract class DiagnosticableTree with Diagnosticable {
  DiagnosticableTreeNode toDiagnosticsNode({String? name, DiagnosticsTreeStyle? style});
  List<DiagnosticsNode> debugDescribeChildren();
  void debugFillProperties(DiagnosticPropertiesBuilder properties);
}
```

**Widget**, **Element**, and **RenderObject** all inherit from `DiagnosticableTree`. This means *any* widget can describe itself, its properties, and its children in a structured way.

### 1.3 `debugFillProperties()` — The Self-Description Hook

This is the workhorse of Flutter diagnostics. Every concrete class overrides it to expose its configuration:

```dart
// Example from Flutter's Container widget
@override
void debugFillProperties(DiagnosticPropertiesBuilder properties) {
  super.debugFillProperties(properties);
  properties.add(DiagnosticsProperty<AlignmentGeometry>('alignment', alignment, defaultValue: null));
  properties.add(DiagnosticsProperty<EdgeInsetsGeometry>('padding', padding, defaultValue: null));
  properties.add(ColorProperty('color', color, defaultValue: null));
  properties.add(DiagnosticsProperty<Decoration>('decoration', decoration, defaultValue: null));
  properties.add(DoubleProperty('width', width, defaultValue: null));
  properties.add(DoubleProperty('height', height, defaultValue: null));
  properties.add(DiagnosticsProperty<BoxConstraints>('constraints', constraints, defaultValue: null));
  properties.add(DiagnosticsProperty<EdgeInsetsGeometry>('margin', margin, defaultValue: null));
}
```

**Why this matters:** Without `debugFillProperties`, a debugging tool can only tell you "this is a Container." With it, the tool can tell you "this is a Container with width=100, height=200, color=red, padding=EdgeInsets.all(8)."

### 1.4 `DiagnosticsNode` — The Typed Property Hierarchy

Flutter has ~15 specialized property types:

| Class | Purpose |
|-------|---------|
| `DiagnosticsProperty<T>` | Generic typed property |
| `StringProperty` | String values |
| `DoubleProperty` | Float/double values (with precision formatting) |
| `IntProperty` | Integer values |
| `ColorProperty` | Colors (shows swatch + hex) |
| `EnumProperty<T>` | Enum values |
| `FlagProperty` | Boolean flags (shows as presence/absence) |
| `IterableProperty<T>` | Lists/collections |
| `ObjectFlagProperty<T>` | Objects shown only when non-null |
| `PercentProperty` | Values 0.0–1.0 shown as percentages |
| `MessageProperty` | Free-form text messages |
| `DiagnosticsBlock` | Grouped sub-blocks of properties |

Each property carries metadata:
- `defaultValue`: Hide the property when it equals the default
- `level`: `DiagnosticLevel.hidden`, `.info`, `.debug`, `.warning`, `.error`
- `showName`: Whether to show "name: value" or just "value"
- `ifNull`, `ifEmpty`: Custom text when value is null/empty
- `tooltip`: Extra explanatory text

### 1.5 `debugDescribeChildren()` — The Tree Structure Hook

While `debugFillProperties` describes *properties* of a node, `debugDescribeChildren` describes the *child nodes* in the tree:

```dart
// Example from Element
@override
List<DiagnosticsNode> debugDescribeChildren() {
  final List<DiagnosticsNode> children = <DiagnosticsNode>[];
  visitChildren((Element child) {
    children.add(child.toDiagnosticsNode());
  });
  return children;
}
```

This is what lets the Widget Inspector show a collapsible tree.

### 1.6 The WidgetInspectorService Protocol

Flutter exposes these service extensions (called via the Dart VM protocol):

| Extension | Purpose |
|-----------|---------|
| `ext.flutter.inspector.getRootWidgetSummaryTree` | JSON tree of Widgets |
| `ext.flutter.inspector.getRootRenderObjectTree` | JSON tree of RenderObjects |
| `ext.flutter.inspector.getSelectedWidget` | Details of clicked widget |
| `ext.flutter.inspector.getParentChain` | Ancestors of selected widget |
| `ext.flutter.inspector.getChildrenDetailsSubtree` | Deep properties of children |
| `ext.flutter.inspector.screenshot` | Raster screenshot of a subtree |
| `ext.flutter.inspector.setSelectionById` | Programmatically select widget |
| `ext.flutter.inspector.isWidgetTreeReady` | Check if first frame rendered |
| `ext.flutter.inspector.trackRebuildDirtyWidgets` | Track which widgets rebuild |

The Inspector Service also handles **Select Widget Mode**: when enabled, every tap on the device is intercepted, hit-tested against the render tree, and the corresponding Element is reported as "selected."

### 1.7 Visual Debug Overlays

Flutter has a rich set of visual debugging flags:

| Flag | Visual Effect | Use Case |
|------|--------------|----------|
| `debugPaintSizeEnabled` | Cyan border + constraints/size text | Layout debugging |
| `debugPaintBaselinesEnabled` | Green/yellow baseline lines | Text alignment |
| `debugPaintLayerBordersEnabled` | Orange borders around compositing layers | Layer optimization |
| `debugPaintPointersEnabled` | Colored dots at touch points | Gesture debugging |
| `debugRepaintRainbowEnabled` | Cycling rainbow border on repaint | Excess repaint detection |
| `debugInvertOversizedImages` | Inverts images larger than display | Image memory optimization |
| `debugProfileBuildsEnabled` | Prints build times to console | Build performance |
| `debugPrintRebuildDirtyWidgets` | Prints widget names on rebuild | Rebuild storm detection |

These are implemented as **extra paint calls** inside `RenderObject.paint()` or as hooks in the rendering pipeline, gated by global bools.

### 1.8 Track Widget Creation (Source Location)

Flutter's compiler can instrument widget constructors with `_Location` objects that record file, line, and column. This lets the inspector show *exactly where in source code* a widget was instantiated. Without it, the tree is a sea of anonymous `Text`, `Container`, `Padding` nodes.

### 1.9 Error Reporting with Diagnostics

When Flutter catches an exception, it wraps it in `FlutterErrorDetails` which includes a `DiagnosticsNode` context tree. This means error messages include structured information about the widget tree at the point of failure.

---

## Part 2: Current State of campello_widgets Debugging

### What Already Exists (and works well)

```cpp
struct DebugFlags {
    static bool paintSizeEnabled;        // Cyan borders (Flutter equivalent)
    static bool repaintRainbowEnabled;   // Rainbow fill on dirty repaint
    static bool showDebugBanner;         // Top-right DEBUG ribbon
    static bool showPerformanceOverlay;  // FPS chart + frame times
};
```

These are integrated into `RenderObject::paint()` and `Renderer::generateDrawList()`, properly guarded so there's zero overhead when disabled.

### What's Missing (and hurting debuggability)

| Missing Capability | Impact |
|-------------------|--------|
| No structured property exposure on Widget/Element/RenderObject | Can't ask "what are the properties of this widget?" |
| No tree serialization | Can't dump the widget tree to console or file |
| No child enumeration API on Element | Can't walk the element tree generically |
| No hit-test → Element mapping | Can't implement "click to inspect" |
| No source location tracking | Widgets are anonymous in any output |
| No baseline/layer/pointer debug paint | Limited visual debugging |
| No build/rebuild counters | Can't detect rebuild storms |
| No runtime debug panel | All flags require recompile to change |
| No assertion with tree context | Errors don't include surrounding tree state |

---

## Part 3: Proposed Implementation Plan

### Phase 1: Diagnostics Framework (Foundation)
**Goal:** Give every Widget, Element, and RenderObject the ability to describe itself.

#### 3.1.1 `DiagnosticProperty<T>` and Friends

Create a lightweight C++ diagnostics type system:

```cpp
// inc/campello_widgets/diagnostics/diagnostic_property.hpp
namespace systems::leal::campello_widgets {

enum class DiagnosticLevel { hidden, fine, debug, info, warning, error, summary };

class DiagnosticPropertyBase {
public:
    std::string name;
    DiagnosticLevel level = DiagnosticLevel::info;
    bool showName = true;
    bool showSeparator = true;
    std::string tooltip;
    std::string defaultValue;       // If value == defaultValue, omit from output
    std::string ifNull;             // Text to show when value is null
    
    virtual ~DiagnosticPropertyBase() = default;
    virtual std::string valueToString() const = 0;
    virtual bool isInteresting() const = 0;  // false if value == defaultValue
};

template<typename T>
class DiagnosticProperty : public DiagnosticPropertyBase {
public:
    T value;
    std::string valueToString() const override;  // SFINAE/overloads for common types
    bool isInteresting() const override;
};

// Aliases for convenience
using StringProperty   = DiagnosticProperty<std::string>;
using DoubleProperty   = DiagnosticProperty<double>;
using IntProperty      = DiagnosticProperty<int>;
using BoolProperty     = DiagnosticProperty<bool>;
using ColorProperty    = DiagnosticProperty<Color>;
using SizeProperty     = DiagnosticProperty<Size>;
using OffsetProperty   = DiagnosticProperty<Offset>;
using ConstraintsProperty = DiagnosticProperty<BoxConstraints>;

class FlagProperty : public DiagnosticPropertyBase {
public:
    bool value;
    std::string ifTrue;
    std::string ifFalse;
    // Shows as a presence flag, e.g. "enabled" or "disabled"
};

class MessageProperty : public DiagnosticPropertyBase {
public:
    std::string message;
    // Always shows the message text
};

} // namespace
```

**Design note:** Keep this allocation-light. Use `std::vector<std::unique_ptr<DiagnosticPropertyBase>>` in the builder. In debug builds, memory overhead is acceptable. In release builds, this entire subsystem compiles away via `#ifdef NDEBUG` or constexpr bool flags.

#### 3.1.2 `DiagnosticsNode` — The Tree Node

```cpp
// inc/campello_widgets/diagnostics/diagnostics_node.hpp
namespace systems::leal::campello_widgets {

enum class DiagnosticsTreeStyle { sparse, offstage, dense, transition };

class DiagnosticsNode {
public:
    std::string name;                    // e.g. "Container", "Text"
    std::string description;             // e.g. "Container(width: 100, height: 200)"
    DiagnosticsTreeStyle style = DiagnosticsTreeStyle::sparse;
    DiagnosticLevel level = DiagnosticLevel::info;
    
    std::vector<std::unique_ptr<DiagnosticPropertyBase>> properties;
    std::vector<std::shared_ptr<DiagnosticsNode>> children;
    
    // Serialization
    std::string toString() const;
    std::string toStringDeep(std::string prefix = "") const;
    nlohmann::json toJson() const;       // For external tool consumption
};

} // namespace
```

*(Note: `nlohmann::json` is header-only and can be added as a dependency, or we can write a simple custom JSON serializer.)*

#### 3.1.3 `Diagnosticable` Mixin

```cpp
// inc/campello_widgets/diagnostics/diagnosticable.hpp
namespace systems::leal::campello_widgets {

class Diagnosticable {
public:
    virtual ~Diagnosticable() = default;
    
    // Override to add typed properties
    virtual void debugFillProperties(std::vector<std::unique_ptr<DiagnosticPropertyBase>>& properties) const {}
    
    // Override to describe children in diagnostic trees
    virtual std::vector<std::shared_ptr<DiagnosticsNode>> debugDescribeChildren() const { return {}; }
    
    // Build a complete DiagnosticsNode for this object
    virtual std::shared_ptr<DiagnosticsNode> toDiagnosticsNode(const std::string& name = "") const;
    
    // Convenience: shallow string representation
    virtual std::string toStringShort() const;
    
    // Convenience: deep string representation
    std::string toStringDeep() const;
};

} // namespace
```

#### 3.1.4 Integration into the Three Trees

**Widget:**
```cpp
class Widget : public std::enable_shared_from_this<Widget>, public Diagnosticable {
    // ... existing code ...
    
    std::string toStringShort() const override {
        return typeid(*this).name();  // Or better: demangled type name
    }
};
```

**Element:**
```cpp
class Element : public BuildContext, public Diagnosticable {
    // ... existing code ...
    
    void debugFillProperties(std::vector<std::unique_ptr<DiagnosticPropertyBase>>& out) const override {
        out.push_back(std::make_unique<StringProperty>("widget", toStringShort(widget_->toStringShort())));
        out.push_back(std::make_unique<StringProperty>("dirty", dirty_ ? "true" : "false"));
        out.push_back(std::make_unique<StringProperty>("depth", std::to_string(depth())));
    }
    
    std::vector<std::shared_ptr<DiagnosticsNode>> debugDescribeChildren() const override {
        std::vector<std::shared_ptr<DiagnosticsNode>> result;
        visitChildren([&](const Element* child) {
            result.push_back(child->toDiagnosticsNode());
        });
        return result;
    }
    
    // NEW: Generic child visitor (needed for tree walking)
    virtual void visitChildren(const std::function<void(Element*)>& visitor) {
        if (auto* c = firstChildElement()) visitor(c);
    }
};
```

**RenderObject:**
```cpp
class RenderObject : public Diagnosticable {
    // ... existing code ...
    
    void debugFillProperties(std::vector<std::unique_ptr<DiagnosticPropertyBase>>& out) const override {
        out.push_back(std::make_unique<ConstraintsProperty>("constraints", constraints_));
        out.push_back(std::make_unique<SizeProperty>("size", size_));
    }
};
```

**Concrete widget example (SizedBox):**
```cpp
class SizedBox : public SingleChildRenderObjectWidget {
    // ... existing code ...
    
    void debugFillProperties(std::vector<std::unique_ptr<DiagnosticPropertyBase>>& out) const override {
        SingleChildRenderObjectWidget::debugFillProperties(out);
        if (width.has_value())
            out.push_back(std::make_unique<DoubleProperty>("width", *width));
        if (height.has_value())
            out.push_back(std::make_unique<DoubleProperty>("height", *height));
    }
};
```

### Phase 2: Tree Inspector Service
**Goal:** Enable runtime querying and visualization of the three trees.

#### 3.2.1 `WidgetInspector` Singleton

```cpp
// inc/campello_widgets/diagnostics/widget_inspector.hpp
namespace systems::leal::campello_widgets {

class WidgetInspector {
public:
    static WidgetInspector& instance();
    
    // --- Tree snapshots ---
    std::shared_ptr<DiagnosticsNode> getWidgetTree() const;
    std::shared_ptr<DiagnosticsNode> getRenderObjectTree() const;
    std::string getWidgetTreeJson() const;
    std::string getRenderObjectTreeJson() const;
    
    // --- Selection ---
    void setSelectedElement(Element* element);
    Element* selectedElement() const noexcept { return selected_element_; }
    
    // --- Select Widget Mode ---
    void setSelectModeEnabled(bool enabled);
    bool isSelectModeEnabled() const noexcept { return select_mode_enabled_; }
    
    // Called by PointerDispatcher when select mode is on
    bool onSelectModeTap(const Offset& position);
    
    // --- Rebuild tracking ---
    void recordRebuild(Element* element);
    void resetRebuildCounters();
    std::vector<std::pair<Element*, int>> rebuildCounts() const;
    
    // --- Dump to console ---
    void dumpWidgetTree() const;
    void dumpRenderObjectTree() const;
    
private:
    Element* selected_element_ = nullptr;
    bool select_mode_enabled_ = false;
    std::unordered_map<Element*, int> rebuild_counts_;
};

} // namespace
```

#### 3.2.2 Hit-Test → Element Mapping (Select Widget Mode)

This requires extending the hit-test system to carry `Element*` pointers, not just `RenderBox*` pointers. Currently `HitTestResult` collects `HitTestEntry` with `RenderBox*`. We need to map each hit `RenderBox` back to its owning `Element`.

**Approach:** Store an `Element*` weak back-pointer in `RenderObjectElement` that gets set during `mount()`. Then during hit-test, after finding the deepest `RenderBox`, walk up the render tree to find the nearest `RenderObjectElement` and return its `Element*`.

```cpp
// In WidgetInspector::onSelectModeTap:
HitTestResult result;
if (root_render_box_->hitTest(result, position)) {
    for (const auto& entry : result.path()) {
        if (auto* element = findElementForRenderBox(entry.renderBox)) {
            setSelectedElement(element);
            break;
        }
    }
}
```

#### 3.2.3 In-App Debug Overlay Panel

Since campello_widgets is C++ (no Dart VM service protocol), the "DevTools" equivalent should be an in-app overlay panel, togglable at runtime:

```cpp
// New debug overlay widget (only compiled in debug builds)
class DebugOverlayPanel : public StatelessWidget {
    // Shows:
    // - Toggle switches for all DebugFlags
    // - Live widget tree view (collapsible)
    // - Selected widget details panel
    // - Rebuild counter list
    // - Frame time graph (mini version)
};
```

Triggered by a global hotkey (e.g. F12 or Cmd+Shift+D) or a swipe gesture.

### Phase 3: Enhanced Visual Debugging
**Goal:** Expand debug paint to cover more scenarios.

#### 3.3.1 New Debug Flags

```cpp
struct DebugFlags {
    // Existing flags
    static bool paintSizeEnabled;
    static bool repaintRainbowEnabled;
    static bool showDebugBanner;
    static bool showPerformanceOverlay;
    
    // NEW:
    static bool paintBaselinesEnabled;      // Green/yellow baseline lines on text
    static bool paintLayerBordersEnabled;   // Orange borders around compositing layers
    static bool paintPointersEnabled;       // Colored dots at recent touch points
    static bool printRebuildsEnabled;       // Log widget name to stdout on rebuild
    static bool profileBuildsEnabled;       // Time build() and print slow ones
    static bool highlightOversizedImages;   // Tint images decoded larger than drawn
    static bool showInspectorPanel;         // Show the in-app debug overlay
};
```

#### 3.3.2 Baseline Debug Paint

`RenderText` and `RenderParagraph` should implement `debugPaint()`:

```cpp
void RenderText::debugPaint(PaintContext& context, const Offset& offset) {
    if (DebugFlags::paintBaselinesEnabled) {
        const float baseline = offset.y + textMetrics().baseline;
        const Paint paint = Paint::stroked(Color::fromRGBA(0.0f, 0.8f, 0.0f, 0.7f), 1.0f);
        context.canvas().drawLine(
            Offset{offset.x, baseline},
            Offset{offset.x + size_.width, baseline},
            paint);
    }
}
```

#### 3.3.3 Pointer Debug Paint

`PointerDispatcher` keeps a ring buffer of recent pointer events. In `Renderer::generateDrawList()`, after the normal paint, if `paintPointersEnabled`, draw circles at recent pointer positions with fade-out based on age.

### Phase 4: Widget Creation Tracking (Source Locations)
**Goal:** Show file:line where each widget was constructed.

#### 3.4.1 Macro-Based Instrumentation

Use C++ macros and `std::source_location` (C++20) to capture construction sites:

```cpp
// inc/campello_widgets/diagnostics/widget_location.hpp
struct WidgetLocation {
    std::string file;
    int line;
    int column;
    std::string function;
};

// Macro that every widget constructor should use
#define WIDGET_CONSTRUCTOR \
    void captureLocation(const std::source_location& loc = std::source_location::current()) { \
        location_ = WidgetLocation{loc.file_name(), static_cast<int>(loc.line()), \
                                   static_cast<int>(loc.column()), loc.function_name()}; \
    }
```

**Alternatively** (less invasive): A `DEBUG_MAKE(WidgetType, args...)` macro that wraps `std::make_shared` and stores location info in a side table keyed by `Widget*`.

#### 3.4.2 Location in Diagnostics

```cpp
void Widget::debugFillProperties(...) const {
    if (location_.file.empty() == false) {
        properties.push_back(std::make_unique<StringProperty>(
            "creation location", location_.file + ":" + std::to_string(location_.line)));
    }
}
```

### Phase 5: Structured Assertions with Tree Context
**Goal:** When an assertion fires, print the surrounding widget tree.

```cpp
#define CW_ASSERT(condition) \
    do { \
        if (!(condition)) { \
            std::cerr << "Assertion failed: " #condition "\n"; \
            std::cerr << "Widget tree at failure point:\n"; \
            WidgetInspector::instance().dumpWidgetTree(); \
            __builtin_debugtrap(); /* or std::abort() */ \
        } \
    } while(0)
```

More sophisticated: a `FlutterError`-like class that captures `DiagnosticsNode` context:

```cpp
class CampelloError : public std::exception {
public:
    std::string message;
    std::shared_ptr<DiagnosticsNode> context;
    
    std::string what() const override {
        return message + "\nContext:\n" + context->toStringDeep();
    }
};
```

---

## Part 4: Concrete File Plan

```
inc/campello_widgets/
├── diagnostics/
│   ├── diagnostic_property.hpp        # Property type hierarchy
│   ├── diagnostics_node.hpp           # Tree node + JSON serialization
│   ├── diagnosticable.hpp             # Mixin base class
│   ├── widget_inspector.hpp           # Inspector service singleton
│   └── widget_location.hpp            # Source location tracking
│
├── ui/
│   ├── debug_flags.hpp                # Expand with new flags
│   └── render_object.hpp              # Inherit Diagnosticable
│
└── widgets/
    ├── widget.hpp                     # Inherit Diagnosticable
    ├── element.hpp                    # Inherit Diagnosticable + visitChildren
    └── debug_overlay_panel.hpp        # In-app inspector UI (optional)

src/diagnostics/
├── diagnostic_property.cpp            # toString() implementations
├── diagnostics_node.cpp               # Serialization
├── widget_inspector.cpp               # Tree walking, hit-test mapping
└── widget_location.cpp                # (maybe header-only)

src/ui/
├── render_object.cpp                  # Add debugPaint() hook call
└── renderer.cpp                       # Add pointer overlay + debug panel paint

src/widgets/
└── element.cpp                        # visitChildren(), toDiagnosticsNode()
```

---

## Part 5: Immediate Next Steps (Priority Order)

### Step 1: `Diagnosticable` Mixin + `DiagnosticsNode` (1–2 days)
- Create `diagnostic_property.hpp` with `DiagnosticProperty<T>`, `FlagProperty`, `MessageProperty`
- Create `diagnostics_node.hpp` with tree structure and `toStringDeep()`
- Create `diagnosticable.hpp` with `debugFillProperties()`, `debugDescribeChildren()`, `toDiagnosticsNode()`
- Add `#ifdef NDEBUG` guards so the entire subsystem is compiled away in release builds

### Step 2: Integrate into Widget, Element, RenderObject (1 day)
- Make `Widget`, `Element`, `RenderObject` inherit from `Diagnosticable`
- Implement `visitChildren()` on `Element` (default walks `firstChildElement()`)
- Implement `debugFillProperties()` on `RenderObject` (constraints, size)
- Implement `debugFillProperties()` on base `Widget` and `Element`

### Step 3: `WidgetInspector` + Console Dump (1 day)
- Implement `WidgetInspector` singleton
- Implement `getWidgetTree()` by walking from root element
- Implement `dumpWidgetTree()` → `std::cout`
- Hook `Element::rebuild()` to record rebuild counts when `printRebuildsEnabled`

### Step 4: Debug Paint Expansion (1 day)
- Add `paintBaselinesEnabled` flag
- Implement `debugPaint()` virtual on `RenderObject`, called from `paint()` after `performPaint()`
- Implement baseline painting in `RenderText`
- Add pointer position tracking + `paintPointersEnabled` overlay in `Renderer`

### Step 5: Select Widget Mode + Hit-Test Mapping (1–2 days)
- Store `Element*` back-pointer in `RenderObjectElement`
- Implement `WidgetInspector::onSelectModeTap()` using hit-test
- Add visual highlight (magenta border) around selected widget's render bounds
- Log selected widget's `toDiagnosticsNode()->toString()` to console

### Step 6: In-App Debug Panel (Optional, 2–3 days)
- Build a `DebugOverlayPanel` widget with toggles and a scrolling tree view
- Hook to a global keyboard shortcut via platform layer
- Render as a top-level overlay in the render tree

---

## Part 6: Usage Examples (After Implementation)

### Debugging the "Blank Animate Tab" Problem

```cpp
// In your app startup or on a hotkey:
WidgetInspector::instance().dumpWidgetTree();
```

**Console output:**
```
[root] RenderView
 └─AnimationDemo
   └─SingleChildScrollView
     └─Column
       ├─TweenSection [dirty: false]
       │ └─AnimatedBuilder
       │   └─Container [width: 200, height: 50, color: Color(0xFF2196F3)]
       ├─ImplicitSection [dirty: false]
       │ └─AnimatedContainer [width: 150, height: 150, duration: 300ms]
       ├─OpacitySection [dirty: false]
       │ └─AnimatedOpacity [opacity: 1.0, duration: 500ms]
       └─StaggerSection [dirty: false]
         └─Column
           ├─Container [width: 200, height: 30, color: Color(0xFFE91E63)]
           ├─Container [width: 200, height: 30, color: Color(0xFF9C27B0)]
           └─Container [width: 200, height: 30, color: Color(0xFF673AB7)]
```

If all widgets are present but nothing shows, the problem is in the render tree:

```cpp
WidgetInspector::instance().dumpRenderObjectTree();
```

**Console output:**
```
RenderDecoratedBox [size: 375x812, constraints: BoxConstraints(375.0<=w<=375.0, 812.0<=h<=812.0)]
 └─RenderPadding [size: 375x812, padding: EdgeInsets(16,16,16,16)]
   └─RenderSingleChildScrollView [size: 343x780]
     └─RenderColumn [size: 343x0, mainAxisAlignment: start]   <-- SIZE IS ZERO!
       ├─RenderSizedBox [size: 343x50, width: 343, height: 50]
       ├─RenderSizedBox [size: 343x150, width: 343, height: 150]
       ...
```

Now you can see that `RenderColumn` has `size: 343x0` — immediately pointing to a layout bug in the Column or its constraints.

### Runtime Flag Toggling

```cpp
// In a keyboard handler or button callback:
DebugFlags::paintSizeEnabled = !DebugFlags::paintSizeEnabled;
DebugFlags::repaintRainbowEnabled = !DebugFlags::repaintRainbowEnabled;
// No recompile needed — takes effect next frame
```

### Detecting Rebuild Storms

```cpp
DebugFlags::printRebuildsEnabled = true;
```

**Console output:**
```
[REBUILD] TweenSection (2 builds in last second)
[REBUILD] AnimatedBuilder (60 builds in last second)   <-- every frame!
[REBUILD] CounterText (0 builds in last second)
```

### Select Widget Mode

```cpp
WidgetInspector::instance().setSelectModeEnabled(true);
// User clicks on screen...
// Console output:
// [SELECTED] AnimatedContainer at (120, 340)
//   size: Size(150, 150)
//   constraints: BoxConstraints(0<=w<=343, 0<=h<=780)
//   color: Color(0xFF4CAF50)
//   duration: 300ms
```

---

## Part 7: Long-Term Vision

### External Tool Integration (Future)
Once the JSON serialization protocol is stable, we could expose it via:
- A local WebSocket server in debug builds
- A companion macOS app that connects and renders the tree
- A VS Code extension that displays the tree in a side panel

### Performance Profiling (Future)
- Frame timeline with build/layout/paint phases
- Flame graph of which widgets consume build time
- Memory snapshot of the element/render object counts

### Golden Test Integration (Future)
- The visual fidelity tester already exists in `tests/visual_fidelity/`
- Connect the inspector to highlight which widgets differ between golden and current render

---

## Summary

Flutter's debugging power comes from **structured introspection at every layer**. campello_widgets has the same three-tree architecture, so it can support the same debugging model. The implementation breaks down into:

1. **Diagnostics Framework** — typed properties + tree nodes (Phase 1)
2. **Inspector Service** — tree snapshots + selection + hit-test mapping (Phase 2)
3. **Visual Overlays** — baselines, pointers, layer borders (Phase 3)
4. **Source Tracking** — file:line for widget construction (Phase 4)
5. **Structured Errors** — tree context in assertions (Phase 5)

This proposal prioritizes **console-based tree dumps** and **expanded debug paint flags** as the highest-impact, lowest-effort first steps. These alone would have made diagnosing the blank Animate tab trivial — you would have seen `RenderColumn size: 343x0` in seconds.
