# Changelog

All notable changes to DPTLib are recorded here.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/).
This project does not yet publish versioned releases; entries are grouped under
_Unreleased_ until one is cut.

## [Unreleased]

### Added

- `wuss_ICON_TYPE_SLIDER` — horizontal/vertical slider icon: bevelled surround
  with a sunken groove inset by a fixed gap, drag/click value mapping that
  rounds to the nearest value rather than truncating, and a min > max
  reversed-fill mode. `wuss_icon_get_value()` / `wuss_icon_set_value()` read
  and write it.
- `geom/stack` — a nested HBox/VBox box-stack layout solver with flex
  weights, cross-axis alignment, per-item min/max clamps and container
  padding/gap (`docs/windowing/stack-sketch.md`); `STACK_VBOX`/`STACK_HBOX`/
  `STACK_LEAF`/`STACK_SPACER` (plus `_EX` variants) compact a static
  `stack_item_t` table to one designated-init literal per row. The saturn
  Size dialogue is laid out through it.
- `wuss/icon-spec.h` — `wuss_icon_spec_label()` / `_slider()` / `_action()`
  fill a `wuss_icon_spec_t` for the common cases, replacing manual
  field-by-field assignment (following the `create-from-desc.c` helper
  precedent).
- `wuss_STD_GAP` / `wuss_STD_INSET` / `wuss_STD_SLIDER_HEIGHT` /
  `wuss_STD_SECONDARY_BUTTON_HEIGHT` / `wuss_STD_PRIMARY_BUTTON_HEIGHT` —
  standard sibling-gap, edge-inset and icon-height constants in
  `wuss/icon.h`, generalised out of what was local to `saturn.c`.
- The saturn task gets a Colours menu (Foreground/Background via
  `wuss_colourmenu_t`) and a Size... submenu — a hover-opened dialogue
  (`wuss_menu_item_t::window`) with a slider for the sketch size and a
  second slider for the stars-loop iteration count, Apply/Cancel/
  Adjust-Apply/Adjust-Cancel following RISC OS convention.
- The `wuss` chars task draws a dotted baseline rule and a light-blue
  advance-width rule under each glyph (useful for eyeballing advance-width
  drift), trims contiguous leading/trailing blank grid rows, and gains a
  `wuss_STD_INSET` margin.
- `pixelfmt_p8` — 8bpp paletted screen support across the
  framebuf/wuss/image-io stack: `span_p8`, screen blend/copy/fill-pattern/
  RLE-blit p8 branches, `bmfont` p8 glyph drawing, `bitmap_convert()`
  p8→bgrx8888/rgbx8888, PNG load/save keeping a palette-type PNG as
  `pixelfmt_p8` instead of expanding it, and `--depth 8` on the `wuss` SDL
  frontend. The palette task gains a second "Screen" window showing the
  physical screen bitmap's own palette; the p8 tail beyond the 16 UI colours
  is filled with the web-safe 216-colour cube.
- `screen_copy_bitmap_dithered()` — blits onto a paletted screen with
  per-pixel 8x8 Bayer ordered dithering before the nearest-palette lookup,
  phased to screen coordinates so it stays put across redraws;
  `pattern_bayer_threshold()` exposes the underlying matrix. The `wuss`
  image task gains a Dithering menu toggle.
- `screen_copy_bitmap()` now blits a paletted (p1/p2/p4/p8) source bitmap
  onto a screen of any format via a shared `src_fetch_rgba()` decode
  helper, reusing the existing paletted→deep `pixelmap_get()` table with no
  extra allocation or pass; a paletted source's tRNS-derived alpha is
  honoured.
- `screen_copy_rect()` now supports 1bpp and 2bpp screens (generalised from
  the existing 4bpp packed-pixel path), so window move/scroll blit fast
  paths work on low-bpp screens instead of falling back to a full repaint.
- `wuss_EVENT_POINTER_ENTER` / `wuss_EVENT_POINTER_EXIT` — window-scoped
  events fired when the pointer crosses onto or off a window's whole
  on-screen footprint (content or furniture alike); exactly one ENTER is
  outstanding per window, always balanced by a later EXIT.
