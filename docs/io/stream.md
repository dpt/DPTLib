# [DPTLib](https://github.com/dpt/DPTLib) > io > stream

"stream" is a sub-library of DPTLib for creating data sources and transforms. It provides a unified interface for reading data from various sources and chaining together data processing operations to build flexible pipelines.

The core type `stream_t` is an interface which can be used to wrap or create sources of bytes. Stream implementations can accept other streams as input, allowing you to chain operations together similar to Unix pipes for data transformation and processing.

- It provides both byte-oriented and block-oriented access patterns.
- It supports chaining streams to create data processing pipelines.
- It includes example implementations for compression and decompression.
- Its byte access is efficiently implemented as a macro for performance.

This is a reimplementation of a technique that I was introduced to by Robin Watts and Paul Gardiner.

## Stream Architecture

The stream interface is primarily byte-oriented but supports block operations as well. Streams can be implemented to read from various sources (memory, files) or to transform data from other streams (compression, encoding).

Each stream implementation provides the standard stream interface, making them interchangeable and composable into data processing pipelines.

## Setup

#### Creating a stream:

See `stream_mem_create()` and its associated functions for a concrete example of how to construct a stream from memory.

Different stream types have their own creation functions, but all return a `stream_t` pointer that can be used with the standard stream interface.

## Reading from Streams

Use `stream_getc()` to fetch a single byte:

```C
int stream_getc(stream_t *stream);
```

It returns the next byte from the stream, or `EOF` if the stream has ended.

For block operations, use `stream_remaining_and_fill()` to fill the internal buffer:

```C
int stream_remaining_and_fill(stream_t *stream);
```

This attempts to fill the buffer `stream->buf`. The amount filled is not specified to allow for implementation flexibility. After calling this, you can access the buffered data directly.

## Chaining Streams

You can link streams together to create data pipelines that transform data in sequence. Transform streams accept another stream as input, allowing you to build complex processing chains.

For example, `stream_mtfcomp_create()` accepts a stream as input and provides compression, so you could chain:

```
File Stream → Compression Stream → Your Application
```

This creates a pipeline where data flows from a file, through compression, to your application code.

## Provided Stream Implementations

- `stream-stdio` - Creates a stream from a stdio `FILE` (read only).
- `stream-mem` - Creates a stream from a single block of memory (read only).
- `stream-packbits` - Performs PackBits RLE (de)compression.
- `stream-mtfcomp` - Provides "move to front" adaptive (de)compression.

The compression streams are primarily intended as examples of how to implement transform streams, though they are fully functional.

## Advanced Usage

The stream interface is flexible enough to support more complex patterns:

- **Forking streams** - Split data into multiple output streams
- **Concatenating streams** - Join multiple input streams into one
- **Multiplexing streams** - Combine or interleave multiple data sources

These patterns allow for sophisticated data processing architectures while maintaining the simple stream interface.

## Cleanup

Remember to properly clean up stream resources when finished. Each stream implementation provides its own cleanup method, typically named `stream_[type]_destroy()`.

## Limitations

- The interface is read-only - no support for writing to streams.
- No built-in error handling beyond `EOF` - stream implementations must handle their own error conditions.
- No seeking or random access - streams are sequential only.

Remember to never cross the streams.
