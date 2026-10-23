# DPTLib

version 0.6.0

[![Build status](https://github.com/dpt/DPTLib/actions/workflows/ci.yml/badge.svg)](https://github.com/dpt/DPTLib/actions)

© David Thomas, 2013-2026

## Overview

DPTLib is my platform independent C library. It contains a wide variety of functions, formed of various portable C code that I've written for [PrivateEye](https://github.com/dpt/PrivateEye), [MotionMasks](https://github.com/dpt/MotionMasks) and other projects. Please consider it a permanent work in progress.

## Modules

### Base

- [`base/debug.h`](https://github.com/dpt/DPTLib/blob/master/include/base/debug.h) — debugging and logging macros
- [`base/result.h`](https://github.com/dpt/DPTLib/blob/master/include/base/result.h) — generic function return values
- [`base/types.h`](https://github.com/dpt/DPTLib/blob/master/include/base/types.h) — fixed-width integer types
- [`base/utils.h`](https://github.com/dpt/DPTLib/blob/master/include/base/utils.h) — various utilities

### Databases

- [`databases/digest-db.h`](https://github.com/dpt/DPTLib/blob/master/include/databases/digest-db.h) — digest database
- [`databases/filename-db.h`](https://github.com/dpt/DPTLib/blob/master/include/databases/filename-db.h) — filename database
- [`databases/tag-db.h`](https://github.com/dpt/DPTLib/blob/master/include/databases/tag-db.h) — tag database {[docs](https://github.com/dpt/DPTLib/blob/master/docs/databases/tag-db.md)}
- [`databases/pickle.h`](https://github.com/dpt/DPTLib/blob/master/include/databases/pickle.h) — (de-)serialising associative arrays {[docs](https://github.com/dpt/DPTLib/blob/master/docs/databases/pickle.md)}
  - [`databases/pickle-reader-hash.h`](https://github.com/dpt/DPTLib/blob/master/include/databases/pickle-reader-hash.h) — glue methods to let pickle read from hashes
  - [`databases/pickle-writer-hash.h`](https://github.com/dpt/DPTLib/blob/master/include/databases/pickle-writer-hash.h) — glue methods to let pickle write to hashes

### Data Structures

- [`datastruct/atom.h`](https://github.com/dpt/DPTLib/blob/master/include/datastruct/atom.h) — indexed data block storage
- [`datastruct/bitarr.h`](https://github.com/dpt/DPTLib/blob/master/include/datastruct/bitarr.h) — arrays of bits
- [`datastruct/bitfifo.h`](https://github.com/dpt/DPTLib/blob/master/include/datastruct/bitfifo.h) — fifo which stores bits
- [`datastruct/bitvec.h`](https://github.com/dpt/DPTLib/blob/master/include/datastruct/bitvec.h) — flexible arrays of bits
- [`datastruct/cache.h`](https://github.com/dpt/DPTLib/blob/master/include/datastruct/cache.h) — generic single-block cache {[docs](https://github.com/dpt/DPTLib/blob/master/docs/datastruct/cache.md)}
- [`datastruct/hash.h`](https://github.com/dpt/DPTLib/blob/master/include/datastruct/hash.h) — associative arrays
- [`datastruct/hlist.h`](https://github.com/dpt/DPTLib/blob/master/include/datastruct/hlist.h) — "Hanson" linked list library - from the book [C Interfaces and Implementations](https://github.com/drh/cii/)
- [`datastruct/list.h`](https://github.com/dpt/DPTLib/blob/master/include/datastruct/list.h) — linked lists
- [`datastruct/ntree.h`](https://github.com/dpt/DPTLib/blob/master/include/datastruct/ntree.h) — n-ary trees
- [`datastruct/vector.h`](https://github.com/dpt/DPTLib/blob/master/include/datastruct/vector.h) — flexible arrays

### Frame Buffer

- [`framebuf/bitmap.h`](https://github.com/dpt/DPTLib/blob/master/include/framebuf/bitmap.h) — bitmap image type
  - [`framebuf/bitmap-set.h`](https://github.com/dpt/DPTLib/blob/master/include/framebuf/bitmap-set.h) — a set of bitmap images
- [`framebuf/bmfont.h`](https://github.com/dpt/DPTLib/blob/master/include/framebuf/bmfont.h) — a proportional bitmap font engine {[docs](https://github.com/dpt/DPTLib/blob/master/docs/framebuf/bmfont.md)}
  - [`framebuf/bmfontcache.h`](https://github.com/dpt/DPTLib/blob/master/include/framebuf/bmfontcache.h) — shared bitmap font loader
  - [`framebuf/bmfontfamily.h`](https://github.com/dpt/DPTLib/blob/master/include/framebuf/bmfontfamily.h) — bitmap font families and style selection
- [`framebuf/colour.h`](https://github.com/dpt/DPTLib/blob/master/include/framebuf/colour.h) — colour definition and conversion
- [`framebuf/composite.h`](https://github.com/dpt/DPTLib/blob/master/include/framebuf/composite.h) — Porter-Duff image compositing {[docs](https://github.com/dpt/DPTLib/blob/master/docs/framebuf/composite.md)}
- [`framebuf/curve.h`](https://github.com/dpt/DPTLib/blob/master/include/framebuf/curve.h) — Bezier curve calculations
- [`framebuf/dither.h`](https://github.com/dpt/DPTLib/blob/master/include/framebuf/dither.h) — error-diffusion dithering to a palette {[docs](https://github.com/dpt/DPTLib/blob/master/docs/framebuf/dither.md)}
- [`framebuf/effects.h`](https://github.com/dpt/DPTLib/blob/master/include/framebuf/effects.h) — bitmap effects {[docs](https://github.com/dpt/DPTLib/blob/master/docs/framebuf/effects.md)}
- [`framebuf/octree.h`](https://github.com/dpt/DPTLib/blob/master/include/framebuf/octree.h) — octree colour quantiser {[docs](https://github.com/dpt/DPTLib/blob/master/docs/framebuf/octree.md)}
- [`framebuf/palettes.h`](https://github.com/dpt/DPTLib/blob/master/include/framebuf/palettes.h) — standard colour palettes
- [`framebuf/pattern.h`](https://github.com/dpt/DPTLib/blob/master/include/framebuf/pattern.h) — 8x8 repeating fill pattern
- [`framebuf/pixelfmt.h`](https://github.com/dpt/DPTLib/blob/master/include/framebuf/pixelfmt.h) — pixel formats
- [`framebuf/pixelmap.h`](https://github.com/dpt/DPTLib/blob/master/include/framebuf/pixelmap.h) — cached bitmap-format conversion tables {[docs](https://github.com/dpt/DPTLib/blob/master/docs/framebuf/pixelmap.md)}
- [`framebuf/screen.h`](https://github.com/dpt/DPTLib/blob/master/include/framebuf/screen.h) — screen type
- [`framebuf/span.h`](https://github.com/dpt/DPTLib/blob/master/include/framebuf/span.h) — interface of plotting methods
  - [`framebuf/span-registry.h`](https://github.com/dpt/DPTLib/blob/master/include/framebuf/span-registry.h) — registry of plotting methods by pixel format
  - [`framebuf/span-p1.h`](https://github.com/dpt/DPTLib/blob/master/include/framebuf/span-p1.h) — plotting methods for 1bpp paletted
  - [`framebuf/span-p2.h`](https://github.com/dpt/DPTLib/blob/master/include/framebuf/span-p2.h) — plotting methods for 2bpp paletted
  - [`framebuf/span-p4.h`](https://github.com/dpt/DPTLib/blob/master/include/framebuf/span-p4.h) — plotting methods for 4bpp paletted
  - [`framebuf/span-p8.h`](https://github.com/dpt/DPTLib/blob/master/include/framebuf/span-p8.h) — plotting methods for 8bpp paletted
  - [`framebuf/span-rgb565.h`](https://github.com/dpt/DPTLib/blob/master/include/framebuf/span-rgb565.h) — plotting methods for RGB565
  - [`framebuf/span-rgbx5551.h`](https://github.com/dpt/DPTLib/blob/master/include/framebuf/span-rgbx5551.h) — plotting methods for RGBx5551
  - [`framebuf/span-bgrx8888.h`](https://github.com/dpt/DPTLib/blob/master/include/framebuf/span-bgrx8888.h) — plotting methods for BGRx8888
  - [`framebuf/span-rgba8888.h`](https://github.com/dpt/DPTLib/blob/master/include/framebuf/span-rgba8888.h) — plotting methods for RGBA8888
  - [`framebuf/span-rgbx8888.h`](https://github.com/dpt/DPTLib/blob/master/include/framebuf/span-rgbx8888.h) — plotting methods for RGBx8888
  - [`framebuf/span-xbgr8888.h`](https://github.com/dpt/DPTLib/blob/master/include/framebuf/span-xbgr8888.h) — plotting methods for XBGR8888
- [`framebuf/tonemap.h`](https://github.com/dpt/DPTLib/blob/master/include/framebuf/tonemap.h) — per-channel tone maps

### Geometry

- [`geom/box.h`](https://github.com/dpt/DPTLib/blob/master/include/geom/box.h) — box type
- [`geom/inset.h`](https://github.com/dpt/DPTLib/blob/master/include/geom/inset.h) — edge inset type
- [`geom/packer.h`](https://github.com/dpt/DPTLib/blob/master/include/geom/packer.h) — box packing for layout {[docs](https://github.com/dpt/DPTLib/blob/master/docs/geom/packer.md)}
- [`geom/layout.h`](https://github.com/dpt/DPTLib/blob/master/include/geom/layout.h) — laying out elements using the packer
- [`geom/line.h`](https://github.com/dpt/DPTLib/blob/master/include/geom/line.h) — line type and clipping
- [`geom/point.h`](https://github.com/dpt/DPTLib/blob/master/include/geom/point.h) — point type
- [`geom/region.h`](https://github.com/dpt/DPTLib/blob/master/include/geom/region.h) — sparse sets of pixels held as per-row runs: box and polygon fill, invert, outline
- [`geom/size.h`](https://github.com/dpt/DPTLib/blob/master/include/geom/size.h) — size type
- [`geom/stack.h`](https://github.com/dpt/DPTLib/blob/master/include/geom/stack.h) — box-stack layout solver
- [`geom/stage.h`](https://github.com/dpt/DPTLib/blob/master/include/geom/stage.h) — document background layout: pasteboard, border, margin and shadow {[docs](https://github.com/dpt/DPTLib/blob/master/docs/geom/stage.md)}

### I/O

- [`io/path.h`](https://github.com/dpt/DPTLib/blob/master/include/io/path.h) — filename path handling
- [`io/dirlist.h`](https://github.com/dpt/DPTLib/blob/master/include/io/dirlist.h) — sorted directory listing
- [`io/dirscan.h`](https://github.com/dpt/DPTLib/blob/master/include/io/dirscan.h) — platform-independent flat directory scan
- [`io/filetype.h`](https://github.com/dpt/DPTLib/blob/master/include/io/filetype.h) — file type identification and RISC OS filetype mapping
- [`io/namelist.h`](https://github.com/dpt/DPTLib/blob/master/include/io/namelist.h) — collect matching directory leafnames into a fixed table
- [`io/stream.h`](https://github.com/dpt/DPTLib/blob/master/include/io/stream.h) — stream system {[docs](https://github.com/dpt/DPTLib/blob/master/docs/io/stream.md)}
  - [`io/stream-stdio.h`](https://github.com/dpt/DPTLib/blob/master/include/io/stream-stdio.h) — C standard IO stream implementation
  - [`io/stream-mem.h`](https://github.com/dpt/DPTLib/blob/master/include/io/stream-mem.h) — memory block IO stream implementation
  - [`io/stream-packbits.h`](https://github.com/dpt/DPTLib/blob/master/include/io/stream-packbits.h) — PackBits compression - from [TIFF](http://en.wikipedia.org/wiki/Tagged_Image_File_Format)
  - [`io/stream-mtfcomp.h`](https://github.com/dpt/DPTLib/blob/master/include/io/stream-mtfcomp.h) — "move to front" adaptive compression stream - from the book [Small Memory Software, Chapter 4](http://www.smallmemory.com/4_CompressionChapter.pdf)

### Test

- [`test/txtscr.h`](https://github.com/dpt/DPTLib/blob/master/include/test/txtscr.h) — text format 'screen'

### Text

- [`text/txtfmt.h`](https://github.com/dpt/DPTLib/blob/master/include/text/txtfmt.h) — word-wrap a string to a character width (for monospaced text)
- [`text/bmtext.h`](https://github.com/dpt/DPTLib/blob/master/include/text/bmtext.h) — word-wrap and draw a paragraph to a pixel width in a `bmfont`
- [`text/utf8.h`](https://github.com/dpt/DPTLib/blob/master/include/text/utf8.h) — UTF-8 decoding and encoding

### Utilities

- [`utils/array.h`](https://github.com/dpt/DPTLib/blob/master/include/utils/array.h) — array utilities
- [`utils/barith.h`](https://github.com/dpt/DPTLib/blob/master/include/utils/barith.h) — binary arithmetic
- [`utils/bsearch.h`](https://github.com/dpt/DPTLib/blob/master/include/utils/bsearch.h) — binary searching arrays
- [`utils/bytesex.h`](https://github.com/dpt/DPTLib/blob/master/include/utils/bytesex.h) — reversing bytesex
- [`utils/fxp.h`](https://github.com/dpt/DPTLib/blob/master/include/utils/fxp.h) — fixed point helpers
- [`utils/maths.h`](https://github.com/dpt/DPTLib/blob/master/include/utils/maths.h) — maths utils
- [`utils/pack.h`](https://github.com/dpt/DPTLib/blob/master/include/utils/pack.h) — structure packing and unpacking helpers
- [`utils/primes.h`](https://github.com/dpt/DPTLib/blob/master/include/utils/primes.h) — cache of prime numbers
- [`utils/rng.h`](https://github.com/dpt/DPTLib/blob/master/include/utils/rng.h) — tiny deterministic pseudo-random number generators

### Windowing

- [`wuss/wuss.h`](https://github.com/dpt/DPTLib/blob/master/include/wuss/wuss.h) — minimal window manager {[docs](https://github.com/dpt/DPTLib/blob/master/docs/windowing/wuss.md)}
  - [`wuss/window.h`](https://github.com/dpt/DPTLib/blob/master/include/wuss/window.h) — window creation, positioning, sizing and client delegation
  - [`wuss/task.h`](https://github.com/dpt/DPTLib/blob/master/include/wuss/task.h) — tasks: the clients of windows
  - [`wuss/message.h`](https://github.com/dpt/DPTLib/blob/master/include/wuss/message.h) — inter-task messaging
  - [`wuss/icon.h`](https://github.com/dpt/DPTLib/blob/master/include/wuss/icon.h) — work-area icons
  - [`wuss/icon-spec.h`](https://github.com/dpt/DPTLib/blob/master/include/wuss/icon-spec.h) — fill helpers for common icon shapes
  - [`wuss/iconbar.h`](https://github.com/dpt/DPTLib/blob/master/include/wuss/iconbar.h) — RISC OS-style icon bar
  - [`wuss/menu.h`](https://github.com/dpt/DPTLib/blob/master/include/wuss/menu.h) — pop-up menus
  - [`wuss/menu-desc.h`](https://github.com/dpt/DPTLib/blob/master/include/wuss/menu-desc.h) — build a menu tree from a descriptor string
  - [`wuss/gadget/colourset.h`](https://github.com/dpt/DPTLib/blob/master/include/wuss/gadget/colourset.h) — titled colour well with a colour menu
  - [`wuss/gadget/gridview.h`](https://github.com/dpt/DPTLib/blob/master/include/wuss/gadget/gridview.h) — glyph and label grid
  - [`wuss/gadget/stringset.h`](https://github.com/dpt/DPTLib/blob/master/include/wuss/gadget/stringset.h) — field with a pop-up menu of fixed strings
  - [`wuss/component/dialogue.h`](https://github.com/dpt/DPTLib/blob/master/include/wuss/component/dialogue.h) — interactive dialogue base class
  - [`wuss/component/saveas.h`](https://github.com/dpt/DPTLib/blob/master/include/wuss/component/saveas.h) — RISC OS-style drag-and-drop Save As dialogue
  - [`wuss/component/info.h`](https://github.com/dpt/DPTLib/blob/master/include/wuss/component/info.h) — label and value grid dialogue
  - [`wuss/component/proginfo.h`](https://github.com/dpt/DPTLib/blob/master/include/wuss/component/proginfo.h) — shared program information dialogue
  - [`wuss/component/colourmenu.h`](https://github.com/dpt/DPTLib/blob/master/include/wuss/component/colourmenu.h) — menu of the system palette colours
  - [`wuss/component/fontmenu.h`](https://github.com/dpt/DPTLib/blob/master/include/wuss/component/fontmenu.h) — menu of the available bitmap fonts

The interactive `wuss` demo (`apps/wuss/`) hosts several example tasks, including [Painting](https://github.com/dpt/DPTLib/blob/master/docs/windowing/painting.md). More notes are in [`docs/windowing/`](https://github.com/dpt/DPTLib/blob/master/docs/windowing).

## Building

Use CMake with Ninja or Make, e.g.:

```
cmake -B build -G Ninja -DBUILD_TESTS=YES
cmake --build build --target DPTLibTest
```

Requires libpng. Useful options:

- `BUILD_TESTS` — the core test binary
- `BUILD_SDL_TESTS` — interactive SDL tests
- `BUILD_APPS` — the interactive `wuss` app
- `USE_ASAN` — AddressSanitizer and UBSan
- `WUSS_FURNITURE`, `WUSS_ICONS`, `WUSS_MENUS`, `WUSS_GADGETS`, `WUSS_COMPONENTS` — wuss subsystems (all default ON)

RISC OS is built with a GCCSDK toolchain file. WebAssembly builds of the `wuss` demo use `emcmake`; see [AGENTS.md](AGENTS.md).

## Testing

Run from the repository root:

```
./build/DPTLibTest -resources .
```

(Debug builds name the binary `DPTLibTest-debug`.) Name tests to run a subset, e.g. `atom bitvec`. If successful you'll see:

```
++ Tests completed in 0.9909s: N of N tests passed.
```