- `wuss_text_draw()` / `wuss_text_measure()` — public wrappers exposing the
  existing internal `bmfont_draw`/`bmfont_measure` pass-throughs, so a
  client task can draw/measure text in a wuss system font without including
  `framebuf/bmfont.h`.
- The minesweeper task gains a Grid Size submenu (24x24 down to 12x12,
  radio-ticked to the current size); board dimensions move from
  compile-time defines to runtime task fields.
- The image task gains a Background colour menu (`wuss_colourmenu_t`) for
  the fill behind the 9-patch border band; `wuss_MENU_ITEM_BORROWED_SUBMENU`
  marks a submenu `wuss_menu_destroy()` should not recurse into, and
  `wuss_NO_BACKDROP` replaces the `wuss_BACKDROP_COLOUR(wuss_NO_BACKGROUND)`
  idiom.
- `wuss_menu_create_from_desc()`'s `!`/`~` (tick/shade) prefixes now
  optionally pull a bool vararg at the point they're scanned instead of
  always applying, so a descriptor can encode live state (e.g. a toggle's
  current value) directly.
- `wuss_menu_tick_exclusive()` / `wuss_menu_tick_item()` /
  `wuss_menu_tick_set_live()` — data-level tick setters (for editing menu
  items before a (re)open) completing the existing live-chain setter family
  (`wuss_menu_tick_exclusive_live`, `wuss_menu_tick_item_live`, renamed from
  `wuss_menu_set_ticked`/`wuss_menu_set_item_ticked`).
- Shift-F1 on the RISC OS frontend now forces `wuss_INPUT_GARBAGE`
  (single-frame screen corruption), matching the existing SDL frontend and
  `frontend.h`'s documented F1/Shift-F1 intent.
- `path_leaf_strip_ext()` — host-aware `.ext`-stripping helper; on RISC OS a
  leafname carries no dotted extension at all (file type is separate
  metadata), so the three call sites hand-rolling `strcmp`-based stripping
  (namelist, bmfont enumerate, wuss icon registry) previously matched
  nothing there.
- `bmfont` glyphs are now drawn at the text baseline rather than the
  glyph-cell top-left: `bmfont_create()` derives ascent/descent at load
  time from the space glyph's grid rows, `bmfont_get_info()` gains ascent/
  descent out-params, and every caller doing manual top-left positioning is
  updated to add ascent.
- Six reserved `wuss_ICON_TYPE_*` constants (`DISPLAY`, `WRITABLE`,
  `NUMBER`, `STRING_SET`, `SLIDER`, `DRAGGABLE`) after `RULE`, validated
  from a spec but (bar SLIDER, now implemented) not yet drawn/hit-tested/
  routed.
- `bmfont_draw_relief()` — draws a string twice with a transparent
  background, a shadow pass in a given colour at `pos + offset` then the
  main pass at `pos`, factoring the hand-rolled two-call drop-shadow idiom
  into the library.
- `bmfont` opt-in monospaced mode: `bmfont_set_flags()` with
  `bmfont_FLAG_MONOSPACE` forces every glyph to advance by the font's
  widest advance width (`maxadw`, computed once at load). Glyph bitmaps are
  unchanged — proportional ink sits left-aligned in the wider fixed cell.
  `bmfont_measure()` and both `bmfont_draw` paths route their advance
  lookup through a new `bmfont_advance_for()` helper.
- `io/dirscan` — a `dirscan_walk()` flat-directory walk (RISC OS
  `OS_GBPB` / Win32 `FindFirstFile` / POSIX `dirent`) with a per-leafname
  callback, extracted from the copies `bmfont_enumerate()` and the wuss
  palette picker each carried.
