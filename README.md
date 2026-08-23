# DPTLib

version 0.5.0

[![Build status](https://github.com/dpt/DPTLib/actions/workflows/ci.yml/badge.svg)](https://github.com/dpt/DPTLib/actions)

© David Thomas, 2013-2024

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
- [`framebuf/colour.h`](https://github.com/dpt/DPTLib/blob/master/include/framebuf/colour.h) — colour definition and conversion
- [`framebuf/composite.h`](https://github.com/dpt/DPTLib/blob/master/include/framebuf/composite.h) — Porter-Duff image compositing {[docs](https://github.com/dpt/DPTLib/blob/master/docs/framebuf/composite.md)}
- [`framebuf/curve.h`](https://github.com/dpt/DPTLib/blob/master/include/framebuf/curve.h) — Bezier curve calculations
- [`framebuf/palettes.h`](https://github.com/dpt/DPTLib/blob/master/include/framebuf/palettes.h) — standard colour palettes
- [`framebuf/pixelfmt.h`](https://github.com/dpt/DPTLib/blob/master/include/framebuf/pixelfmt.h) — pixel formats
- [`framebuf/screen.h`](https://github.com/dpt/DPTLib/blob/master/include/framebuf/screen.h) — screen type
- [`framebuf/span.h`](https://github.com/dpt/DPTLib/blob/master/include/framebuf/span.h) — interface of plotting methods
  - [`framebuf/span-registry.h`](https://github.com/dpt/DPTLib/blob/master/include/framebuf/span-registry.h) — registry of plotting methods by pixel format
  - [`framebuf/span-bgrx8888.h`](https://github.com/dpt/DPTLib/blob/master/include/framebuf/span-bgrx8888.h) — plotting methods for BGRx8888
  - [`framebuf/span-rgba8888.h`](https://github.com/dpt/DPTLib/blob/master/include/framebuf/span-rgba8888.h) — plotting methods for RGBA8888
  - [`framebuf/span-rgbx8888.h`](https://github.com/dpt/DPTLib/blob/master/include/framebuf/span-rgbx8888.h) — plotting methods for RGBx8888
  - [`framebuf/span-xbgr8888.h`](https://github.com/dpt/DPTLib/blob/master/include/framebuf/span-xbgr8888.h) — plotting methods for XBGR8888

### Geometry

- [`geom/box.h`](https://github.com/dpt/DPTLib/blob/master/include/geom/box.h) — box type
- [`geom/packer.h`](https://github.com/dpt/DPTLib/blob/master/include/geom/packer.h) — box packing for layout {[docs](https://github.com/dpt/DPTLib/blob/master/docs/geom/packer.md)}
- [`geom/layout.h`](https://github.com/dpt/DPTLib/blob/master/include/geom/layout.h) — laying out elements using the packer
- [`geom/line.h`](https://github.com/dpt/DPTLib/blob/master/include/geom/line.h) — line type and clipping
- [`geom/point.h`](https://github.com/dpt/DPTLib/blob/master/include/geom/point.h) — point type

### I/O

- [`io/path.h`](https://github.com/dpt/DPTLib/blob/master/include/io/path.h) — filename path handling
- [`io/stream.h`](https://github.com/dpt/DPTLib/blob/master/include/io/stream.h) — stream system {[docs](https://github.com/dpt/DPTLib/blob/master/docs/io/stream.md)}
  - [`io/stream-stdio.h`](https://github.com/dpt/DPTLib/blob/master/include/io/stream-stdio.h) — C standard IO stream implementation
  - [`io/stream-mem.h`](https://github.com/dpt/DPTLib/blob/master/include/io/stream-mem.h) — memory block IO stream implementation
  - [`io/stream-packbits.h`](https://github.com/dpt/DPTLib/blob/master/include/io/stream-packbits.h) — PackBits compression - from [TIFF](http://en.wikipedia.org/wiki/Tagged_Image_File_Format)
  - [`io/stream-mtfcomp.h`](https://github.com/dpt/DPTLib/blob/master/include/io/stream-mtfcomp.h) — "move to front" adaptive compression stream - from the book [Small Memory Software, Chapter 4](http://www.smallmemory.com/4_CompressionChapter.pdf)

### Test

- [`test/txtscr.h`](https://github.com/dpt/DPTLib/blob/master/include/test/txtscr.h) — text format 'screen'

### Utilities

- [`utils/array.h`](https://github.com/dpt/DPTLib/blob/master/include/utils/array.h) — array utilities
- [`utils/barith.h`](https://github.com/dpt/DPTLib/blob/master/include/utils/barith.h) — binary arithmetic
- [`utils/bsearch.h`](https://github.com/dpt/DPTLib/blob/master/include/utils/bsearch.h) — binary searching arrays
- [`utils/bytesex.h`](https://github.com/dpt/DPTLib/blob/master/include/utils/bytesex.h) — reversing bytesex
- [`utils/fxp.h`](https://github.com/dpt/DPTLib/blob/master/include/utils/fxp.h) — fixed point helpers
- [`utils/maths.h`](https://github.com/dpt/DPTLib/blob/master/include/utils/maths.h) — maths utils
- [`utils/pack.h`](https://github.com/dpt/DPTLib/blob/master/include/utils/pack.h) — structure packing and unpacking helpers
- [`utils/primes.h`](https://github.com/dpt/DPTLib/blob/master/include/utils/primes.h) — cache of prime numbers

## Building

Use CMake, e.g.:

```
mkdir build
cd build
cmake ..
make -j4
```

## Testing

Enable `BUILD_TESTS`, e.g. using `ccmake` and build. Then invoke DPTLibTest like:

```
./DPTLibTest -resources <path to DPTLib>
```

It'll spew a load of test output. If successful you'll see:

```
++ Tests completed in 0.9909s: 18 of 18 tests passed.
```
