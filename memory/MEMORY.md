# campello_widgets — Project Memory

## Build & Test
- Build: `./build.sh darwin`
- Test: `./test.sh` — 339 tests pass; 2 pre-existing failures (campello_input not built)
- Build output: `build/darwin/`

## Code Patterns
- StatelessWidget: implement `build()` in `.cpp`; factory `create()` static methods are common
- StatefulWidget: State class defined in `.cpp` (not header); `createState()` returns `make_unique<XState>()`
- Template StatefulWidget (e.g., `DropdownButton<T>`): full State implementation in header
- InheritedWidget: `updateShouldNotify()` + static `of(BuildContext&)` pattern
- Overlay-based UI (menus, snackbars): `Overlay::insert(entry)` / `Overlay::remove(entry)` singleton
- Multi-child RenderObjects: extend `MultiChildRenderObjectWidget`, implement `insertRenderObjectChild` and `clearRenderObjectChildren`
- `WidgetRef` = `shared_ptr<const Widget>` — do NOT cast to non-const
- `Color` fields: use `.a` directly (no `.alpha()` method)
- `EdgeInsets::symmetric(vertical, horizontal)` — note param order
- `Expanded` requires constructor arg: `std::make_shared<Expanded>(child)` — no default ctor
- `BoxConstraints` has no `tightFor`/`copyWith` — use direct struct init: `{minW, maxW, minH, maxH}`

## Widgets Implemented (High Priority Batch)
- `Card` — StatelessWidget, DecoratedBox + elevation shadow
- `ListTile` — StatelessWidget, Row with leading/title/subtitle/trailing slots
- `FloatingActionButton` — StatelessWidget, circular decorated button
- `SnackBar` + `showSnackBar()` / `hideSnackBar()` — Overlay-based, auto-dismiss via AnimationController
- `PopupMenuButton` — StatefulWidget, Overlay popup with ModalBarrier dismissal
- `DropdownButton<T>` — template StatefulWidget (header-only), Overlay popup
- `DefaultTabController` + `TabScope` (InheritedWidget) + `TabBar` + `TabBarView` — tab navigation system
- `PageView` + `PageController` + `RenderPageView` — swipeable pages with snap physics

## Key File Paths
- Umbrella header: `inc/campello_widgets/campello_widgets.hpp`
- Widget headers: `inc/campello_widgets/widgets/`
- RenderObject headers: `inc/campello_widgets/ui/`
- Widget impls: `src/widgets/`
- RenderObject impls: `src/ui/`

## Bug Fixes & Platform Notes
- [Retina DPR coordinate fix](feedback_dpr_math.md) — flushDrawList seeds transform with DPR scale; text font_size is pre-scaled by render objects, don't re-scale in drawText
