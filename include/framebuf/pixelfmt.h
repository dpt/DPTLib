/* framebuf/pixelfmt.h -- pixel formats */

#ifndef FRAMEBUF_PIXELFMT_H
#define FRAMEBUF_PIXELFMT_H

#ifdef __cplusplus
extern "C"
{
#endif

/* ----------------------------------------------------------------------- */

/**
 * All currently known pixel formats.
 *
 * \note `pixelfmt_rgbx8888` represents a pixel of the form 0xXXBBGGRR.
 *
 * \note Round up to whole units using X if that part of the word is empty.
 */
typedef enum pixelfmt
{
  /* packed/paletted */
  pixelfmt_p1,             /* 1bpp paletted */
  pixelfmt_p2,             /* 2bpp paletted */
  pixelfmt_p4,             /* 4bpp paletted */
  pixelfmt_p8,             /* 8bpp paletted */

  /* 8bpp */
  pixelfmt_y8,             /* 8bpp grey */

  /* 12bpp */
  pixelfmt_bgrx4444,       // 0bXXXXRRRRGGGGBBBB
  pixelfmt_rgbx4444,       // 0bXXXXBBBBGGGGRRRR
  pixelfmt_xbgr4444,       // 0bRRRRGGGGBBBBXXXX
  pixelfmt_xrgb4444,       // 0bBBBBGGGGRRRRXXXX

  /* 15bpp */
  pixelfmt_bgrx5551,       // 0bXRRRRRGGGGGBBBBB
  pixelfmt_rgbx5551,       // 0bXBBBBBGGGGGRRRRR (the RISC OS 15bpp format)
  pixelfmt_xbgr1555,       // 0bRRRRRGGGGGBBBBBX
  pixelfmt_xrgb1555,       // 0bBBBBBGGGGGRRRRRX

  /* 16bpp */
  pixelfmt_bgr565,         // 0bRRRRRGGGGGGBBBBB
  pixelfmt_rgb565,         // 0bBBBBBGGGGGGRRRRR

  /* 32bpp, no alpha */
  pixelfmt_bgrx8888,       // 0bXXXXXXXXRRRRRRRRGGGGGGGGBBBBBBBB
  pixelfmt_rgbx8888,       // 0bXXXXXXXXBBBBBBBBGGGGGGGGRRRRRRRR (the RISC OS 24bpp format)
  pixelfmt_xbgr8888,       // 0bRRRRRRRRGGGGGGGGBBBBBBBBXXXXXXXX (an OS X 24bpp format)
  pixelfmt_xrgb8888,       // 0bBBBBBBBBGGGGGGGGRRRRRRRRXXXXXXXX

  /* 32bpp with alpha */
  pixelfmt_bgra8888,       // 0bAAAAAAAARRRRRRRRGGGGGGGGBBBBBBBB
  pixelfmt_rgba8888,       // 0bAAAAAAAABBBBBBBBGGGGGGGGRRRRRRRR (the RISC OS 32bpp format)
  pixelfmt_abgr8888,       // 0bRRRRRRRRGGGGGGGGBBBBBBBBAAAAAAAA
  pixelfmt_argb8888,       // 0bBBBBBBBBGGGGGGGGRRRRRRRRAAAAAAAA

  /* other */
  pixelfmt_unknown,        /* represents an unknown/undefined pixel format */
}
pixelfmt_t;

/* ----------------------------------------------------------------------- */

typedef unsigned int   pixelfmt_p1_t; /* packed/paletted 1bpp */
typedef unsigned int   pixelfmt_p2_t; /* packed/paletted 2bpp */
typedef unsigned int   pixelfmt_p4_t; /* packed/paletted 4bpp */
typedef unsigned int   pixelfmt_p8_t; /* packed/paletted 8bpp */

typedef unsigned char  pixelfmt_y8_t; /* 8bpp grey */

typedef unsigned short pixelfmt_bgrx4444_t; /* 12bpp bgrx4444 */
typedef unsigned short pixelfmt_rgbx4444_t; /* 12bpp rgbx4444 */
typedef unsigned short pixelfmt_xbgr4444_t; /* 12bpp xbgr4444 */
typedef unsigned short pixelfmt_xrgb4444_t; /* 12bpp xrgb4444 */

typedef unsigned short pixelfmt_bgrx5551_t; /* 15bpp bgrx5551 */
typedef unsigned short pixelfmt_rgbx5551_t; /* 15bpp rgbx5551 */
typedef unsigned short pixelfmt_xbgr1555_t; /* 15bpp xbgr1555 */
typedef unsigned short pixelfmt_xrgb1555_t; /* 15bpp xrgb1555 */

typedef unsigned short pixelfmt_bgr565_t; /* 16bpp bgr565 */
typedef unsigned short pixelfmt_rgb565_t; /* 16bpp rgb565 */

typedef unsigned int   pixelfmt_bgrx8888_t; /* 24bpp bgrx8888 */
typedef unsigned int   pixelfmt_rgbx8888_t; /* 24bpp rgbx8888 */
typedef unsigned int   pixelfmt_xbgr8888_t; /* 24bpp xbgr8888 */
typedef unsigned int   pixelfmt_xrgb8888_t; /* 24bpp xrgb8888 */

typedef unsigned int   pixelfmt_bgra8888_t; /* 32bpp bgra8888 */
typedef unsigned int   pixelfmt_rgba8888_t; /* 32bpp rgba8888 */
typedef unsigned int   pixelfmt_abgr8888_t; /* 32bpp abgr8888 */
typedef unsigned int   pixelfmt_argb8888_t; /* 32bpp argb8888 */

typedef unsigned int   pixelfmt_xxxa8888_t; /* any 32bpp alpha */

typedef unsigned int   pixelfmt_any_t; /* generic/unspecified pixel */
typedef unsigned char  pixelfmt_any8_t; /* any 8bpp pixel */
typedef unsigned short pixelfmt_any16_t; /* any 16bpp pixel */
typedef unsigned int   pixelfmt_any32_t; /* any 32bpp pixel */

/* ----------------------------------------------------------------------- */

#define PIXELFMT_TRANSPARENT (0x00u) /* transparent pixel */
#define PIXELFMT_OPAQUE      (0xFFu) /* opaque pixel */

/* ----------------------------------------------------------------------- */

/**
 * Extract a field from a pixel.
 *
 * \param px Pixel value.
 * \param sh Right shift amount.
 * \param n Mask.
 */
#define PIXELFMT_EXTRACT(px,sh,n) (((pixelfmt_any_t) (px) >> (sh)) & (n))

/* Constants for RGB565 */
#define PIXELFMT_Rxx565_SHIFT  (0)
#define PIXELFMT_xGx565_SHIFT  (5)
#define PIXELFMT_xxB565_SHIFT (10)
#define PIXELFMT_Rxx565_MASK  (0x1Fu <<  0)
#define PIXELFMT_xGx565_MASK  (0x3Fu <<  5)
#define PIXELFMT_xxB565_MASK  (0x1Fu << 10)
#define PIXELFMT_Rxx565(px) PIXELFMT_EXTRACT(px, PIXELFMT_Rxx565_SHIFT, 0x1Fu)
#define PIXELFMT_xGx565(px) PIXELFMT_EXTRACT(px, PIXELFMT_xGx565_SHIFT, 0x3Fu)
#define PIXELFMT_xxB565(px) PIXELFMT_EXTRACT(px, PIXELFMT_xxB565_SHIFT, 0x1Fu)

/* Constants for any 8888 */
#define PIXELFMT_Rxxx8888_SHIFT  (0)
#define PIXELFMT_Bxxx8888_SHIFT  (0)
#define PIXELFMT_xGxx8888_SHIFT  (8)
#define PIXELFMT_xxBx8888_SHIFT (16)
#define PIXELFMT_xxRx8888_SHIFT (16)
#define PIXELFMT_xxxA8888_SHIFT (24)
#define PIXELFMT_Rxxx8888_MASK  (0xFFu <<  0)
#define PIXELFMT_Bxxx8888_MASK  (0xFFu <<  0)
#define PIXELFMT_xGxx8888_MASK  (0xFFu <<  8)
#define PIXELFMT_xxBx8888_MASK  (0xFFu << 16)
#define PIXELFMT_xxRx8888_MASK  (0xFFu << 16)
#define PIXELFMT_xxxA8888_MASK  (0xFFu << 24)

/* Constants for BGRX8888 */
#define PIXELFMT_Bxxx8888(px) PIXELFMT_EXTRACT(px, PIXELFMT_Bxxx8888_SHIFT, 0xFFu)
#define PIXELFMT_xGxx8888(px) PIXELFMT_EXTRACT(px, PIXELFMT_xGxx8888_SHIFT, 0xFFu)
#define PIXELFMT_xxRx8888(px) PIXELFMT_EXTRACT(px, PIXELFMT_xxRx8888_SHIFT, 0xFFu)
#define PIXELFMT_MAKE_BGRX8888(R,G,B) \
  (((pixelfmt_any_t) (R) << PIXELFMT_Bxxx8888_SHIFT) | \
   ((pixelfmt_any_t) (G) << PIXELFMT_xGxx8888_SHIFT) | \
   ((pixelfmt_any_t) (B) << PIXELFMT_xxRx8888_SHIFT) | \
   ((pixelfmt_any_t) (0xFF) << PIXELFMT_xxxA8888_SHIFT))

/* Constants for RGBA8888 */
#define PIXELFMT_Rxxx8888(px) PIXELFMT_EXTRACT(px, PIXELFMT_Rxxx8888_SHIFT, 0xFFu)
#define PIXELFMT_xGxx8888(px) PIXELFMT_EXTRACT(px, PIXELFMT_xGxx8888_SHIFT, 0xFFu)
#define PIXELFMT_xxBx8888(px) PIXELFMT_EXTRACT(px, PIXELFMT_xxBx8888_SHIFT, 0xFFu)
#define PIXELFMT_xxxA8888(px) PIXELFMT_EXTRACT(px, PIXELFMT_xxxA8888_SHIFT, 0xFFu)
#define PIXELFMT_MAKE_RGBA8888(R,G,B,A) \
  (((pixelfmt_any_t) (R) << PIXELFMT_Rxxx8888_SHIFT) | \
   ((pixelfmt_any_t) (G) << PIXELFMT_xGxx8888_SHIFT) | \
   ((pixelfmt_any_t) (B) << PIXELFMT_xxBx8888_SHIFT) | \
   ((pixelfmt_any_t) (A) << PIXELFMT_xxxA8888_SHIFT))

/* Constants for BGRA8888 */
#define PIXELFMT_MAKE_BGRA8888(R,G,B,A) \
  (((pixelfmt_any_t) (B) << PIXELFMT_Bxxx8888_SHIFT) | \
   ((pixelfmt_any_t) (G) << PIXELFMT_xGxx8888_SHIFT) | \
   ((pixelfmt_any_t) (R) << PIXELFMT_xxRx8888_SHIFT) | \
   ((pixelfmt_any_t) (A) << PIXELFMT_xxxA8888_SHIFT))

/* ----------------------------------------------------------------------- */

/**
 * Given a pixel format return its log2 bits-per-pixel size.
 *
 * \param fmt Pixel format.
 * \return Log2 bits-per-pixel size.
 */
int pixelfmt_log2bpp(pixelfmt_t fmt);

/* ----------------------------------------------------------------------- */

#ifdef __cplusplus
}
#endif

#endif /* FRAMEBUF_PIXELFMT_H */

