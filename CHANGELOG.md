# Changelog

All notable changes to DPTLib are recorded here.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/).
This project does not yet publish versioned releases; entries are grouped under
_Unreleased_ until one is cut.

## [Unreleased]

### Added

- `wuss/icon.h` — work-area icons: static labels and clickable bevelled
  buttons drawn inside a window's content area. Icon boxes are in virtual
  document space, so they scroll with the content. Wuss hit-tests buttons
  before the content task sees a click and delivers them as `wuss_EVENT_ICON`;
  labels and hidden or disabled icons fall through as `wuss_EVENT_MOUSE`.
- `wuss_icon_create_array()` — creates a batch of icons from a spec array with
  all-or-nothing rollback: on the first failure any icons already created by
  the call are destroyed and no handles are written.
- `wuss_window_create_placed()` — creates a window from a content size instead
  of a box, letting Wuss pack it (furniture included) into the first free
  screen region. Successive auto-placed windows tile; placement cascades when
  no region fits. The slot is released on close and on the first
  `wuss_window_move()` / `wuss_window_resize()`. An overall screen margin is
  kept around all auto-placed windows.
- `screen_fill_pattern()` — 8x8 two-colour tile fill primitive in
  `framebuf/screen` with eight built-in patterns (solid, grey50, stripes,
  diagonal, dots, grid, crosshatch), phase-locked to a caller-supplied origin
  so a scrolling fill stays put. The tile has only eight distinct rows, so each
  is expanded to a phase-shifted colour run once up front and the scanline
  loops index it with no per-pixel bit test; the 32bpp path memcpy's whole
  8-pixel runs.
- `wuss_ICON_TYPE_PATTERN` — a non-interactive work-area icon whose bbox is
  filled with a `screen_fill_pattern()` pattern in fg/bg, aligned to document
  space. Clicks fall through as `wuss_EVENT_MOUSE`; disabled swatches fold fg
  into bg.
- `WUSS_FURNITURE` and `WUSS_ICONS` CMake options (both default ON) drop the
  furniture/*.c and icon/*.c files and `#ifdef`-guard every core call site,
  struct field and helper that references them. With `WUSS_FURNITURE` off every
  window is chromeless (content box == visible box); with `WUSS_ICONS` off the
  `wuss_icon_*` API is not compiled. Programmatic and wheel scrolling survive
  either off via the new core `scroll-step.c`.
- `screen_draw_ninepatch()` — draws a resizable "9-patch" frame from a source
  image that is a 3x3 grid of equal cells: corners at natural size, edges and
  centre tiled, clipped to the destination box and the screen clip.
- `screen_NINEPATCH_NO_CENTRE` flag for `screen_draw_ninepatch()` to draw only
  the border and leave the interior untouched.
- `packer_set_gutter()` — `packer_place_by()` now reserves a configurable
  gutter strip along the box's two inner edges so located boxes are never
  flush. Defaults to 0, leaving existing callers unchanged; the returned
  position is still the un-inflated box.
- `packer_release()` — inverse of `packer_place_*`, returns an area to the
  pool. Released areas are not coalesced.
- `POINT(x, y)` and `SIZE2D(w, h)` compound-literal macros in `geom/point.h`
  and `geom/size.h`.

### Changed

- **Breaking:** `wuss_button_t` values are now flags (`wuss_BUTTON_SELECT` 4,
  `wuss_BUTTON_MENU` 2, `wuss_BUTTON_ADJUST` 1, `wuss_BUTTON_NONE` 0) so
  chords such as Select+Adjust can be reported. Client code comparing a
  reported button for equality must now test with `&`.
- **Breaking:** `wuss_window_create()` takes a `min_doc` argument between
  `doc` and `window`, the minimum content extent a resize-drag or toggle-size
  will shrink to. It is clamped up to the built-in grab floor and down to
  `doc`. Pass `(0, 0)` for the built-in floor.
- Adjust-clicking a scroll arrow now steps against the direction the arrow
  points, so one arrow can be worked both ways without moving the pointer.
  Toggle-size stays Select-only.

### Fixed

- Dragging a scrollbar well with Select no longer raises the window; only a
  resize-icon grab restacks it.
- `wuss_window_move()` no longer repaints already-blitted pixels when a drag
  past an occluded corner slides one clean piece of the window onto ground
  another clean piece just vacated.
- A work-area button held on mouse-down is now released if the click opens a
  window that covers the button's owner, instead of staying stuck pressed.
- Resize-corner drag preserves where within the resize icon the mouse-down
  landed, so the window's corner no longer jumps to the raw pointer position
  on the first move.
- Removed signed-overflow and negative-shift undefined behaviour in the
  anti-aliased fixed-point line rasteriser, reachable with long or off-screen
  endpoints.