- `wuss/menu.h` gains `wuss_menu_should_keep_open(ev)` (the "ADJUST keeps
  the chain open, SELECT has already freed it" predicate) and
  `wuss_menu_open_ticked(task, menu, ticks[], at, out)` (sets each row's
  TICKED bit from a bool array, then opens), factoring the open/close
  boilerplate tasks hand-rolled.
- `wuss_menu_set_item_ticked()` — updates a single menu row's tick in
  place on an ADJUST pick, for a menu tracking more than one independent
  tick, without reopening or repositioning the chain.
- `wuss_window_set_doc()` — change a window's virtual document extent after
  creation (used when cycling to a differently-sized image).
- `bitmap` now loads 1/2/4-bpp paletted PNGs: the sub-byte indices are
  unpacked with `png_set_packing()` before the existing palette-to-RGB
  expansion, and the `pngbitdepth != 8` rejection is restricted to
  non-palette colour types.
- `screen_draw_dashed_line()` now anchors its dash phase to the unclipped
  start point, so a dashed rule scrolled partly off-screen and back no
  longer smears its pattern.
- New `wuss` demo tasks: **Minesweeper** (12x12/20-mine board, flood-fill
  reveal, flags on Adjust, mine counter and elapsed timer HUD) and
  **greeble** (an edge-matched tile-placement greebling pattern built from
  205 baked artist stamps, prefab blocks scattered largest-first, a random
  4-colour palette per pattern, Adjust cycles the base palette).
- `wuss` text task gains a Sample submenu (pangrams plus the Lorem Ipsum
  default) and moves the font picker under a root MENU.
- The `wuss` SDL frontend tracks an integer device-pixel scale in
  `struct wuss_frontend`, opening at 2x; F2 / Shift-F2 step it, clamped to
  `[1, 4]`, and the window is sized from `scr_width/height * scale` so
  repeated halving can't drift it off the grid.
- A custom Emscripten shell page for the `wuss` demo (`--shell-file`),
  replacing the default boilerplate with a title, control-key legend and
  about blurb; `LINK_DEPENDS` on it so editing the shell relinks.
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
  rectangle `wuss_window_invalidate_visible()` covers, so a change touching the
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
- 1bpp and 2bpp paletted pixel formats (`pixelfmt_p1`, `pixelfmt_p2`),
  packed MSB-first (bit 7 / bits 7..6 are the leftmost pixel) to match PNG
  and the existing pattern bit order. `span_p1` / `span_p2` (`fill` +
  `blendconst`) are registered so `bitmap_init()` hands out a valid span;
  `screen_set_pixel` / `screen_blend_pixel` / `screen_fill_pattern` /
  `screen_copy_bitmap` gain p1 and p2 cases; `bitmap_convert()` grows
  `p1 -> bgrx8888` and `p2 -> bgrx8888` paths for the SDL frontend's
  per-frame display conversion. `pixelmap` already handled both generically.
- `bmfont_draw()` renders to a `pixelfmt_p1` or `pixelfmt_p2` screen. A
  single `bmfont_p1_plot_row` / `bmfont_p2_plot_row` does the per-pixel
  MSB-first bit write for both 1- and 2-byte glyph rows and both opaque
  and transparent backgrounds.
- `pixelfmt_paletted_nentries()` — palette entry count for any paletted
  format, replacing the `(scr->format == pixelfmt_p4) ? 16 : 0` hack at
  every `colour_to_pixel()` call site (which passed a broken `nentries=0`
  for any non-p4 paletted screen).
- The `wuss` SDL demo accepts `-d`/`--depth 1` and `--depth 2`, selecting a
  `pixelfmt_p1` (stride `(width + 7) / 8`) or `pixelfmt_p2` (stride
  `(width + 3) / 4`) framebuffer alongside the existing 4 and 32.
- The `wuss` curve task draws the active control points' convex hull
  (Andrew's monotone chain) as a closed light-grey polyline before the
  curve and blobs.
- The `wuss` gradient task cycles its ordered-dither matrix through 2x2,
  4x4 and 8x8 on SELECT / ADJUST clicks; each matrix cell maps to a fixed
  -8..+8 offset so a larger matrix gives a finer pattern, not a noisier
  one (4x4 output unchanged). The current size is drawn top-left.
- `tools/ttf2bmfont.py` — renders a TTF into the bmfont PNG format the
  loader expects. Three small paletted bitmap faces (`04b_03`, `04b_25`,
  `Nokia`) added under `resources/bmfonts/`.

### Changed

- **Breaking:** furniture window flags flip to opt-in:
  `wuss_WINDOW_NO_TITLEBAR`/`_OUTLINE`/`_CLOSE`/`_BACK`/`_TOGGLE_SIZE`/
  `_VSCROLL`/`_HSCROLL`/`_RESIZE` become `wuss_WINDOW_TITLEBAR`/`_OUTLINE`/
  `_CLOSE`/`_BACK`/`_TOGGLE_SIZE`/`_VSCROLL`/`_HSCROLL`/`_RESIZE` (set to
  show, not set to hide); `wuss_WINDOW_DEFAULT` is the OR of all eight, so a
  bare-`DEFAULT` caller needs no change, but any caller that combined
  `DEFAULT` with a single furniture flag (e.g. `wuss_WINDOW_NO_RESIZE_BLIT`
  alone) must now OR `DEFAULT` in explicitly to keep its chrome.
- **Breaking:** menu tick setters are renamed to separate data-level and
  live-chain variants: `wuss_menu_set_ticked` → `wuss_menu_tick_exclusive_live`,
  `wuss_menu_set_item_ticked` → `wuss_menu_tick_item_live`;
  `wuss_menu_open_ticked` and the new `wuss_menu_tick_set` take a bit-vector
  (`unsigned int`) instead of an `int` array.
- struct `wuss_icon`'s mutually-exclusive per-type fields (label border,
  pattern tile, bitmap image, radio group, menu-entry swatch) move into a
  per-type union, mirrored on `wuss_icon_spec_t`, then the spec is nested
  directly inside `struct wuss_icon` as `{ spec; state; }` —
  `icon->FIELD` becomes `icon->spec.FIELD`. `wuss_icon_get_window()` and the
  icon→window back-pointer are removed; the four mutators that needed it
  (delete, set_text, set_hidden, set_selected) now take the window as an
  explicit argument.
- `wuss_ICON_TYPE_BUTTON` is renamed `wuss_ICON_TYPE_ACTION` throughout the
  icon API, implementation, tests and docs.
- Fonts move out of loose `struct wuss` fields into a core-owned
  `libraries/wuss/font/` module (`struct wuss_fontset`,
  `wuss__fontset_init/_height`, the `wuss_get_font*` accessors, the
  centralised text renderer); `wuss`'s four loose font fields collapse to
  one embedded fontset.
- The `wuss` demo's SDL present path converts and uploads only dirty rows
  instead of the whole screen every frame (`wuss_frontend_present` now
  takes a dirty box); a new `wuss->touched` region list (distinct from
  `wuss->dirty`) tracks pixels written directly by `wuss__blit_pieces`/
  resize/toggle-size so the frontend still re-uploads blitted-but-not-
  repainted destinations.
- `wuss_window_move()` translates the cached furniture-layout rects by the
  move delta instead of invalidating and rebuilding the whole per-window
  layout cache on every drag tick; `wuss_window_move()` and the resize path
  both now early-return once the target box is found equal to the window's
  current one, skipping the blit/invalidate machinery for a no-op drag tick.
- Toggle-size now grows a window to fill the whole screen (capped per axis
  at the document extent, or uncapped when that's 0), nudging the top-left
  toward the origin by the minimum needed to fit, rather than pinning the
  top-left and only growing the bottom-right; restore returns the exact
  pre-toggle box.
- Adjust-clicking a scroll well now pages away from the click point
  (mirroring Select's page-towards), matching RISC OS's usual Select/Adjust
  reversal elsewhere in the furniture.
- `screen_set_pixel_8()` is renamed `screen_set_pixel_p8()`, matching the
  `_p1`/`_p2`/`_p4` naming of the other paletted set-pixel helpers.
- Advance widths encoded in font PNGs are now 1px shorter than the true pen
  advance (`bmfont_advance_for()` adds the missing pixel back in as letter
  spacing at every use site); the bundled bitmap fonts (`04b_03`, `04b_25`,
  the `DPT-*` faces, `Nokia`, `Symbols`, `Tiny`) are regenerated to match,
  and `tools/ttf2bmfont.py`'s ink-advance mode drops a spurious extra pixel
  it previously added.
- The bundled bitmap fonts are renamed with a provenance prefix (`DPT-` for
  the in-house faces, `ZX-` for GliderRider).
- `screen_copy_bitmap_i`'s has-alpha-channel check is hoisted into a general
  `pixelfmt_has_alpha()` macro alongside `pixelfmt_is_rle`/
  `pixelfmt_paletted_nentries`.
- **Breaking:** `wuss_window_invalidate_all()` is renamed
  `wuss_window_invalidate_visible()` -- "all" misread as the whole document
  when it only ever covered the visible content rectangle. Same behaviour;
  use `wuss_window_invalidate_extent()` for the full document.
- The `wuss` demo task launcher is split out of `apps/wuss/main.c` into
  `apps/wuss/tasks.c` behind `tasks.h`: the 19 `spawn_*` callbacks, every
  menu table and `task_handle_event` move over, and the shared file-scope
  context becomes `struct wuss_app_tasks`. `main.c` keeps the frame loop,
  `pixel_stress` and the chrome config.
- The `wuss` palette picker is driven off the `wuss_EVENT_PALETTE`
  broadcast rather than an app-supplied `on_select` callback:
  `palette_create()` no longer takes a palette/`on_select`/`user_data`,
  `palette_menu_select` installs the pick with `wuss_set_palette()`
  directly, and the swatch grid reads the live array back via a new
  `wuss_get_palette()` accessor each redraw. The frontend bitmap push
  moves into `main.c`'s `wuss_EVENT_PALETTE` case.
- The `wuss` palette-picker menu and the greeble palettes are now scanned
  from `resources/palettes/*.hex` at runtime instead of hardcoded PICO-8 /
  WIMP16 tables, so a new palette drops in without a rebuild.
- The `wuss` menu row highlight now inverts only the text column between
  the two gutters (padded by a measured space-width either side), not the
  full item bbox, so the tick and submenu-arrow gutters stay un-darkened.
  `WUSS_MENU_TICK_W` / `WUSS_MENU_ARROW_W` renamed
  `WUSS_MENU_GUTTER_LEFT` / `WUSS_MENU_GUTTER_RIGHT`;
  `WUSS_MENU_TEXT_PAD` dropped.
- The `wuss` demo quit key moves from `Q` (which collided with text-entry
  tasks) to `F4`; Escape still quits.
- `greeble_stamp` blits by same-slot run with `screen_fill_hline()` (1-4
  run fills per row) instead of 64 `screen_set_pixel()` calls per stamp.
- `GREEBLE_MAX_COLS` goes 32 → 48 to match `GREEBLE_MAX_ROWS`, and the
  greeble window opens at the full generator grid size (from
  `GREEBLE_MAX_COLS/ROWS` × tile pixel size) rather than a hardcoded
  160x320.
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
- `screen_set_pixel_8()` is renamed `screen_set_pixel_p8()`, matching the
  `_p1` / `_p2` / `_p4` naming of the other paletted set-pixel helpers.
- The greeble prefab tables (`greeble_prefab_cells` / `greeble_prefab`) are
  regenerated from the current sheet; `GREEBLE_NPREFAB` 103 -> 137.
- The `wuss` image task resizes the window frame on load (via
  `wuss_window_resize()` before `wuss_window_set_doc()`, which alone only
  moved the scroll extent) and insets an 8px solid border inside the
  ninepatch frame.

### Fixed

- `bmfont_draw()` clipped full-width glyphs (M, W, ...) out of existence:
  `bmfont_advance_for()`'s advance (cell width + the new 1px letter
  spacing) was used directly as the drawable pixel width, pushing
  `clippedcharwidth` past `charwidth` and driving `right_skip` negative.
  Each glyph's draw now splits into the cell itself (clamped to
  `charwidth`) and any leftover advance, drawn from a zero-filled glyph
  buffer.
- Several furniture task windows (swatches, text, palette) passed
  `wuss_WINDOW_NO_RESIZE_BLIT` alone as their window flags, zeroing every
  furniture bit under the new opt-in flag polarity instead of just opting
  out of blit-based resize; now OR'd with `wuss_WINDOW_DEFAULT`.
- The saturn Size dialogue's Cancel/Apply buttons now act on Select release
  (`wuss_MOUSE_UP`) instead of press, so a press dragged off the button no
  longer commits and an Adjust click leaves the dialogue open per RISC OS
  convention; the slider groove now derives from the surround box rather
  than the bevel box.
- Icon text was vertically centred using ascender+descender (`font_height`),
  pulling label/button/radio-option/menu-entry text visually high since
  most glyphs never touch the descender band; centred by `font_ascent`
  instead, factored into a shared `icon_text_baseline_y()` helper.
- The chars grid's document extent wasn't updated on a font switch, so the
  scroll range stayed sized to the original font — a larger font put grid
  cells out of scroll reach, a smaller one left dead scroll space.
- A borrowed-window menu item (`wuss_menu_item_t::window`) could open
  off-screen near the parent menu's right edge, since `wuss_window_move`
  deliberately doesn't clamp; extracted `wuss_window_create()`'s on-screen
  nudge into a shared helper and applied it after the anchor move.
- The RADIO/OPTION icon glyph square was always font-height, truncating a
  larger state-sprite blit and ignoring its real width for the label
  offset; now sized from the state bitmap when present. Its box now also
  grows to fit the larger of its two state bitmaps so a select-time
  invalidate covers the whole overhanging sprite, and the box is now
  cleared before redraw when the icon has no explicit background
  (previously a shrinking glyph swap left stale pixels).
- A submenu dragged back over its own parent's titlebar was closed
  mid-drag, because the IDLE-tick "pointer over parent titlebar" check
  didn't verify the parent was actually frontmost there; now requires
  `wuss__window_at` to resolve to the parent itself. Separately, a menu
  chain's submenu now only opens while the pointer is in the row's arrow
  gutter (not anywhere on the row) and closes on re-entry; and the parent
  menu's title bar being hovered now closes an open child chain (previously
  `wuss_mouse_move`'s early-return over furniture meant nothing heard about
  it).
