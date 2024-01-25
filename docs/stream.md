[DPTLib](https://github.com/dpt/DPTLib) > io > stream
=====================================================
"stream" is a sub-library of DPTLib for creating data sources and transforms.

The core type `stream_t` is an interface which can be used to wrap, or create, sources of bytes. The interface is primarily byte oriented but block operations are supported too. Byte access is efficiently implemented as a macro.

If a stream implementor also accepts a stream as input you can chain together the streams to build pipelines, similar to Unix pipes.

Implementations of TIFF-style PackBits RLE and "Move to Front" compression and decompression are provided but they're really only intended as examples.

This is a reimplementation of a technique that I was introduced to by Robin Watts and Paul Gardiner.

Creating a stream
-----------------
See `stream_mem_create()` and its associated functions for a concrete example of how to construct a stream.

Using a stream
--------------
Fetch a **byte** by using `stream_getc(stream)`. If the returned value is EOF then the stream has ended.

Fetch a **block** by first using `stream_remaining_and_fill(stream)`. This will attempt to fill the buffer `stream->buf` up. Note that we don't specify by how much the buffer will be filled to allow for flexibility.

Chaining streams
----------------
You can link streams together to create data pipelines that transform data in sequence. The second stream needs to be written to accept a stream as input, e.g. the example `stream_mtfcomp_create()` works like this.

Provided example streams
------------------------
* `stream-stdio`
  - Creates a stream from a stdio FILE (read only).
* `stream-mem`
  - Creates a stream from a single block of memory (read only).
* `stream-packbits`
  - Performs PackBits RLE (de)compression.
* `stream-mtfcomp`
  - Provides "move to front" adaptive (de)compression.

Taking it further
-----------------
Streams can be written to "fork" data into two separate pipes, to "cat" two pipes together, to "zip" pipes, etc.

Remember to never cross the streams.
