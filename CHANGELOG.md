# Changelog

All notable changes to DPTLib are recorded here.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/).
This project does not yet publish versioned releases; entries are grouped under
_Unreleased_ until one is cut.

## [Unreleased]

### Added

- `wuss/menu.h` (new `WUSS_MENUS` option, implies `WUSS_ICONS`) — RISC OS-style
  pop-up menus. `wuss_menu_open()` shows a caller-owned, immutable
  `wuss_menu_t` (title plus an array of `wuss_menu_item_t`) as a borderless
  window, nudged to stay on screen and opened under the pointer; wuss owns
  layout, submenu chaining on hover and whole-chain dismissal. Per-item flags
  cover ticks, disabled rows, dashed separators and submenus. A leaf pick is
  delivered to the opening task as `wuss_EVENT_MENU_SELECT` — SELECT closes
  the chain, ADJUST keeps it open. `wuss_menu_close()` / `wuss_menu_is_open()`
  manage a chain by handle. An over-tall menu gets a real vertical scrollbar
  instead of being cropped.
- `wuss_menu_create_from_desc()` / `wuss_menu_destroy()` — build a heap
  `wuss_menu_t` tree from a compact descriptor string (PrivateEye's
  `menu_create_from_desc` syntax: `,` between items, leading `|` for a dashed
  separator, `{ ... }` submenus, `!` tick, `~` shade, `>` and `%s` varargs).
  The whole tree is one owned allocation graph freed by `wuss_menu_destroy()`.
- `wuss_menu_item_t::window` — a menu row may carry a caller-owned
  `wuss_window_t` instead of a `submenu` (the two are mutually exclusive).
  Hovering the row shows that window where a submenu would open, using the same
  anchor maths; leaving the row, a click outside, a leaf SELECT elsewhere or
  `wuss_destroy()` hide it again rather than close it, so the same handle is
  reused on the next hover. Create the window with `wuss_WINDOW_HIDDEN`.
- `wuss_window_set_hidden()` and the `wuss_WINDOW_HIDDEN` create flag — a hidden
  window keeps its z-order slot but is not drawn, not hit-tested and occludes
  nothing. `wuss_window_move()` still works on it (translate only, no blit) so
  it can be parked and re-shown in position. Revealing one fires a veto-able
  `wuss_EVENT_PRE_SHOW` then `wuss_EVENT_SHOW`; `wuss_window_set_hidden()`
  returns `result_t` accordingly.
- `wuss_set_palette()` — swap the system palette mid-session. Copies the new
  palette in, refreshes the cached nearest-black/white indices, broadcasts a
  new `wuss_EVENT_PALETTE` to every registered task so it can recache
  `wuss_nearest_colour()` selections, then invalidates the whole screen.
  Length must match `wuss_create()`'s; a now-out-of-range furniture/bevel/
  backdrop index is rejected with the palette left unchanged.
- `wuss_nearest_colour()` — the system-palette index closest to an RGB value by
  squared Euclidean distance, ties to the lower index.
- `wuss_alloc_t` and the `wuss_alloc` stdlib default — pluggable malloc/realloc/
  free hooks. `wuss_create()` takes a new `const wuss_alloc_t *` argument
  (NULL selects `wuss_alloc`); every heap block a `wuss_t` owns goes through
  the hooks.
- `wuss/icon.h` — work-area icons drawn inside a window's content area, in
  virtual document space so they scroll with the content. Wuss hit-tests
  interactive icons before the content task sees a click and delivers them as
  `wuss_EVENT_ICON`; labels and hidden or disabled icons fall through as
  `wuss_EVENT_MOUSE`. Icon types: `LABEL` (with `JUSTIFY_RIGHT` / `_CENTRE`
  flags), bevelled `BUTTON` (`DEFAULT` flag draws it as the default action
  button), `RADIO` and `OPTION` latching buttons (radios with a non-zero
  `group` are mutually exclusive), `FRAME` grouping box, `BITMAP` (a
  caller-owned image, hit-tested only with the `INTERACTIVE` flag), `PATTERN`
  swatch, `MENU_ENTRY` and inert `RULE` rows.
- `wuss_icon_get_selected()` / `wuss_icon_set_selected()` — query and set a
  radio or option icon's latched state; setting a grouped radio clears the
  others in its group. No task event — the programmatic path.
- `wuss_icon_create_array()` — creates a batch of icons from a spec array with
  all-or-nothing rollback: on the first failure any icons already created by
  the call are destroyed and no handles are written.
- `text/bmtext.h` — `bmtext_layout()` word-wraps a string to a pixel width in a
  `bmfont_t` (measuring each candidate line, so proportional fonts wrap
  correctly); `bmtext_draw()` draws the laid-out lines stacked. Layout is pure.
- `screen_draw_lines()` — connected polyline; a `screen_draw_line()` segment
  between each adjacent pair.
- `screen_draw_rect()` — one-pixel unfilled rectangle outline (falls back to a
  fill for a degenerate size). See _Changed_ for the fill-primitive renames.
- `screen_draw_dashed_line()` — Bresenham line with a dash-period counter.
- `screen_PATTERN_BAYER0` .. `screen_PATTERN_BAYER0 + 64` — 8x8 ordered dither,
  one fill pattern per coverage level 0 (empty) to 64 (solid), indexed as
  `screen_PATTERN_BAYER0 + level`. `BAYER32` == `GREY50`, `BAYER64` == `SOLID`.
- `define_wimp16_palette()` and the `palette_WIMP16_*` names — the RISC OS
  desktop 16-colour palette in native Wimp index order.
