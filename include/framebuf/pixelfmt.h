/* pixelfmt.h -- pixel formats */

#ifndef FRAMEBUF_PIXELFMT_H
#define FRAMEBUF_PIXELFMT_H

#ifdef __cplusplus
extern "C"
{
#endif

/** Represents a pixel format. */
// 'rgbx8888' would be 0xXXBBGGRR
// round up to whole units using X if that part of the word is empty
typedef enum pixelfmt
{
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

  /* 24bpp */
  pixelfmt_bgrx8888,       // 0bXXXXXXXXRRRRRRRRGGGGGGGGBBBBBBBB
  pixelfmt_rgbx8888,       // 0bXXXXXXXXBBBBBBBBGGGGGGGGRRRRRRRR (the RISC OS 24bpp format)
  pixelfmt_xbgr8888,       // 0bRRRRRRRRGGGGGGGGBBBBBBBBXXXXXXXX (an OS X 24bpp format)
  pixelfmt_xrgb8888,       // 0bBBBBBBBBGGGGGGGGRRRRRRRRXXXXXXXX

  /* 32bpp */
  pixelfmt_bgra8888,       // 0bAAAAAAAARRRRRRRRGGGGGGGGBBBBBBBB
  pixelfmt_rgba8888,       // 0bAAAAAAAABBBBBBBBGGGGGGGGRRRRRRRR (the RISC OS 32bpp format)
  pixelfmt_abgr8888,       // 0bRRRRRRRRGGGGGGGGBBBBBBBBAAAAAAAA
  pixelfmt_argb8888,       // 0bBBBBBBBBGGGGGGGGRRRRRRRRAAAAAAAA

  /* other */
  pixelfmt_unknown,        /* represents an unknown/undefined pixel format */
}
pixelfmt_t;

typedef unsigned char  pixelfmt_y8_t;

typedef unsigned short pixelfmt_bgrx4444_t;
typedef unsigned short pixelfmt_rgbx4444_t;
typedef unsigned short pixelfmt_xbgr4444_t;
typedef unsigned short pixelfmt_xrgb4444_t;

typedef unsigned short pixelfmt_bgrx5551_t;
typedef unsigned short pixelfmt_rgbx5551_t;
typedef unsigned short pixelfmt_xbgr1555_t;
typedef unsigned short pixelfmt_xrgb1555_t;

typedef unsigned short pixelfmt_bgr565_t;
typedef unsigned short pixelfmt_rgb565_t;

typedef unsigned int   pixelfmt_bgrx8888_t;
typedef unsigned int   pixelfmt_rgbx8888_t;
typedef unsigned int   pixelfmt_xbgr8888_t;
typedef unsigned int   pixelfmt_xrgb8888_t;

typedef unsigned int   pixelfmt_bgra8888_t;
typedef unsigned int   pixelfmt_rgba8888_t;
typedef unsigned int   pixelfmt_abgr8888_t;
typedef unsigned int   pixelfmt_argb8888_t;

/* Given a pixel format return its log2 bits-per-pixel size. */
int pixelfmt_log2bpp(pixelfmt_t fmt);

#ifdef __cplusplus
}
#endif

#endif /* FRAMEBUF_PIXELFMT_H */

