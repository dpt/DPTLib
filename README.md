# DPTLib

version 0.6.0

[![Build status](https://github.com/dpt/DPTLib/actions/workflows/ci.yml/badge.svg)](https://github.com/dpt/DPTLib/actions)

© David Thomas, 2013-2026

## Overview

DPTLib is my platform independent C library. It contains a wide variety of functions, formed of various portable C code that I've written for [PrivateEye](https://github.com/dpt/PrivateEye), [MotionMasks](https://github.com/dpt/MotionMasks) and other projects. Please consider it a permanent work in progress.

## Modules

### Base

- [`base/debug.h`](https://github.com/dpt/DPTLib/blob/main/include/base/debug.h) — debugging and logging macros
- [`base/result.h`](https://github.com/dpt/DPTLib/blob/main/include/base/result.h) — generic function return values
- [`base/types.h`](https://github.com/dpt/DPTLib/blob/main/include/base/types.h) — fixed-width integer types
- [`base/utils.h`](https://github.com/dpt/DPTLib/blob/main/include/base/utils.h) — various utilities

### Databases

- [`databases/digest-db.h`](https://github.com/dpt/DPTLib/blob/main/include/databases/digest-db.h) — digest database
- [`databases/filename-db.h`](https://github.com/dpt/DPTLib/blob/main/include/databases/filename-db.h) — filename database
- [`databases/tag-db.h`](https://github.com/dpt/DPTLib/blob/main/include/databases/tag-db.h) — tag database {[docs](https://github.com/dpt/DPTLib/blob/main/docs/databases/tag-db.md)}
- [`databases/pickle.h`](https://github.com/dpt/DPTLib/blob/main/include/databases/pickle.h) — (de-)serialising associative arrays {[docs](https://github.com/dpt/DPTLib/blob/main/docs/databases/pickle.md)}
  - [`databases/pickle-reader-hash.h`](https://github.com/dpt/DPTLib/blob/main/include/databases/pickle-reader-hash.h) — glue methods to let pickle read from hashes
  - [`databases/pickle-writer-hash.h`](https://github.com/dpt/DPTLib/blob/main/include/databases/pickle-writer-hash.h) — glue methods to let pickle write to hashes

### Data Structures

- [`datastruct/atom.h`](https://github.com/dpt/DPTLib/blob/main/include/datastruct/atom.h) — indexed data block storage
- [`datastruct/bitarr.h`](https://github.com/dpt/DPTLib/blob/main/include/datastruct/bitarr.h) — arrays of bits
- [`datastruct/bitfifo.h`](https://github.com/dpt/DPTLib/blob/main/include/datastruct/bitfifo.h) — fifo which stores bits
- [`datastruct/bitvec.h`](https://github.com/dpt/DPTLib/blob/main/include/datastruct/bitvec.h) — flexible arrays of bits
- [`datastruct/cache.h`](https://github.com/dpt/DPTLib/blob/main/include/datastruct/cache.h) — generic single-block cache {[docs](https://github.com/dpt/DPTLib/blob/main/docs/datastruct/cache.md)}
- [`datastruct/hash.h`](https://github.com/dpt/DPTLib/blob/main/include/datastruct/hash.h) — associative arrays
- [`datastruct/hlist.h`](https://github.com/dpt/DPTLib/blob/main/include/datastruct/hlist.h) — "Hanson" linked list library - from the book [C Interfaces and Implementations](https://github.com/drh/cii/)
- [`datastruct/list.h`](https://github.com/dpt/DPTLib/blob/main/include/datastruct/list.h) — linked lists
- [`datastruct/ntree.h`](https://github.com/dpt/DPTLib/blob/main/include/datastruct/ntree.h) — n-ary trees
- [`datastruct/vector.h`](https://github.com/dpt/DPTLib/blob/main/include/datastruct/vector.h) — flexible arrays

### Frame Buffer

- [`framebuf/bitmap.h`](https://github.com/dpt/DPTLib/blob/main/include/framebuf/bitmap.h) — bitmap image type
  - [`framebuf/bitmap-set.h`](https://github.com/dpt/DPTLib/blob/main/include/framebuf/bitmap-set.h) — a set of bitmap images
- [`framebuf/bmfont.h`](https://github.com/dpt/DPTLib/blob/main/include/framebuf/bmfont.h) — a proportional bitmap font engine {[docs](https://github.com/dpt/DPTLib/blob/main/docs/framebuf/bmfont.md)}
  - [`framebuf/bmfontcache.h`](https://github.com/dpt/DPTLib/blob/main/include/framebuf/bmfontcache.h) — shared bitmap font loader
  - [`framebuf/bmfontfamily.h`](https://github.com/dpt/DPTLib/blob/main/include/framebuf/bmfontfamily.h) — bitmap font families and style selection
- [`framebuf/colour.h`](https://github.com/dpt/DPTLib/blob/main/include/framebuf/colour.h) — colour definition and conversion
- [`framebuf/composite.h`](https://github.com/dpt/DPTLib/blob/main/include/framebuf/composite.h) — Porter-Duff image compositing {[docs](https://github.com/dpt/DPTLib/blob/main/docs/framebuf/composite.md)}
- [`framebuf/curve.h`](https://github.com/dpt/DPTLib/blob/main/include/framebuf/curve.h) — Bezier curve calculations
- [`framebuf/dither.h`](https://github.com/dpt/DPTLib/blob/main/include/framebuf/dither.h) — error-diffusion dithering to a palette {[docs](https://github.com/dpt/DPTLib/blob/main/docs/framebuf/dither.md)}
- [`framebuf/effects.h`](https://github.com/dpt/DPTLib/blob/main/include/framebuf/effects.h) — bitmap effects {[docs](https://github.com/dpt/DPTLib/blob/main/docs/framebuf/effects.md)}
- [`framebuf/octree.h`](https://github.com/dpt/DPTLib/blob/main/include/framebuf/octree.h) — octree colour quantiser {[docs](https://github.com/dpt/DPTLib/blob/main/docs/framebuf/octree.md)}
- [`framebuf/palettes.h`](https://github.com/dpt/DPTLib/blob/main/include/framebuf/palettes.h) — standard colour palettes
- [`framebuf/pattern.h`](https://github.com/dpt/DPTLib/blob/main/include/framebuf/pattern.h) — 8x8 repeating fill pattern
- [`framebuf/pixelfmt.h`](https://github.com/dpt/DPTLib/blob/main/include/framebuf/pixelfmt.h) — pixel formats
- [`framebuf/pixelmap.h`](https://github.com/dpt/DPTLib/blob/main/include/framebuf/pixelmap.h) — cached bitmap-format conversion tables {[docs](https://github.com/dpt/DPTLib/blob/main/docs/framebuf/pixelmap.md)}
- [`framebuf/screen.h`](https://github.com/dpt/DPTLib/blob/main/include/framebuf/screen.h) — screen type
- [`framebuf/span.h`](https://github.com/dpt/DPTLib/blob/main/include/framebuf/span.h) — interface of plotting methods
  - [`framebuf/span-registry.h`](https://github.com/dpt/DPTLib/blob/main/include/framebuf/span-registry.h) — registry of plotting methods by pixel format
  - [`framebuf/span-p1.h`](https://github.com/dpt/DPTLib/blob/main/include/framebuf/span-p1.h) — plotting methods for 1bpp paletted
  - [`framebuf/span-p2.h`](https://github.com/dpt/DPTLib/blob/main/include/framebuf/span-p2.h) — plotting methods for 2bpp paletted
  - [`framebuf/span-p4.h`](https://github.com/dpt/DPTLib/blob/main/include/framebuf/span-p4.h) — plotting methods for 4bpp paletted
  - [`framebuf/span-p8.h`](https://github.com/dpt/DPTLib/blob/main/include/framebuf/span-p8.h) — plotting methods for 8bpp paletted
  - [`framebuf/span-rgb565.h`](https://github.com/dpt/DPTLib/blob/main/include/framebuf/span-rgb565.h) — plotting methods for RGB565
  - [`framebuf/span-rgbx5551.h`](https://github.com/dpt/DPTLib/blob/main/include/framebuf/span-rgbx5551.h) — plotting methods for RGBx5551
  - [`framebuf/span-bgrx8888.h`](https://github.com/dpt/DPTLib/blob/main/include/framebuf/span-bgrx8888.h) — plotting methods for BGRx8888
  - [`framebuf/span-rgba8888.h`](https://github.com/dpt/DPTLib/blob/main/include/framebuf/span-rgba8888.h) — plotting methods for RGBA8888
  - [`framebuf/span-rgbx8888.h`](https://github.com/dpt/DPTLib/blob/main/include/framebuf/span-rgbx8888.h) — plotting methods for RGBx8888
  - [`framebuf/span-xbgr8888.h`](https://github.com/dpt/DPTLib/blob/main/include/framebuf/span-xbgr8888.h) — plotting methods for XBGR8888
- [`framebuf/tonemap.h`](https://github.com/dpt/DPTLib/blob/main/include/framebuf/tonemap.h) — per-channel tone maps

### Geometry

- [`geom/box.h`](https://github.com/dpt/DPTLib/blob/main/include/geom/box.h) — box type
- [`geom/inset.h`](https://github.com/dpt/DPTLib/blob/main/include/geom/inset.h) — edge inset type
- [`geom/packer.h`](https://github.com/dpt/DPTLib/blob/main/include/geom/packer.h) — box packing for layout {[docs](https://github.com/dpt/DPTLib/blob/main/docs/geom/packer.md)}
- [`geom/layout.h`](https://github.com/dpt/DPTLib/blob/main/include/geom/layout.h) — laying out elements using the packer
- [`geom/line.h`](https://github.com/dpt/DPTLib/blob/main/include/geom/line.h) — line type and clipping
- [`geom/point.h`](https://github.com/dpt/DPTLib/blob/main/include/geom/point.h) — point type
- [`geom/region.h`](https://github.com/dpt/DPTLib/blob/main/include/geom/region.h) — sparse sets of pixels held as per-row runs: box and polygon fill, invert, outline
- [`geom/size.h`](https://github.com/dpt/DPTLib/blob/main/include/geom/size.h) — size type
- [`geom/stack.h`](https://github.com/dpt/DPTLib/blob/main/include/geom/stack.h) — box-stack layout solver
- [`geom/stage.h`](https://github.com/dpt/DPTLib/blob/main/include/geom/stage.h) — document background layout: pasteboard, border, margin and shadow {[docs](https://github.com/dpt/DPTLib/blob/main/docs/geom/stage.md)}

### I/O

- [`io/path.h`](https://github.com/dpt/DPTLib/blob/main/include/io/path.h) — filename path handling
- [`io/dirlist.h`](https://github.com/dpt/DPTLib/blob/main/include/io/dirlist.h) — sorted directory listing
- [`io/dirscan.h`](https://github.com/dpt/DPTLib/blob/main/include/io/dirscan.h) — platform-independent flat directory scan
- [`io/filetype.h`](https://github.com/dpt/DPTLib/blob/main/include/io/filetype.h) — file type identification and RISC OS filetype mapping
- [`io/namelist.h`](https://github.com/dpt/DPTLib/blob/main/include/io/namelist.h) — collect matching directory leafnames into a fixed table
- [`io/stream.h`](https://github.com/dpt/DPTLib/blob/main/include/io/stream.h) — stream system {[docs](https://github.com/dpt/DPTLib/blob/main/docs/io/stream.md)}
  - [`io/stream-stdio.h`](https://github.com/dpt/DPTLib/blob/main/include/io/stream-stdio.h) — C standard IO stream implementation
  - [`io/stream-mem.h`](https://github.com/dpt/DPTLib/blob/main/include/io/stream-mem.h) — memory block IO stream implementation
  - [`io/stream-packbits.h`](https://github.com/dpt/DPTLib/blob/main/include/io/stream-packbits.h) — PackBits compression - from [TIFF](http://en.wikipedia.org/wiki/Tagged_Image_File_Format)
  - [`io/stream-mtfcomp.h`](https://github.com/dpt/DPTLib/blob/main/include/io/stream-mtfcomp.h) — "move to front" adaptive compression stream - from the book [Small Memory Software, Chapter 4](http://www.smallmemory.com/4_CompressionChapter.pdf)

### Test

- [`test/txtscr.h`](https://github.com/dpt/DPTLib/blob/main/include/test/txtscr.h) — text format 'screen'

### Text

- [`text/txtfmt.h`](https://github.com/dpt/DPTLib/blob/main/include/text/txtfmt.h) — word-wrap a string to a character width (for monospaced text)
- [`text/bmtext.h`](https://github.com/dpt/DPTLib/blob/main/include/text/bmtext.h) — word-wrap and draw a paragraph to a pixel width in a `bmfont`
- [`text/utf8.h`](https://github.com/dpt/DPTLib/blob/main/include/text/utf8.h) — UTF-8 decoding and encoding

### Utilities

- [`utils/array.h`](https://github.com/dpt/DPTLib/blob/main/include/utils/array.h) — array utilities
- [`utils/barith.h`](https://github.com/dpt/DPTLib/blob/main/include/utils/barith.h) — binary arithmetic
- [`utils/bsearch.h`](https://github.com/dpt/DPTLib/blob/main/include/utils/bsearch.h) — binary searching arrays
- [`utils/bytesex.h`](https://github.com/dpt/DPTLib/blob/main/include/utils/bytesex.h) — reversing bytesex
- [`utils/fxp.h`](https://github.com/dpt/DPTLib/blob/main/include/utils/fxp.h) — fixed point helpers
- [`utils/maths.h`](https://github.com/dpt/DPTLib/blob/main/include/utils/maths.h) — maths utils
- [`utils/pack.h`](https://github.com/dpt/DPTLib/blob/main/include/utils/pack.h) — structure packing and unpacking helpers
- [`utils/primes.h`](https://github.com/dpt/DPTLib/blob/main/include/utils/primes.h) — cache of prime numbers
- [`utils/rng.h`](https://github.com/dpt/DPTLib/blob/main/include/utils/rng.h) — tiny deterministic pseudo-random number generators

### Windowing

The `wuss` window manager and its interactive demo, formerly part of DPTLib,
now live in their own [Wuss](https://github.com/dpt/Wuss) repository, which
builds on DPTLib.

## Building

Use CMake with Ninja or Make, e.g.:

```
cmake -B build -G Ninja -DBUILD_TESTS=YES
cmake --build build --target DPTLibTest
```

Requires libpng. Useful options:

- `BUILD_TESTS` — the core test binary
- `BUILD_SDL_TESTS` — interactive SDL tests
- `USE_ASAN` — AddressSanitizer and UBSan

RISC OS is built with a GCCSDK toolchain file; see [AGENTS.md](AGENTS.md).

## Testing

Run from the repository root:

```
./build/DPTLibTest -resources .
```

(Debug builds name the binary `DPTLibTest-debug`.) Name tests to run a subset, e.g. `atom bitvec`. If successful you'll see:

```
++ Tests completed in 0.9909s: N of N tests passed.
```