- `screen_copy_bitmap()` read a paletted source bitmap (e.g. a PLTE-chunk
  PNG) as if it were 32bpp RGBA, running past the buffer end; first
  hardened to reject non-32bpp sources, then given real paletted-source
  support via a shared `src_fetch_rgba()` decode path.
- The ordered-dither bias table was keyed on raw palette entry count
  instead of the true per-channel quantisation level (4:4:4 for p1/p2/p4,
  5:6:5 for p8), collapsing the p8 Bayer bias to `{-1,0}` and making
  dithering invisible; and the dither cell was keyed on destination screen
  coordinates rather than sprite-local ones, so the pattern crawled across
  a moving sprite instead of staying fixed to it.
- The generalised packed-copy path (`screen_copy_rect_packed`, covering
  p1/p2/p4) used LSB-first within-byte indexing for every depth, but p1/p2
  are packed MSB-first — a non-byte-aligned move on a 2bpp screen picked
  the wrong pixel and shuffled the row.
- The screen palette wasn't repadded on a live palette change, only at
  startup: picking a palette entry while running p8 fed wuss's bare
  16-entry UI palette straight to `bitmap_set_palette()`, which reads 256
  entries for p8 — a heap-buffer-overflow. The pad-to-screen-size logic is
  now shared between startup and the live-change handler.
