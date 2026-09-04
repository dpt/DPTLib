# Changelog

All notable changes to DPTLib are recorded here.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/).
This project does not yet publish versioned releases; entries are grouped under
_Unreleased_ until one is cut.

## [Unreleased]

### Added

- An Emscripten/WebAssembly build of the interactive `wuss` demo. The
  run-loop is extracted into `wuss_frame(void *)` over a `wuss_frame_ctx`,
  driven by `emscripten_set_main_loop_arg` in the browser while desktop and
  RISC OS still spin it directly. CMake's `EMSCRIPTEN` branch uses the
  bundled `-sUSE_LIBPNG=1` port instead of `find_package(PNG)`, links
  Emscripten's SDL3 port (`-sUSE_SDL=3`), and preloads `resources/` into
  `wuss.data` mounted at MEMFS `/resources`; the target emits `wuss.html`.
  A GitHub Actions Pages workflow builds and deploys the demo on pushes to
  `master` and `develop`.
- `bmfont_enumerate()` — non-recursive scan of a directory's `*.png` files,
  reporting each as a (name, path) pair (name is the leaf with `.png`
  stripped) to a caller-supplied callback; the callback may return
  `result_STOP_WALK` to stop early.
- `wuss/component/fontmenu.h` and `wuss/component/colourmenu.h` — shared
  task components (the RISC OS Toolbox analogue), gated on a new
  `WUSS_COMPONENTS` CMake option (implies `WUSS_MENUS`). `wuss_fontmenu_create()`
  builds a flat, name-sorted `wuss_menu_t` from the bitmap fonts in a
  directory (via `bmfont_enumerate()`); `wuss_fontmenu_selected()` recovers
  the picked font name from a `wuss_EVENT_MENU_SELECT`, and
  `wuss_fontmenu_set_ticked(fm, index)` mutates the component's own items to
  tick one row (index -1 clears every tick). `wuss_colourmenu_create()`
  builds a swatch-row menu, one row per system-palette entry labelled
  `#RRGGBB`, and resolves a pick back to the palette index. Both components
  route every allocation through the caller's `wuss_alloc_t` hooks (copied
  into the handle, not a borrowed `wuss_t *`, so they can safely outlive
  `wuss_destroy()`).
- `wuss_menu_item_t` / icon rows gain an optional colour chip
  (`wuss_MENU_ITEM_SWATCH` / `wuss_ICON_FLAGS_SWATCH` plus a `wuss_colour_t
  swatch` field), drawn in the left gutter where the tick sits; a chip wins
  over a selected tick.
- Font slot 2 (`WUSS_SYMBOL_FONT`) is now consulted to draw the menu
  selection tick and submenu arrow as glyphs (`WUSS_GLYPH_TICK` `'*'` /
  `WUSS_GLYPH_SUBMENU` `'>'`) instead of vector strokes, when a font is
  present in that slot.
- `wuss_create()`'s `fonts` array elements are now tagged with a
  `wuss_font_class_t` and a borrowed leafname (`wuss_font_desc_t`), letting
  a picker such as `wuss_fontmenu` and the system-decoration font (menu
  ticks/arrows) be identified by class rather than array position.
  `wuss_FONT_CLASS_SYSTEM` marks a chrome/decoration-only slot.
- `wuss_icon_plot()` — validates an icon spec exactly as `wuss_icon_create()`
  does and draws it once through the window manager's screen, retaining
  nothing; for static content a task can redraw from its own model without
  a live icon per element.
- `wuss_window_invalidate_extent()` — marks a window's whole virtual
  document extent dirty (`window->doc`), not just the visible content
  rectangle `wuss_window_invalidate_all()` covers, so a change touching the
  whole document repaints correctly at any scroll position.
- Clicking in a scrollbar well (not on the sausage) now pages the content
  one visible extent towards the click, keeping one `WUSS_SCROLL_STEP` of
  overlap, RISC OS style.
