DPTLib
======

DPTLib is my C library.

It is a work in progress while I pull into it the various bits of portable code I have.

Things
------

### Base

 * base/debug.h -- debugging and logging macros
 * base/result.h -- generic function return values
 * base/suppress.h -- helpers to suppress compiler warnings
 * base/types.h -- fixed-width integer types

### Databases

 * databases/digest-db.h -- digest database
 * databases/filename-db.h -- filename database
 * databases/pickle-reader-hash.h -- glue methods to let pickle read from hashes
 * databases/pickle-writer-hash.h -- glue methods to let pickle write to hashes
 * databases/pickle.h -- (de-)serialising associative arrays
 * databases/tag-db.h -- tag database

### Data Structures

 * datastruct/atom.h -- indexed data block storage
 * datastruct/bitarr.h -- arrays of bits
 * datastruct/bitvec.h -- flexible arrays of bits
 * datastruct/hash.h -- associative arrays
 * datastruct/hlist.h -- "Hanson" linked list library (from the book [C Interfaces and Implementations](https://sites.google.com/site/cinterfacesimplementations/))
 * datastruct/list.h -- linked lists
 * datastruct/ntree.h -- n-ary trees
 * datastruct/vector.h -- flexible arrays

### Frame Buffer

 * framebuf/bitmap-set.h -- a set of bitmap images
 * framebuf/bitmap.h -- bitmap image type
 * framebuf/pixelfmt.h -- pixel formats
 * framebuf/screen.h -- screen type

### Geometry

 * geom/box.h -- box type
 * geom/packer.h -- box packing for layout
 * geom/layout.h -- laying out elements using the packer

### I/O

 * io/stream-mem.h -- memory block IO stream implementation
 * io/stream-stdio.h -- C standard IO stream implementation
 * io/stream.h -- stream system

### Test

 * test/txtscr.h -- text format 'screen'

### Utilities

 * utils/array.h -- array utilities
 * utils/barith.h -- binary arithmetic
 * utils/bsearch.h -- binary searching arrays
 * utils/bytesex.h -- reversing bytesex
 * utils/fxp.h -- fixed point helpers
 * utils/likely.h -- hints to compiler of probable execution path
 * utils/maths.h -- math utils
 * utils/minmax.h -- clamping numbers
 * utils/pack.h -- structure packing and unpacking helpers
 * utils/primes.h -- cache of prime numbers