- A p8-loaded PNG converted to `bgrx8888` before reaching
  `screen_copy_bitmap`/`screen_copy_ninepatch` produced dull/wrong-hue
  composites, since that blit family assumes `rgba8888` byte order;
  switched to a matching `bmconv_p8_to_rgbx8888`.
- `wuss_window_invalidate(window, NULL)` built its whole-visible-rect
  placeholder already in screen space then ran it through the doc-space-
  to-screen translation a second time, double-applying scroll once a
  window was shrunk below its document and scrolled, sliding queued dirty
  rects off to one side; the placeholder is now built in doc space.
- A client-supplied `wuss_window_invalidate()` box was never clipped
  against the window's own furniture (only against occluding windows above
  it), so an edge-hugging client could dirty the titlebar/scrollbars/
  outline around its content; now intersected with the content box first.
- The ball task's edge-clamp was off-by-one on the right/bottom (content
  being a half-open box), poking the invalidation 1px into the furniture
  strip.
- A resize drag's scroll-reclamp fast-path blit read its source without
  checking `wuss->dirty[]` (unlike move/set-scroll), letting an earlier
  unpainted dirty region in the same frame get slid forward as settled
  content; a follow-up guarded the same path against
  `WUSS_MAX_INVALIDATE_PIECES` overflow (falling back to a full repaint
  instead of silently dropping survivor pieces), and a duplicate
  furniture-repaint call on the same reclamp path was removed.