- `bitmap_fill_pattern()` (see _Changed_ for the rename from
  `bitmap_draw_pattern`) and `screen_fill_pattern()` now share a single
  `pattern_t { bits, fg, bg, flags, origin }` describing an 8x8 tile,
  built via `pattern_from_preset()` (the Bayer ramp and named tiles) or
  `pattern_from_mask()` (stencil) in the new `framebuf/pattern` module.
- `screen_fill_hline()` — fills one clipped horizontal run; the per-row
  primitive `screen_fill_rect()` and `screen_draw_circle()`'s scanline fill
  now loop over.
- Symbolic `wuss_colour_t` values (`wuss/wuss.h`). A raw `wuss_colour_t` is a
  palette index, `0..127`; `wuss_COLOUR_SYMBOLIC` (128) and up name roles the
  window manager resolves to a concrete index. `wuss_COLOUR_BLACK`,
  `_WHITE`, `_RED`, `_GREEN`, `_BLUE`, `_YELLOW`, `_CYAN`, `_MAGENTA`,
  `_GREY` pick the nearest system-palette entry to the named RGB;
  `wuss_COLOUR_TITLE_BG` / `_TITLE_FG` / `_BUTTON_HILIGHT` / `_BUTTON_SHADOW`
  / `_ACCENT_BG` / `_ACCENT_FG` / `_BACKDROP` echo the matching
  `wuss_config_t` field. Accepted anywhere a `wuss_colour_t` is taken —
  config furniture/bevel/accent/backdrop, `wuss_window_create()` /
  `wuss_window_set_background()` backgrounds, icon specs — and resolved once
  when the value is stored, so draw code and `wuss_nearest_colour()` are
  unaffected. Resolutions are recomputed on `wuss_set_palette()` and
  `wuss_set_backdrop()`.
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

- **Breaking:** `wuss_create()`'s `fonts` argument is now
  `const wuss_font_desc_t *` (handle, `wuss_font_class_t`, borrowed
  leafname) instead of a bare `bmfont_t *const *` array; up to
  `wuss_MAX_FONTS` (4) slots are stored, slot 0 remaining the system font
  used for titlebars and any icon that does not select another.
  `wuss_get_font_n(wuss, index)` reads a given slot; `wuss_get_font` stays
  slot 0. An icon's flags gain a two-bit font-select field
  (`wuss_ICON_FLAGS_FONT_MASK`, bits 9-10) with `wuss_ICON_FONT(n)` /
  `wuss_ICON_FONT_OF(f)` helpers, and titlebars are drawn in font slot 1
  (the bold weight) when one is supplied, else slot 0.
- **Breaking:** `wuss_fontmenu_create()` takes a fourth argument,
  `const wuss_alloc_t *alloc` (NULL selects `wuss_alloc`), routing every
  block the handle keeps through those hooks instead of libc
  malloc/calloc/strdup/free, matching `wuss_colourmenu` and `wuss_create`.
- **Breaking:** `bitmap_draw_pattern()` is renamed `bitmap_fill_pattern()`
  and now takes `const pattern_t *` and returns `result_t`.
  `screen_draw_bitmap()` is renamed `screen_copy_bitmap()` and
  `screen_draw_ninepatch()` renamed `screen_copy_ninepatch()` (its
  `screen_NINEPATCH_NO_CENTRE` flag is unchanged) — `copy` is now the verb
  for all pixel transfer, matching `screen_copy_rect()`. `screen_fill_pattern()`
  now takes `const pattern_t *` instead of a preset enum. Hard renames, no
  compatibility wrappers.
- `screen_copy_bitmap()`, `screen_copy_ninepatch()` and `screen_copy_rect()`
  now all return `result_t` (`result_OK` / `result_NOT_SUPPORTED`) instead
  of `void` / `int`; `screen_copy_rect()` also returns
  `result_NOT_SUPPORTED` when clipping leaves nothing to copy, so a caller
  must fall back to a full redraw in that case.
- Resizing a window (`wuss_window_resize()`) no longer clamps the result to
  the visible on-screen strip when the window's top-left is off-screen —
  the requested content size is applied verbatim and the window may
  overhang the screen edge, the same latitude toggle-size and drag already
  allow. `wuss_window_create()` keeps its own screen cap.
