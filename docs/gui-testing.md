# GUI testing

This document describes the host-side GUI test harness, the invariants it
enforces, and the rules that keep the GUI layer portable.

## Running the tests

From the repository root:

```
make gui-test
```

This builds `tests/host/gui-test` with the host compiler (native 64-bit) under
AddressSanitizer and UndefinedBehaviorSanitizer, then runs it. The harness
compiles the **same** widget sources that go into the kernel, against a shim
layer in `tests/host/shim/`, so a passing host test exercises production code.

- `./tests/host/gui-test` prints TAP and a `# TOTAL_CHECKS=` line.
- `./tests/host/gui-test --count` runs the suites and prints only the number of
  assertions (used to check the 10,000 target quickly).
- The run exits non-zero if any check fails, or with code 2 if fewer than
  10,000 checks executed.

The full check suite (`make check`) also runs `check_gui_boundary.sh`, which
fails if a portable GUI source includes a kernel header.

## Portability boundary

The following files are portable and **must not** include `<core/...>`:

- All `gui/*.cpp` widget and layout sources: `widget`, `window`, `button`,
  `iconbutton`, `label`, `listview`, `inputbox`, `progressbar`, `checkbox`,
  `toggle`, `slider`, `scrollbar`, `scrollcontainer`, `combobox`, `table`,
  `tabs`, `radiobutton`, `spinbox`, `tooltip`, `textbox`, `imageview`,
  `contextmenu`, `layout`
- `gui/elements/*.cpp`, `gui/input/*.cpp`
- `gui/render/raster.cpp`, `gui/renderer/nina.cpp`, `gui/fonts/font.cpp`

Everything they need from the environment goes through:

- `include/gui/guiplat.h` — abort, critical section, clock, sleep.
- `include/gui/widgetHost.h` — event delivery to the owning process.
- `include/gui/render/raster.h` — `RasterSurface`, the pixel surface widgets
  draw into. `GraphicsDriver` derives from it in the kernel.

Kernel-only GUI glue (`desktop.cpp`, `taskbar.cpp`, `Hgui.cpp`, `bmp.cpp`,
`bootanim.cpp`, `messagebox.cpp`, `infodialog.cpp`, `terminalview.cpp`, and the
`*Kernel.cpp` adapters) may include kernel headers; they are listed in
`check_gui_boundary.sh`.

## Invariants checked by the suite

1. **Hit-testing**: `ContainsCoordinate(p)` agrees with the direct bounds test
   for every grid point and every child.
2. **Coordinates**: `ModelToScreen` of a descendant origin equals the sum of the
   ancestor offsets.
3. **Focus uniqueness**: at most one child of a composite is focused.
4. **Dirty convergence**: `MarkDirty` on a descendant marks every ancestor;
   a subsequent `Draw` on the root clears the whole visible tree.
5. **Cache/geometry**: the cache is non-null exactly when both dimensions are
   positive; `Resize` keeps `w`/`h` consistent.
6. **Event accounting**: a button press+release inside its bounds emits exactly
   one `ON_CLICK`; a release outside emits none.
7. **Raster exactness**: a filled rectangle touches exactly its pixels and
   nothing outside. Sizes are signed: a non-positive width or height draws
   nothing, and a rectangle wholly left of or above the surface stays empty
   instead of wrapping into a full-surface fill.
8. **Value clamping**: ranged widgets keep their value within bounds.
9. **Coordinate frame**: every mouse callback receives coordinates in the
   widget's *parent* frame, the same frame as `x`/`y`. Widgets that need local
   coordinates subtract `x`/`y`; `ContainsCoordinate` expects parent-frame
   coordinates.
10. **Pointer capture**: a widget that starts a drag sets `SetMouseCaptured(true)`
    and clears it on release. Composites forward the mouse-up (and moves) to a
    capturing descendant even when the cursor leaves their bounds, so drags
    cannot stick.
11. **Press/release pairing**: a release only activates a widget that received
    the matching press on it. Dragging from another widget and releasing over a
    checkbox, tab, or combo must not trigger it.
12. **Nested compositing**: a composite's `RedrawToCache()` composites its
    visible children, so a `Tabs` content area or `ScrollContainer` viewport
    renders inside its parent instead of being silently skipped.
13. **Visibility**: hidden children are skipped by hit-testing, hover, wheel,
    and double-click routing.
14. **Input dispatch**: drivers push decoded `InputEvent`s from interrupt
    context; `Desktop::ProcessInput()` drains them on the desktop task, which is
    the only place input mutates the widget tree.
15. **Deferred destruction**: a window closed without an owning handler is
    detached and queued through `WidgetHost::DestroyWidgetLater()`;
    `DrainDeferredWidgetDeletes()` frees it after every event handler has
    returned. Cached system dialogs opt out with `Window::SetDeleteOnClose(false)`
    and hide for reuse instead.
16. **Popup dismissal**: `ContextMenu` tracks the active menu;
    `DismissActiveAt()` hides it on any outside press and Escape hides it, so
    menus owned by any window dismiss consistently.
17. **Drag targeting**: `Widget::HitTest()` returns the deepest visible widget,
    so `OnDragStart/Move/End` reach nested widgets rather than only the
    top-level window. Current widgets implement dragging through pointer
    capture (`SetMouseCaptured(true)`) plus `OnMouseMove`, not the drag hooks.

`tests/host/suites/interaction_suite.cpp` holds the regression coverage for
these rules.

## Kernel arithmetic

The kernel is freestanding and does not link libgcc, so a 64-bit `/` or `%`
emits an undefined reference to `__divdi3`/`__udivdi3`. Geometry code that needs
a wide intermediate (slider fractions, scrollbar positions) must use
`GuiMulDivU32()` / `GuiMulDivSigned()` from `include/gui/mathutil.h`, which
divide by a 32-bit track size using shifts and compares only.

## Adding a widget (test-first)

1. Add `include/gui/<name>.h` and `gui/<name>.cpp` (kernel renderer).
2. Add a suite in `tests/host/suites/<name>_suite.cpp` and include it from
   `tests/host/main.cpp`.
3. Write the behavior matrix first: construction/bounds, hit-testing, focus,
   dirty propagation, event routing, destruction cleanup, and any value
   invariant. Then implement until the suite passes.
4. Keep the header free of `<core/...>` includes.

## Regenerating golden render tests

Golden tests hash a fixed widget configuration rendered into a `RasterSurface`.
When rendering changes intentionally, update the stored hashes in the same
commit that changes the renderer; never update them to make an unexpected
failure disappear.