- `wuss__blit_pieces` translated clean source pieces by the scroll delta
  and clipped only against occluding windows, never against the content
  box itself — a fast scroll (especially after a shrink) could blit and
  then invalidate screen area outside the window, painted with stale/
  garbage content on the next redraw; now intersected against the content
  box first.
- A scroll-well click's page-and-hold also armed a sausage drag, turning a
  held button into a live scroll; then, once guarded off by region, that
  guard also blocked a *direct* sausage hit from arming a drag at all —
  now gated on whether the click actually paged, not on which region it
  hit.
- Interactive resize-drag could be dragged past the screen's own
  width/height when the window's top-left was off-screen (negative x0/y0),
  since the clamp measured room left from the window's current position
  rather than the screen's total size.
- `path_join_leafname()` on RISC OS produced invalid pathnames like
  `Nokia/png` (`/` is a literal leafname character there, not a separator,
  and file type isn't part of the string at all).
- `mkstemps` (a GNU/BSD extension absent from the RISC OS GCCSDK libc) in
  `rle-test.c` broke the RISC OS build; replaced with a fixed leafname via
  `path_join_leafname`.
- Minesweeper repainted the whole HUD strip and every board cell on any
  click, even a single flag toggle; reveal now accumulates just the
  board-cell range its flood fill touched and redraw skips the HUD/
  off-dirty cells accordingly, cutting the flicker.
- curve/porter-duff/gradient task content (a type label, a checkerboard, a
  composited bitmap) was pinned to the window corner instead of tracking
  the document scroll offset, so it stayed visually fixed while the rest
  of the content scrolled underneath it.
- `wuss_window_move()`'s fast path no longer slides pixels still queued in
  `wuss->dirty[]` from an earlier invalidation this frame onto the
  window's new position: pending dirty regions are stripped from the
  clean blit-source set before the slide. Emscripten's browser event
  queue batches several `MOUSE_MOTION` events per frame, so edge-drags hit
  this every time; native SDL rarely batched enough to expose it.
- `wuss_mouse_move()` clamps the incoming point to the screen before the
  drag-move / resize / scrollbar paths, so a pointer report from outside
  the frame can no longer carry a window off the desktop.
- The greeble menu's file-scope struct is shared by every greeble window;
  its single row's tick now follows the window it opened over (synced from
  task state at open time, re-ticked in place on an ADJUST pick) instead
  of showing whichever window toggled last.
