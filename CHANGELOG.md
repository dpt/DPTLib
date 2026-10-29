# Changelog

All notable changes to DPTLib are recorded here.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/).

## [Unreleased]

### Added

- `dither_options_t`'s `linear` now applies to the ordered methods, Knoll and
  Yliluoma too: ordered thresholds split pixels in proportion to linear
  light, Knoll carries its error and Yliluoma mixes its plans in linear
  light.

### Fixed

- `bmfont_draw` draws to rgbx8888 and rgba8888 screens (such as RISC OS 16M
  colour modes) instead of failing with `result_NOT_SUPPORTED`.

## [0.7.0] - 2026-10-29

### Added

- `framebuf/dither.h` gains ordered and further diffusion methods. Ordered:
  2x2, 4x4, 8x8 and 16x16 Bayer (`dither_BAYER_2` to `dither_BAYER_16`),
  64x64 blue noise, an 8x8 clustered-dot halftone
  (`pattern_clustered_threshold`), repeatable white-noise thresholds
  (`dither_RANDOM`), Knoll's pattern dither and Yliluoma's positional dither
  (algorithm 2). Diffusion: Sierra two-row and Sierra lite, Fan and
  Shiau-Fan (four and five cell), Riemersma along a Hilbert curve,
  Ostromoukhov's variable coefficients and Zhou-Fang threshold modulation.
  `dither_options_t` gains `linear` to diffuse in linear light.
- `framebuf/colourspace.h`: `colourspace_srgb_decode` and
  `colourspace_srgb_encode` return shared sRGB/linear-light lookup tables.
  Dithering and resizing now use them instead of building their own.
- `framebuf/bitmap.h`: `bitmap_convert_in_place` swaps red and blue between
  any pair of rgbx, rgba, bgrx and bgra; `bitmap_clone` and
  `bitmap_copy_pixels`; `bitmap_load_png_deep` loads a PNG and expands a
  paletted result to a deep format; `bitmap_flood_fill`, a scanline flood
  fill for 32bpp bitmaps that reports the bounds it changed;
  `bitmap_restore_outside_region` copies back from a second bitmap the
  pixels of a box that lie outside a region; `bitmap_has_transparency`
  reports whether any pixel is less than opaque; `bitmap_clear_transparent`
  clears colour-keyed pixels to transparent.
- `framebuf/screen.h`: `screen_draw_ellipse` and `screen_fill_ellipse` for
  axis-aligned ellipses; `screen_fill_checks` fills a box with a phased
  checkerboard; `screen_fill_linear_gradient` fills a box with a 256-step
  linear gradient between two points, alpha included.
- `framebuf/colour.h`: `colour_mix_rgba` blends alpha too and rounds to
  nearest, where `colour_mix` is opaque and truncates.
- `framebuf/span-bgra8888.h`: a bgra8888 span blending on straight alpha.
- `framebuf/pattern.h`: `pattern_bayer_threshold_n` gives the threshold
  from a Bayer matrix of any power of two size; `pattern_is_solid` reports
  whether a pattern paints a single colour.
- `framebuf/pixelfmt.h`: `pixelfmt_channel_step` gives the gap between
  adjacent levels of a direct-colour format's channel.
- `framebuf/tonemap.h`: `tonemap_rgb_in_sync` and `tonemap_has_alpha` report
  whether red, green and blue were last set together and whether alpha is
  present, so a tone map editor can choose between one combined curve and
  separate ones.
- `framebuf/bmfontfamily.h`: `bmfontfamily_split_label` splits a
  `Family Style` label; `bmfontfamily_label_path` uses it.
- `framebuf/palettes.h`: `palettes_load_hex` reads Lospec-style `.hex`
  palette files, one colour per line.
- DPT-Henry maps the em dash and lookalike codepoints (spaces, dashes,
  quotes, primes, Greek and Cyrillic lookalikes, Roman numerals, maths
  operators and fullwidth ASCII) onto its existing ASCII cells.
- `geom/box.h`: `box_subtract` splits one box minus another into up to four
  non-overlapping bands; `box_equal` compares two boxes.
- `geom/hull.h`: `hull_convex` computes the convex hull of a set of points
  as a closed polyline.
- `geom/region.h`: `region_from_flood` builds the region 4-connected to a
  seed through pixels a caller's predicate accepts, such as a magic wand
  selection; `region_union` and `region_subtract` combine two regions row
  by row; `region_translate` copies a region shifted and clipped to a box.
