/* text/utf8.h -- UTF-8 decoding and encoding */

/**
 * \file utf8.h
 *
 * Decodes and encodes UTF-8 one codepoint at a time. Malformed input never
 * stalls: each bad byte decodes as U+FFFD and consumes one byte.
 */

#ifndef TEXT_UTF8_H
#define TEXT_UTF8_H

#ifdef __cplusplus
extern "C"
{
#endif

/* ----------------------------------------------------------------------- */

/** U+FFFD REPLACEMENT CHARACTER, returned for malformed input. */
#define utf8_REPLACEMENT (0xFFFDul)

/* ----------------------------------------------------------------------- */

/**
 * Decode the codepoint at the start of a UTF-8 string.
 *
 * A stray continuation byte, an overlong form, a surrogate, a value above
 * U+10FFFF or a sequence cut short by \p len each decode as \ref
 * utf8_REPLACEMENT and consume one byte.
 *
 * \param[in]  s          String to decode.
 * \param[in]  len        Bytes available at \p s. Must be at least 1.
 * \param[out] codepoint  Decoded codepoint.
 *
 * \return Number of bytes consumed, 1..4.
 */
int utf8_decode(const char *s, int len, unsigned long *codepoint);

/**
 * Encode a codepoint as UTF-8.
 *
 * \param[in]  codepoint  Codepoint to encode.
 * \param[out] buf        Buffer of at least 4 bytes. Not terminated.
 *
 * \return Number of bytes written, 1..4, or 0 if \p codepoint is a surrogate
 *         or above U+10FFFF.
 */
int utf8_encode(unsigned long codepoint, char *buf);

/**
 * Find the start of the codepoint before \p index, stepping back as
 * utf8_decode() steps forward: over a whole well-formed sequence, or over
 * one malformed byte.
 *
 * \param[in]  s      UTF-8 string.
 * \param[in]  index  Byte index, greater than zero, on a codepoint boundary.
 *
 * \return Byte index of the previous codepoint.
 */
int utf8_prev(const char *s, int index);

/* ----------------------------------------------------------------------- */

#ifdef __cplusplus
}
#endif

#endif /* TEXT_UTF8_H */
