/* framebuf/bitmap/rle.h -- private RLE bitmap encode/decode shared internals */

#ifndef FRAMEBUF_BITMAP_RLE_H
#define FRAMEBUF_BITMAP_RLE_H

#include <stddef.h>
#include <stdint.h>

#include "base/result.h"

/* ----------------------------------------------------------------------- */

/**
 * Blob header prefixing a compressed bitmap's `base`. Two little-endian
 * `uint32_t`s, written and read byte-wise (see bitmap__rle_put32 /
 * bitmap__rle_get32) so the on-blob layout is host-endian independent.
 */
#define bitmap__RLE_HEADER_SIZE 8

/* ----------------------------------------------------------------------- */

/*
 * Row opcode stream. The opcode of the first byte of each command is its
 * count of leading zero bits (clz8), UTF-8-prefix style. Lengths are pixel
 * counts. A zero length field means 2^(field width). Long forms store
 * `length - shortmax - 1` so their range begins where the short form ends.
 *
 *   1LLLLLLL                  literal      : 1..128 px, then len px inline
 *   01LLLLLL <pixel>          repeat       : 1..64 copies of one inline px
 *   001LLLLL                  skip         : 1..32 transparent px  [alpha only]
 *   0001LLLL LLLLLLLL         literal long : 129..4224 px, then len px inline
 *   00001LLL LLLLLLLL <pixel> repeat long  : 65..2112 copies
 *   000001LL LLLLLLLL         skip long    : 33..1056 px           [alpha only]
 *   00000001                  EOL
 *   00000000                  reserved (decoder rejects)
 */

#define bitmap__RLE_LITERAL_ID       0
#define bitmap__RLE_REPEAT_ID        1
#define bitmap__RLE_SKIP_ID          2
#define bitmap__RLE_LITERAL_LONG_ID  3
#define bitmap__RLE_REPEAT_LONG_ID   4
#define bitmap__RLE_SKIP_LONG_ID     5
/* id 6 reserved */
#define bitmap__RLE_EOL_ID           7

#define bitmap__RLE_LITERAL_LEN_BITS       7
#define bitmap__RLE_REPEAT_LEN_BITS        6
#define bitmap__RLE_SKIP_LEN_BITS          5
#define bitmap__RLE_LITERAL_LONG_LEN_BITS 12
#define bitmap__RLE_REPEAT_LONG_LEN_BITS  11
#define bitmap__RLE_SKIP_LONG_LEN_BITS    10

#define bitmap__RLE_LITERAL_LEN_MAX  (1 << bitmap__RLE_LITERAL_LEN_BITS) /* 128 */
#define bitmap__RLE_REPEAT_LEN_MAX   (1 << bitmap__RLE_REPEAT_LEN_BITS)  /*  64 */
#define bitmap__RLE_SKIP_LEN_MAX     (1 << bitmap__RLE_SKIP_LEN_BITS)    /*  32 */

#define bitmap__RLE_LITERAL_LONG_LEN_MIN (bitmap__RLE_LITERAL_LEN_MAX + 1)
#define bitmap__RLE_REPEAT_LONG_LEN_MIN  (bitmap__RLE_REPEAT_LEN_MAX + 1)
#define bitmap__RLE_SKIP_LONG_LEN_MIN    (bitmap__RLE_SKIP_LEN_MAX + 1)

#define bitmap__RLE_LITERAL_LONG_LEN_MAX \
  ((1 << bitmap__RLE_LITERAL_LONG_LEN_BITS) - 1 + bitmap__RLE_LITERAL_LONG_LEN_MIN)
#define bitmap__RLE_REPEAT_LONG_LEN_MAX \
  ((1 << bitmap__RLE_REPEAT_LONG_LEN_BITS) - 1 + bitmap__RLE_REPEAT_LONG_LEN_MIN)
#define bitmap__RLE_SKIP_LONG_LEN_MAX \
  ((1 << bitmap__RLE_SKIP_LONG_LEN_BITS) - 1 + bitmap__RLE_SKIP_LONG_LEN_MIN)

#define bitmap__RLE_EOL_BYTE 0x01u

/* First byte of each short opcode: a single set bit at position (7 - id). */
#define bitmap__RLE_VAL(id) ((0x80u >> (id)) & 0xFFu)

/* ----------------------------------------------------------------------- */

/** Write `v` as four little-endian bytes to `p`. */
void bitmap__rle_put32(uint8_t *p, uint32_t v);

/** Read four little-endian bytes from `p`. */
uint32_t bitmap__rle_get32(const uint8_t *p);

/* ----------------------------------------------------------------------- */

/**
 * Encode one source row into the opcode stream.
 *
 * \param[in]  srcrow   Row of `npix` pixels, `1 << (log2bpp - 3)` bytes
 *                      each.
 * \param[in]  npix     Pixels in the row.
 * \param[in]  log2bpp  3 (1-byte pixels) or 5 (4-byte pixels).
 * \param[in]  has_alpha Non-zero to emit `skip` for runs of alpha-0 pixels.
 * \param[out] dst      Output cursor.
 * \param[in]  dstcap   Bytes available at `dst`.
 * \param[out] dstused  Bytes written (including the EOL).
 * \return \ref result_OK, or \ref result_BUFFER_OVERFLOW if `dstcap` is
 *         short.
 */
result_t bitmap__rle_encode_row(const void *srcrow,
                                int         npix,
                                int         log2bpp,
                                int         has_alpha,
                                uint8_t    *dst,
                                size_t      dstcap,
                                size_t     *dstused);

/**
 * Decode one row of the opcode stream into `dstrow`, honouring a left-skip
 * and a plot count for clipping.
 *
 * Literal runs are `memcpy`d, repeat runs splatted, `skip` runs advance the
 * destination pointer only (whole-byte formats, so the copy is
 * format-agnostic given `bpp`). `dstrow` points at the first pixel to write
 * (already offset to the clip's left edge); `skip` pixels of the row are
 * consumed before writing begins and at most `plot` pixels are written.
 *
 * \param[in] p         Cursor at the row's first opcode.
 * \param[in] log2bpp   3 or 5.
 * \param[in] dstrow    Destination, at the first pixel to be written.
 * \param[in] skip      Row pixels to consume before writing.
 * \param[in] plot      Maximum pixels to write.
 * \param[in] zero_skip Non-zero to write zero pixels for `skip` runs
 *                      (lossless decode); zero to leave them untouched
 *                      (alpha-tested blit).
 * \return Cursor just past this row's EOL.
 */
const uint8_t *bitmap__rle_decode_row(const uint8_t *p,
                                      int            log2bpp,
                                      void          *dstrow,
                                      int            skip,
                                      int            plot,
                                      int            zero_skip);

/**
 * Advance past one row's opcodes without decoding, for reaching a clipped
 * top row.
 *
 * \param[in] p       Cursor at the row's first opcode.
 * \param[in] end     One past the last valid stream byte.
 * \param[in] log2bpp 3 or 5 (sizes inline literal/repeat pixels).
 * \return Cursor just past this row's EOL (clamped to `end`).
 */
const uint8_t *bitmap__rle_skip_row(const uint8_t *p,
                                    const uint8_t *end,
                                    int            log2bpp);

#endif /* FRAMEBUF_BITMAP_RLE_H */