- `text/mdspans.h`: `mdspans_parse` turns minimal Markdown into plain text
  and styled spans, without overlaps.
- `text/utf8.h`: `utf8_rune_start` snaps a byte index to the start of its
  codepoint; `utf8_word_left` and `utf8_word_right` step over
  space-separated words.
- `io/path.h`: `path_parent`.
- `utils/alloc.h`: `alloc_t` allocator hooks (malloc, realloc and free),
  the stdlib `alloc_default` and `alloc_strdup`, moved from Wuss's
  `wuss_alloc_t`. `array_grow_alloc` grows an array through an `alloc_t`.
- `utils/rng.h`: `rng_bits` returns the low bits of an xorshift32 step;
  `rng_hash32` is a stateless keyed hash and `rng_noise1d` smooth 1-D value
  noise.

### Changed

- **Breaking:** `dither_method_t` is renumbered: nearest first, then the
  threshold methods, then the error-diffusion methods. `dither_BAYER` is
  renamed `dither_BAYER_8`.
- `pattern_from_colour` tries pairs nearest first and stops once no farther
  pair can win, so a 256-entry palette costs a sort and a few pairs rather
  than all 32,896. Only the first 256 entries are considered.
- `pattern_from_colour` mixes in linear light with perceptual channel
  weights, so stipples average as the eye sees them: greys stay neutral and
  1bpp gradients dither smoothly rather than jumping between solids.
- `bmfont_lookup` answers ASCII from a 128-entry table rather than a binary
  search.
- `BUILD_SDL_TESTS` is offered only when DPTLib is the top-level project.
- The default branch is renamed from `master` to `main`.

### Fixed

- Error diffusion limits the error each pixel carries to two of the
  palette's typical gaps, so chroma a palette cannot express no longer
  builds up until channels clip.

## [0.6.0] - 2026-10-23

Changes up to and including 0.6.0. Earlier releases were not recorded
separately.

### Removed