- Toggle-size now grows a window to fill the whole screen (capped per axis
  at the window's document extent, or uncapped when that extent is 0),
  nudging the top-left toward the origin by the minimum needed to fit; it
  previously pinned the top-left and only grew the bottom-right corner, so
  a window not already near the origin could never fill the screen and a
  zero document extent toggled to nothing. Restore returns the exact
  pre-toggle box, position included.
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
  - `wuss_task_set_autoclose(wuss_task_t *, int)` opts a task into
    self-destruct: once its last window closes it fires one `wuss_EVENT_QUIT`
    and unregisters, so it stops receiving `wuss_idle()` / `wuss_set_palette()`
    broadcasts. Such tasks should free `task_data` from `wuss_EVENT_QUIT`, not
    `wuss_EVENT_CLOSE`.
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

- `wuss_destroy()` now delivers `wuss_EVENT_QUIT` to each still-registered
  task before freeing it, instead of freeing the task block directly. Per
  the `task_data`-ownership contract a task's client-owned allocations are
  freed only from its `QUIT` handler, so a still-registered task (e.g. a
  menu-spawned demo window not yet closed) previously leaked `task_data`
  on whole-manager teardown.
- `wuss_task_destroy()` now closes a live menu chain it owns before freeing
  the task, matching `wuss_destroy()`'s whole-manager teardown; previously
  a later row pick or in-flight pick-flash completion could call into the
  freed task through a dangling chain-owner pointer.
- A click outside a menu (or another `wuss_menu_open()`/`wuss_menu_close()`)
  landing during a pick's highlight flash no longer silently drops the
  `wuss_EVENT_MENU_SELECT` the flash was standing in for.
- A borrowed window opened by hovering a menu row (`wuss_menu_item_t::window`)
  no longer sticks at the submenu anchor position when its
  `wuss_EVENT_PRE_SHOW` vetoes the reveal — its prior position is restored.
- A submenu now opens only while the pointer is in the row's arrow gutter,
  not anywhere on the row, and closes on any re-entry rather than staying
  open for every subsequent mouse move over that row.
- An ADJUST pick that keeps a menu chain open now re-ticks the picked row
  in place; previously the tick was only applied at `wuss_menu_open()` time
  so an ADJUST-kept-open menu's tick state went stale.
- A window's furniture (titlebar, scrollbars, outline) is now clipped to
  its unoccluded pieces when redrawing its own dirty region; previously
  only the content redraw was occlusion-clipped, so a partly-covered
  window repainted furniture pixels straight over the window on top.
- Bounds-checked `extract_advance_widths()` pixel reads in `bmfont` before
  they happen, rather than after: a font PNG whose width is not an exact
  multiple of the glyphs-per-row count could previously run the inner loop
  past the row's — or the buffer's — last valid word before any check
  caught it.
- `bmfont`'s grid-size detector now decodes the image and verifies each
  candidate cell height against the advance-width strip's pixel markers,
  rather than taking the first divisor of the image height at or above the
  grid width. The old heuristic could miss a valid cell height below the
  grid width and, when the grid width did not divide the image height
  cleanly, could latch onto a spurious large divisor and overflow the
  pixel buffer in `bmfont_draw()`.
- `bmfont` rejects a malformed font grid with `result_PARSE_ERROR` instead
  of asserting, when `extract_advance_widths()`'s pixel cursor would walk
  out of the decoded image (previously two `assert`s, compiled out under
  `NDEBUG`).
- `screen_fill_hline()` (and so `screen_fill_rect()`, which calls it per
  row) is a no-op in release builds for a `screen_t` whose pixel format has
  no span-registry entry, instead of dereferencing the NULL `scr->span` —
  the guarding `assert` was compiled out under `NDEBUG` so this previously
  crashed.
- `wuss_create()`'s font-slot arrays and loop bound in the interactive demo
  are now sized from one named, compile-time-checked constant
  (`WUSS_MAIN_NFONTS`, checked against `wuss_MAX_FONTS`) instead of three
  independent hardcoded literals that could silently drift apart.
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