- A MENU press that lands on another window is no longer spent closing the
  open menu chain — the chain still closes but the press falls through to
  that window's task so it can open its own menu, matching RISC OS and the
  bare-backdrop path.
- `wuss__menu_spawn` measures the menu title as well as the widest item
  label and sizes the window to the wider of the two, so a title longer
  than every item (e.g. a one-item menu) is no longer clipped by the
  titlebar.
- `spawn_image` copies the ninepatch path into its own buffer before
  `image_create()`, which calls `path_join_filename()` again and clobbers
  the shared static buffer; `image_click` now swallows a failed reload
  (warn, keep the current image) instead of propagating `rc`.
- Dropped dead `< 0` range checks on unsigned `wuss_colour_t` /
  `screen_pattern_t` fields across `wuss` create and icon-from-spec
  (`-Wtype-limits`), removed a bogus `const` on `pattern_runs_t`
  parameters that the RISC OS cross-compiler rejected, and marked
  `bitmap` PNG-save locals `volatile` against `-Wclobbered`.
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
  no span-registry entry — or a span with no `fill` — instead of
  dereferencing the NULL `scr->span` / `scr->span->fill`; the guarding
  `assert` was compiled out under `NDEBUG` so this previously crashed.
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
- `spanregistry_get()`'s one-slot cache seeded `lastformat` to zero, which
  equals `pixelfmt_p1` (the first enum member), so the first p1 lookup
  short-circuited to a NULL span and tripped the `screen_fill_hline()`
  assert. Seeded with `pixelfmt_unknown` and the fast path guarded on a
  non-NULL span.
- The `NO_RESIZE_BLIT` branch of `wuss_window_resize()` invalidated the
  dirty region but never dropped the cached furniture layout, so
  `wuss__furniture_draw` kept painting titlebar/carve/outline rects at the
  previous frame's width. It now calls `wuss__chrome_invalidate_layout`,
  matching the blit branch's `wuss__chrome_repaint`.
