/* pack.h -- structure packing and unpacking helpers */

/**
 * \file Pack (interface).
 *
 * Structure packing and unpacking routines.
 *
 * Inspired by printf, scanf and the Python 'struct' module these allow a
 * string composed of formatting character to specify how data should be
 * marshalled into memory.
 */

#ifndef UTILS_PACK_H
#define UTILS_PACK_H

#include <stdarg.h>
#include <stddef.h>

/**
 * Structure packing.
 *
 * The arguments are packed into 'buf' according to the format string 'fmt'
 * using little-endian byte order.
 *
 * The format string argument accepts the following format characters:
 *
 *  - 'c' to pack into 8 bits  (notional char)
 *  - 's' to pack into 16 bits (notional short)
 *  - 'i' to pack into 32 bits (notional int)
 *  - 'q' to pack into 64 bits (notional long long 'quad')
 *
 * Each format character may be preceded by a count.
 *
 *
 * Examples:
 *
 *     n = pack(outbuf, "ccc", 1, 2, 3); ("ccc" can also be written "3c")
 *     n = pack(outbuf, "2si", 0x2000, 12345, 1 << 31);
 *
 *
 * Using '*' instead of a count invokes array mode: the next argument is
 * used as an array length and the next after that as an array base pointer.
 *
 * Example:
 *
 *     n = pack(outbuf, "*s", 5, shortarray);
 *
 * Writes out five shorts from shortarray to outbuf.
 *
 *
 * \param outbuf Output buffer to receive packed values.
 * \param fmt    Format string specifying what to pack.
 *
 * \return Number of bytes used in output buffer, or zero if error.
 */
size_t pack(unsigned char *outbuf, const char *fmt, ...);

/**
 * Structure unpacking.
 *
 * The arguments are unpacked from 'buf' according to the format string
 * 'fmt' using little-endian byte order.
 *
 * \see pack for a description of the format string.
 *
 * Example:
 *
 *     n = unpack(inbuf, "c3i", &byte1, &byte2, &byte3, &flags);
 *
 * Retrieves three characters and an int from 'inbuf'.
 *
 *
 * Using '*' instead of a count invokes array mode: the next argument is
 * used as an array length and the next after that as an array base pointer.
 *
 * Example:
 *
 *     n = unpack(inbuf, "*s", 5, shortarray);
 *
 * Reads in five shorts from inbuf into shortarray.
 *
 *
 * Additionally, unpack can specify different source and destination sizes by
 * prefixing a formatting character with a source size qualifier:
 *
 *  - 'b' - byte
 *  - 'h' - half-word
 *  - 'w' - word
 *  - 'd' - double word
 *
 * (Note that these specifiers are all different than the formatting characters).
 *
 * With these qualifers, sign becomes important. You can write CSIQ for unsigned
 * arguments, or csiq for signed arguments.
 *
 * Example:
 *
 *     n = unpack(inbuf, "hQ", &quad);
 *
 * Unpacks a two-byte quantity into an unsigned long long.
 *
 * Of course, this works in array mode too.
 *
 *
 * unpack copes with different endian formats. Prefix the string with:
 *
 *  - '<' to unpack little endian data
 *  - '>' to unpack big endian data
 *
 * The default is [ought to be] platform dependent.
 *
 *
 * \param inbuf Input buffer of packed values.
 * \param fmt   Format string specifying what to unpack.
 *
 * \return Number of bytes used from input buffer, or zero if error.
 */
size_t unpack(const unsigned char *inbuf, const char *fmt, ...);

/**
 * Variant of unpack() which accepts a va_list.
 */
size_t vunpack(const unsigned char *buf, const char *fmt, va_list args);

#endif /* UTILS_PACK_H */