- `bitmap_set_palette()` — replace a bitmap's palette in place, reusing the
  existing palette buffer when it is large enough; NULL drops the palette.
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

- **Breaking:** `wuss_create()` takes a `const wuss_alloc_t *alloc` argument
  after `config`; pass NULL for the stdlib allocator.
- **Breaking:** `wuss_config_t::palette` is renamed `furniture`, and its type
  `wuss_palette_t` renamed `wuss_furniture_palette_t`. `wuss_colour_t` narrows
  from `int` to `unsigned char`.
- **Breaking:** the framebuf draw primitives split draw/fill in their names:
  `screen_draw_pixel` → `screen_set_pixel`, `screen_draw_rect` →
  `screen_fill_rect`, `screen_draw_square` → `screen_fill_square`. `screen_draw_rect`
  now names a one-pixel outline.
- **Breaking:** the window/desktop background is now a `wuss_backdrop_t`
  (`{ colour, pattern, pattern_bg }`) instead of a bare `wuss_colour_t`:
  `wuss_config_t::backdrop`, and the `bg` parameter of
  `wuss_window_create()`, `wuss_window_create_placed()` and
  `wuss_window_set_background()`. A non-`screen_PATTERN_SOLID` `pattern` fills
  with `screen_fill_pattern()` — the desktop phased to the screen origin, a
  window's content phased to its scroll origin so the pattern stays locked to
  the content. `wuss_BACKDROP_COLOUR(c)` and `wuss_BACKDROP_PATTERN(c, p, b)`
  build one; the flat-colour case is `wuss_BACKDROP_COLOUR(old_value)`.
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
- A window can no longer be resized larger than the screen.
- **Breaking:** event dispatch is reworked around a registered, opaque
  `wuss_task_t` that owns its windows and is the sole delivery target.
  - `wuss_task_start()` / `wuss_task_stop()` and the by-value task-delegate
    struct are gone. Register a task with
    `wuss_task_create(wuss, const wuss_task_desc_t *, wuss_task_t **)` —
    `wuss_task_desc_t` is `{ wuss_window_fn_t *handle; void *task_data;
    const char *name; }` — and tear it down with
    `wuss_task_destroy(wuss_task_t *)`, which closes the task's windows,
    fires one `wuss_EVENT_QUIT` and unregisters it.
  - `wuss_window_create()` and `wuss_window_create_placed()` no longer take a
    leading `wuss_t *` or a task-delegate pointer; their first argument is now
    the owning `wuss_task_t *`. The window inherits that task's handler.
  - `wuss_event_kind_t` is a single master enum
    (`REDRAW, MOUSE, ICON, SCROLL, OPEN, PRE_SHOW, SHOW, PRE_CLOSE, CLOSE,
    IDLE, QUIT, PALETTE, MENU_SELECT`). Handlers must handle unknown kinds
    (fall through / `default:`).
  - New veto-able pre-events. `wuss_window_set_hidden()` now returns
    `result_t`: revealing a hidden window fires `wuss_EVENT_PRE_SHOW` first
    and a non-OK return keeps it hidden and propagates. New
    `wuss_window_try_close()` fires `wuss_EVENT_PRE_CLOSE` (non-OK vetoes),
    then `wuss_EVENT_CLOSE`, then closes; the close icon routes through it.
    `wuss_window_close()` stays the forced, unvetoable teardown and fires no
    pre-events.
  - `wuss_set_palette()` and `wuss_idle()` now broadcast once per *registered
    task* (in registration order, `window == NULL`), not once per open
    window's task.
  - `wuss_menu_open()` takes a `wuss_task_t *` as its first argument. A picked
    leaf now delivers `wuss_EVENT_MENU_SELECT` (with `window == NULL`) to that
    task — `data.menu_select` carries `{ const struct wuss_menu *menu; int
    index; wuss_button_t button; }`. `wuss_menu_select_fn_t` is removed.

### Fixed

- Opening a menu from a task's mouse-down handler no longer picks the menu's
  first row on the matching mouse-up: that release is now swallowed.
- A menu chain is now closed before its `on_select` callback runs, so a
  callback that opens another menu no longer fights the one being torn down.
- Menu text is drawn in the nearest-black palette entry rather than assuming
  a fixed index, so menus stay legible under any system palette.
- The mouse wheel no longer scrolls a window on an axis it declared
  non-scrollable.
- A MENU-button click on window furniture now has no effect, instead of being
  routed to the content task.
- A window's scroll offset is re-clamped after a resize reveals content past
  the document extent.
- The scrollbar well keeps a 2px gap at each end.
- Dragging a scrollbar well with Select no longer raises the window; only a
  resize-icon grab restacks it.
- `wuss_window_move()` no longer repaints already-blitted pixels when a drag
  past an occluded corner slides one clean piece of the window onto ground
  another clean piece just vacated.
- Several scroll-redraw glitches fixed: stale pixels when scroll events arrive
  faster than redraws, content blitted over a mid-content occluder, blit
  sub-pieces clobbering each other's source region, and repaint sets not
  clipped to the visible area. The hovered icon is re-resolved after a scroll.
- A work-area button held on mouse-down is now released if the click opens a
  window that covers the button's owner, instead of staying stuck pressed.
- Resize-corner drag preserves where within the resize icon the mouse-down
  landed, so the window's corner no longer jumps to the raw pointer position
  on the first move.
- Removed signed-overflow and negative-shift undefined behaviour in the
  anti-aliased fixed-point line rasteriser, reachable with long or off-screen
  endpoints.
- `screen_draw_ninepatch()` clamps its corner cells so they no longer overlap
  and double-draw when the destination box is smaller than the source corners.