- **Breaking:** the `wuss` window manager, its interactive demo app and their
  resources and docs move to their own repository,
  [Wuss](https://github.com/dpt/Wuss), which fetches DPTLib with
  `FetchContent`. The `BUILD_APPS` and `WUSS_*` CMake options go with them.
  `result_BASE_WUSS` stays reserved. `BUILD_TESTS` now builds `DPTLibTest`
  only when DPTLib is the top-level project.

### Added

- `geom/stage.h`: lays out a document background (a page in a margin, with a
  stroked border and optional drop shadow, centred on a pasteboard) as a list
  of tagged boxes to fill. Ported from PrivateEye's `stage`, with `box_t` in
  place of `os_box` and `stageconfig_t::log2_pixel_x`/`log2_pixel_y` in place
  of the RISC OS pixel size query. Viewing uses it: a Stage option (on by
  default) draws the image on a page over a pasteboard, and its Background
  menu becomes Colours (Pasteboard, Paper, Shadow and Border).
- `framebuf/colourspace.h`: converts sRGB to and from CMYK, HSL, HSV and
  OKLab as unclamped doubles, so out-of-gamut values survive round trips;
  `colourspace_to_colour()` clamps and quantises. Also parses `#RRGGBB` and
  `#RGB` hex colours.
- `bmtext_layout_measured()` wraps text with a caller-supplied measure
  callback, so a line can mix faces; `bmtext_layout()` wraps it.
- `io/filetype.h` holds every allocated RISC OS file type (1026, from
  RISC OS Open's list) with its name and extensions, so aliases such as
  `.jpeg` resolve; `filetype_t` gains `name`. `tools/gen_filetypes.py`
  regenerates the tables. Markdown uses `&1D0` as a placeholder type.
- `colour_to_pixel()` handles bgr565 and bgrx5551.
- `USE_COVERAGE` CMake option and `tools/coverage.sh` for a Clang
  source-based coverage report in `build-coverage/`.
- Tests for `barith`, `bsearch`, `bitmap` (palette, clear, convert,
  `fill_pattern`, PNG save), `bmfont` at 8bpp, `bmtext`, `colour`, `curve`,
  `effects`, `layout`, `ntree`, `pack`, `packer`, `pickle`, `span`, `stream`,
  `tag-db` and many `wuss` areas (icons, writables, sliders, menus, menu
  descriptors, dialogues, Save As, colour menus, gridview, icon bar,
  furniture, backdrops, icon pools and toolbars).
- `text/utf8.h` — a UTF-8 decoder/encoder (`utf8_decode`, `utf8_encode`,
  `utf8_prev`). `bmfont` gains `.map` Unicode-cmap sidecars (one per bundled
  font) and `bmfont_lookup()`, and `bmfont_draw()`/`bmfont_measure()` decode
  UTF-8 and route through the cmap instead of a raw `char` index; `wuss`
  writable icons edit as UTF-8. `bmfont_create()` also now reads cell
  metrics from the first ink-free grid cell rather than the space glyph.
  Menu tick/submenu-arrow glyphs move to `U+2713`/`U+25B6` via the Symbols
  font's new cmap. New face: GrongyUI-Bold.
- `screen_draw_rounded_rect()` — rounded-rectangle outline and fill, used
  to draw menu shortcut keycaps.
- `screen_copy_bitmap_dithered()` gains a selectable dithering method
  (`screen_dither_t`), Orbing's own consumer of it.
- `tools/fmt/balance_braces.py` — braces every arm of an if/else chain when
  one arm is already braced (also run as part of the pre-commit format
  pass alongside `wrap_protos.py`/`realign_decls.py`).
- Aseprite extension to resize bmfont cells (`tools/aseprite/`).
- `framebuf/bmfontfamily.h` — font families with style selection.
  `bmfontfamily_scan()` reads a family directory without loading any PNG;
  `bmfontfamily_find()`, `_heavier()`, `_lighter()` and `_with_slant()`
  answer weight and slant queries; `bmfontfamily_label_path()` maps a
  `Family Style` label back to a path. `dirscan_walk_dirs()` lists
  subdirectories.
- `io/filetype.h` — RISC OS filetype to host extension mapping, plus
  `filetype_set()` to record the type as filesystem metadata on RISC OS (a
  no-op elsewhere). `io/dirlist.h` — `dirlist_scan()` gives a sorted,
  dotfile-filtered directory listing with each entry's filetype and
  directory flag. `path_leaf_valid()`/`path_is_full()` validate leafnames
  and full paths.
- rgb565 and rgbx5551 (64K and 32K colour) screens: spans, `pixelmap`, RLE
  bitmap blits, fill patterns, `bmfont` drawing, `screen_copy_bitmap()`
  from 16bpp sources to any screen and ordered dithering onto 16bpp
  screens. The SDL frontend's `--depth 15`/`16` and the Display task select
  them.
- `stack` size groups: items sharing a group get the largest size among
  them, so labels and values in different rows line up. Dialogue labels in
  the saturn, particles, display, orbing and config tasks are now
  size-grouped and measured from the font.
- `geom/stack` — a nested HBox/VBox box-stack layout solver with flex
  weights, cross-axis alignment, per-item min/max clamps and container
  padding/gap (`docs/windowing/stack-sketch.md`); `STACK_VBOX`/`STACK_HBOX`/
  `STACK_LEAF`/`STACK_SPACER` (plus `_EX` variants) compact a static
  `stack_item_t` table to one designated-init literal per row. The saturn
  Size dialogue is laid out through it.
- `pixelfmt_p8` — 8bpp paletted screen support across the
  framebuf/wuss/image-io stack: `span_p8`, screen blend/copy/fill-pattern/
  RLE-blit p8 branches, `bmfont` p8 glyph drawing, `bitmap_convert()`
  p8→bgrx8888/rgbx8888, PNG load/save keeping a palette-type PNG as
  `pixelfmt_p8` instead of expanding it, and `--depth 8` on the `wuss` SDL
  frontend. The palette task gains a second "Screen" window showing the
  physical screen bitmap's own palette; the p8 tail beyond the 16 UI colours
  is filled with the web-safe 216-colour cube.
- `screen_copy_bitmap_dithered()` — blits onto a paletted screen with
  per-pixel ordered dithering before the nearest-palette lookup, phased to
  the source bitmap's top-left so it stays put across redraws. A
  `screen_dither_t` picks the threshold map: none, 8x8 Bayer
  (`pattern_bayer_threshold()`) or 64x64 blue noise
  (`pattern_blue_noise_threshold()`, generated by
  `tools/gen_blue_noise.py`). The `wuss` image task gains a Dithering
  submenu to compare them.
- `screen_copy_bitmap()` now blits a paletted (p1/p2/p4/p8) source bitmap
  onto a screen of any format via a shared `src_fetch_rgba()` decode
  helper, reusing the existing paletted→deep `pixelmap_get()` table with no
  extra allocation or pass; a paletted source's tRNS-derived alpha is
  honoured.
- `screen_copy_rect()` now supports 1bpp and 2bpp screens (generalised from
  the existing 4bpp packed-pixel path), so window move/scroll blit fast
  paths work on low-bpp screens instead of falling back to a full repaint.
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
- `bitmap` now loads 1/2/4-bpp paletted PNGs: the sub-byte indices are
  unpacked with `png_set_packing()` before the existing palette-to-RGB
  expansion, and the `pngbitdepth != 8` rejection is restricted to
  non-palette colour types.
- `screen_draw_dashed_line()` now anchors its dash phase to the unclipped
  start point, so a dashed rule scrolled partly off-screen and back no
  longer smears its pattern.
- `bmfont_enumerate()` — non-recursive scan of a directory's `*.png` files,
  reporting each as a (name, path) pair (name is the leaf with `.png`
  stripped) to a caller-supplied callback; the callback may return
  `result_STOP_WALK` to stop early.
- `bitmap_fill_pattern()` (see _Changed_ for the rename from
  `bitmap_draw_pattern`) and `screen_fill_pattern()` now share a single
  `pattern_t { bits, fg, bg, flags, origin }` describing an 8x8 tile,
  built via `pattern_from_preset()` (the Bayer ramp and named tiles) or
  `pattern_from_mask()` (stencil) in the new `framebuf/pattern` module.
- `screen_fill_hline()` — fills one clipped horizontal run; the per-row
  primitive `screen_fill_rect()` and `screen_draw_circle()`'s scanline fill
  now loop over.
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
- `screen_fill_pattern()` — 8x8 two-colour tile fill primitive in
  `framebuf/screen` with eight built-in patterns (solid, grey50, stripes,
  diagonal, dots, grid, crosshatch), phase-locked to a caller-supplied origin
  so a scrolling fill stays put. The tile has only eight distinct rows, so each
  is expanded to a phase-shifted colour run once up front and the scanline
  loops index it with no per-pixel bit test; the 32bpp path memcpy's whole
  8-pixel runs.
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
- `tools/ttf2bmfont.py` — renders a TTF into the bmfont PNG format the
  loader expects. Three small paletted bitmap faces (`04b_03`, `04b_25`,
  `Nokia`) added under `resources/bmfonts/`.
- `bmtext_layout()` / `bmtext_draw()` take an optional
  `const bmfont_spacing_t *spacing` (NULL for none), threading letter/word
  spacing through the paragraph word-wrap and draw paths, not just the
  lower-level `bmfont_measure`/`bmfont_draw`.
- `bmfont_find_caret()` (pointer x to nearest character boundary),
  `bmfont_caret_x()` (index to caret x, in the letter-spacing gap) and
  `bmfont_draw_caret()` (I-beam spanning the ascent with a 2px overhang).
- `framebuf/bmfontcache` — `bmfontcache_t`, a refcounted acquire/release
  wrapper so callers loading the same font file share one `bmfont_t`.
  `wuss_t` owns one via `wuss_get_font_cache()`; the chars, text and
  minesweeper tasks acquire through it.
- `bmfont` supports glyphs up to 32px wide (64-bit extraction accumulators,
  a 4-byte glyph-row tier and `_4w_` draw variants for every pixel format).
  New fixture fonts: DPT-Digits-Bold-Lg (24px) and its regular weight,
  GrongyUI and SF-Embers.
- `pattern_from_colour()` — picks the palette pair whose Bayer mix best
  approximates an arbitrary colour, with a spread penalty so a near-solid
  entry beats a wide stipple.
- `pathf()` — a printf-like path builder that substitutes `%s` arguments
  then rewrites `/` to the host separator (dropping the final leaf's
  extension on RISC OS). Covered by a new `path` test.
- `bitmap_convert_into()` — a caller-owned-buffer sibling of
  `bitmap_convert()`; both share each `bmconv_*` pixel loop.
- `colour_get_rgb()`, mirroring `colour_get_alpha()`.
- `bitmap_save_png()` writes p1/p2/p4 paletted bitmaps, deriving bit depth
  and PLTE/tRNS size from the pixel format.
- `geom/inset.h` — `inset_t` (t,r,b,l) and an `INSET()` brace initialiser.
- `STACK_HUG` — a nested stack container sized to the sum of its
  children's extents (plus gaps/padding), resolved bottom-up before the
  top-down solve.
- `tools/extract_glyphs.py` — extracts unique 8-connected glyph shapes from
  a PNG into a grid image sorted by area.
- `geom/region.h` — sparse pixel sets held as per-row runs: box and even-odd
  polygon construction, `region_invert`, containment, row access and boundary
  outline.
- `DPTLIB_TMPPREFIX` places test scratch PNGs under `/tmp`.
- `effects_blend_extrapolate()` blends with alpha from -256 to 512, clamping
  per byte. `tonemap_plot()` draws a map's per-channel curves over a grid.
- `easing()` in `utils/` (linear, quad to quint, sine, expo, circ, back,
  elastic and bounce, each in in/out/in-out modes) and `colour_mix()`. The
  Oscillating task uses them for random backwards runs, an Easing menu and a
  Decay trail.
- `bitmap_resize()` with nearest, bilinear, box and Catmull-Rom filters and
  an optional linear-light blend, and a `bitresize` test. The `wuss`
  Resizing task offers Method, Scale, Linear light and Save PNG.

### Changed

- **Breaking:** `screen_DITHER_NONE` is renamed `screen_DITHER_NEAREST`, and
  None menu rows become Nearest.
- **Breaking:** `filetype_t.ext` is undotted (`"png"`, not `".png"`);
  `filetype_from_ext()` accepts either form. The generated tables shrink
  from about 35.6 KB to 10.5 KB and `filetype_from_riscos()` binary
  searches.
- `layout_place()` returns `result_PACKER_DIDNT_FIT` when an element does
  not fit.
- File-scope mutable static globals are grouped into one `static struct g`
  per source file.
- Ordered dithering on paletted screens sizes its bias from the real
  palette spacing (`pixelmap_t` gains `step`), so it hides banding.
- **Breaking:** `screen_copy_bitmap_dithered()`'s dithering method argument
  becomes a first-class selectable `screen_dither_t` rather than an
  implicit fixed method.
- **Breaking:** `array`'s duplicate squeeze/stretch variants are dropped
  (kept only the non-duplicated pair).
- `bytesex`'s scalar byte-swap functions are inlined.
- AGENTS.md (agent guidance) split out from CLAUDE.md and refreshed.
- `screen` dispatches per-depth workers through `log2bpp`-indexed function
  tables; single-use static tables are scoped to their functions.
- bmfont scripts move to `tools/bmfont/`.
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
- **Breaking:** the framebuf draw primitives split draw/fill in their names:
  `screen_draw_pixel` → `screen_set_pixel`, `screen_draw_rect` →
  `screen_fill_rect`, `screen_draw_square` → `screen_fill_square`. `screen_draw_rect`
  now names a one-pixel outline.
- `screen_set_pixel_8()` is renamed `screen_set_pixel_p8()`, matching the
  `_p1` / `_p2` / `_p4` naming of the other paletted set-pixel helpers.
- **Breaking:** `path_join_filename()`/`path_join_leafname()` are removed;
  every caller now uses `pathf()`.
- **Breaking:** `stack_item_t`'s `pad_l/pad_t/pad_r/pad_b` become one
  `inset_t pad`, and `STACK_VBOX_EX`/`STACK_HBOX_EX` take padding in
  (t,r,b,l) order.
- p8 blending and p4 span blending use the cached `pixelmap` table instead
  of a linear palette scan per pixel; `screen_fill_rect()` hoists its clip/
  colour lookup out of the row loop; the SDL present path reuses a
  persistent scratch bitmap instead of allocating per frame.
- `bmfont_draw()` selects its draw function from a
  `[format][rowbytes][transparent]` table.
- `pattern_t.origin` is documented as the absolute coordinate of the
  tile's top-left bit.
- `box_list_add()` merges the pair whose union adds least area when full,
  rather than folding into the last entry.
- `screen_copy_rect()` copies packed 1/2/4bpp rects bytewise.

### Fixed

- `ntree_append()` was declared but never defined.
- `stream`: PackBits fills never rewound their buffers, so output stopped
  after the first buffer's worth.
- `pickle_unpickle()` dropped a final line without a newline, and called
  the writer's stop method after a failed start.
- `layout_place()` ignored spacing in the remaining width, overflowed its
  stack past 32 elements and looped forever on a box too wide for any line.
- `tagdb_untagid()` decremented a count for a tag the id did not carry, and
  `tagdb_forget()` left counts unchanged.
- `pack`: array-mode signed halfwords were zero-extended.
- `bitmap_clear()` at 32bpp took the per-pixel path for any non-zero
  pixel.
- `bmtext` pushed a word ending exactly at the wrap width to the next line,
  and kept trailing whitespace at a break.
- Dither bias on 1bpp and 2bpp screens was about 15x too weak.
- `screen_copy_bitmap()` honours BGR(X|A)8888 source formats instead of
  assuming RGB byte order.
- `stack`: a container's fixed size is counted along its parent's axis
  (previously miscounted on the cross axis).
- `bitmap_save()` wrote packed p1/p2/p4 PNG rows with neighbouring pixels
  swapped.
- `PIXELFMT_MAKE_BGRX8888` swapped red and blue; `PIXELFMT_xxB565_SHIFT`
  overlapped green; `bitmap_clear()` did nothing for 16bpp bitmaps.
- `bmfontfamily_label_path()` clobbered its directory argument when given
  a `pathf()` result, leaving the Emscripten build with a black screen.
- `pathf()` on RISC OS stripped a '.' "extension" from output with no '/',
  truncating native paths.
- `bmfont_draw()` clipped full-width glyphs (M, W, ...) out of existence:
  `bmfont_advance_for()`'s advance (cell width + the new 1px letter
  spacing) was used directly as the drawable pixel width, pushing
  `clippedcharwidth` past `charwidth` and driving `right_skip` negative.
  Each glyph's draw now splits into the cell itself (clamped to
  `charwidth`) and any leftover advance, drawn from a zero-filled glyph
  buffer.
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
- A p8-loaded PNG converted to `bgrx8888` before reaching
  `screen_copy_bitmap`/`screen_copy_ninepatch` produced dull/wrong-hue
  composites, since that blit family assumes `rgba8888` byte order;
  switched to a matching `bmconv_p8_to_rgbx8888`.