- `bitmap_fill_pattern()` negated the `box_intersection()` result (which is
  non-zero when the intersection is *empty*), so any non-NULL `area` that
  overlapped the bitmap returned early and painted nothing — only
  `area == NULL` worked. Drop the negation to match `screen_fill_pattern()`.
- `bitmap__rle_decode_row()` ran a bare `for (;;)` that only stopped on an
  EOL opcode, so a truncated or corrupt RLE blob read opcodes past the
  allocation. It now takes an `end` limit (like every other row walker),
  stops at it, and has a reserved opcode bail to `end` (release builds
  previously did a bare `break`, desynchronising the caller's row cursor).
  `bitmap_decompress()` reads `data_len` from the blob header to form
  `end`.
- `bmfont_draw()` formed `gid = c - ' '` from a raw signed `char` and
  indexed `adw[]` and the glyph bitmaps with no bounds check, so a tab, a
  DEL or any byte `>= 0x80` produced a negative or oversized index and an
  out-of-bounds read/write. Both draw loops now filter `c < ' '` and
  `gid >= totalchars`, matching `bmfont_measure()`. The
  fully-vertically-clipped case (`clippedcharheight <= 0`) also gets a
  release-build bail rather than entering `drawfn` with a negative height
  and looping ~`INT_MAX` times.
- `wuss_destroy()`'s task sweep delivered `wuss_EVENT_QUIT` and freed each
  task node without first setting `wuss_TASK__REAPING`. A QUIT handler that
  closed its last autoclose window hit `wuss_window_close`'s self-destruct
  path, which delivered a second QUIT and freed the node — which the sweep
  then freed again. `wuss_TASK__REAPING` is now set before QUIT in the
  sweep, and `wuss_task_destroy()` is a no-op when the task is already
  being reaped.
- `bitmap_compress()`'s worst-case row estimate assumed one control byte
  per 128 pixels, but the encoder emits a 1-byte literal control for every
  isolated non-repeatable pixel. A legal y8 image of the form singleton,
  pair, singleton, pair, ... needs ~1.33*w bytes/row, overflowing the
  allocation and returning `result_BUFFER_OVERFLOW` on valid input. The
  budget is now `w * (bpp + 1)` plus the literal-long split control and an
  EOL.
- `bmconv_p4_to_bgrx8888()` ran `x < src->size.w / 8` and expanded 8 pixels
  per iteration, so the final `w % 8` pixels of each row were never written
  — the output had uninitialised garbage columns for any width not a
  multiple of 8 — and it ignored row padding in `src->rowbytes`. Switched
  to a per-pixel loop honouring both, matching the p1 and p2 converters.
- `wuss_window_set_hidden()` marked a window hidden but left
  `wuss->furniture.dragging`, `wuss->pressed_icon` and `wuss->hover_icon`
  pointing into it, so hiding a window mid-drag from an event handler meant
  the next `wuss_mouse_move()` still drove `wuss_window_move()` on an
  off-screen window every pointer report. It now clears the same pointer
  state `wuss_window_close()` does.
- `scroll_strip()` always reserved `size + WUSS_DIVIDER_PX` at the far end
  for the resize corner, but `scroll_strip_hit()` only reserves it when
  something owns that corner. With a horizontal scrollbar and `NO_VSCROLL`
  and `NO_RESIZE` the drawn strip stopped short while the hit box ran to
  the visible edge: the rightmost band showed stale pixels yet hit-tested
  as `HSCROLL_WELL` and started a scrollbar drag over empty chrome.
  `scroll_strip()` gets the same far-end rule.
- `wuss__furniture_hit_test()`'s `nearest_edge_region()` fallback claimed
  `wuss_FURNITURE_RESIZE` for a whole bottom or right carve band whenever a
  resize icon was present, and a scroll well for an edge with no scrollbar
  strip. A click on the bare carve band left of the corner icon then
  started a resize far from the visible handle. It now hands off to a
  scroll well only when that scrollbar's strip runs the full edge;
  every other edge pixel resolves to `TITLE` or `CONTENT`.