- `path_join_leafname()` on RISC OS produced invalid pathnames like
  `Nokia/png` (`/` is a literal leafname character there, not a separator,
  and file type isn't part of the string at all).
- `mkstemps` (a GNU/BSD extension absent from the RISC OS GCCSDK libc) in
  `rle-test.c` broke the RISC OS build; replaced with a fixed leafname via
  `path_join_leafname`.
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
- `DPT-Digits-Bold` and `MS Sans Serif` bmfonts had an oversized space glyph
  width; reduced.
- RISC OS `dirscan_walk()`: subdirectories were reported as files (now uses
  OS_GBPB 10 and skips non-files), HostFS `/DS_Store`-style entries
  aborted the scan (skipped), SWI errors are logged, and a mid-scan failure
  returns what was found instead of `result_FILE_NOT_FOUND`.
- `bmfont_enumerate()` kept a pointer into `pathf()`'s shared buffer, so
  later entries (and the RISC OS multi-call scan) clobbered it; it copies
  the directory first.
- `pixelmap` matched each quantised bucket using its top edge, biasing
  deep-to-paletted conversion towards brighter entries; uses the bucket
  centre.
- `span_p4_blendconst` indexed the nibble-packed p4 pixelmap table as if
  byte-per-entry, reading up to 2x past its end.
- `bmfont_draw()` left `*end_pos` unset when fully clipped, garbling
  chained multi-colour runs on partial redraws.
- `bmfont_measure()` skipped bytes >= 0x80 that `bmfont_draw()` renders.
- `bmtext_layout` ignored newlines.
- The `bmconv_*` allocating wrappers leaked their buffer when
  `bitmap_init` failed.
- `INSET` was a compound literal, not a constant expression under GCCSDK.
- The `wuss` and `bmfont` enumerate tests hardcoded fixture counts.
- `rng.h` used `__inline__`, which MSVC rejects; it now uses `INLINE`.
- The pickle test deletes its temporary files.
- `bitmap_load_png` honours colour-key tRNS on RGB and grey PNGs.
