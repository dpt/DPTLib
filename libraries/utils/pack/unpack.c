/* unpack.c -- structure unpacking */

#include <assert.h>
#include <ctype.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

#include "base/types.h"

#include "utils/pack.h"

#define SRC_b 0
#define SRC_h 1
#define SRC_w 2
#define SRC_d 3
#define NSRC  4

#define DST_C 0
#define DST_S 1
#define DST_I 2
#define DST_Q 3
#define DST_c 4
#define DST_s 5
#define DST_i 6
#define DST_q 7
#define NDST  8

#if defined(__LP64__) || defined(_WIN64)
#define PTRCHR_32
#define PTRCHR_64 case 'P':
#else
#define PTRCHR_32 case 'P':
#define PTRCHR_64
#endif

#define MERGE(dst, src) (((dst) << 2) + (src))

/* Declare a function which unpacks. */
#define DECLARE_UNPACKER(name, o0, o1, o2, o3, o4, o5, o6, o7)           \
static size_t name(const unsigned char *buf, const char *fmt, va_list args) \
{                                                                        \
  const uint8_t *bp;                                                     \
  const char    *p;                                                      \
                                                                         \
  bp = buf;                                                              \
                                                                         \
  for (p = fmt; *p != '\0'; )                                            \
  {                                                                      \
    int c;                                                               \
    int n;                                                               \
    int src, dst;                                                        \
                                                                         \
    c = *p++;                                                            \
                                                                         \
    if (c == '*') /* array mode */                                       \
    {                                                                    \
      /* next arg is number of elements to unpack to array */            \
      n = va_arg(args, int);                                             \
      c = *p++;                                                          \
                                                                         \
      /* decode source size specifier where present */                   \
                                                                         \
      switch (c)                                                         \
      {                                                                  \
      case 'b': src = SRC_b; break;                                      \
      case 'h': src = SRC_h; break;                                      \
      case 'w': src = SRC_w; break;                                      \
      case 'd': src = SRC_d; break;                                      \
      default:  src = -1; break;                                         \
      }                                                                  \
                                                                         \
      if (src >= 0)                                                      \
        c = *p++;                                                        \
                                                                         \
      /* decode destination size specifier where present */              \
                                                                         \
      switch (c)                                                         \
      {                                                                  \
      case 'C': dst = DST_C; break;                                      \
      case 'S': dst = DST_S; break;                                      \
      case 'I': PTRCHR_32 dst = DST_I; break;                            \
      case 'Q': PTRCHR_64 dst = DST_Q; break;                            \
      case 'c': dst = DST_c; break;                                      \
      case 's': dst = DST_s; break;                                      \
      case 'i': dst = DST_i; break;                                      \
      case 'q': dst = DST_q; break;                                      \
      default:                                                           \
        assert("unpack: Illegal format specifier" == NULL);              \
        return 0;                                                        \
      }                                                                  \
                                                                         \
      /* allow lone C/S/I/Q/c/s/i/q specifiers to mean bC/hS/wI/dQ/bc/hs/wi/dq */ \
                                                                         \
      if (src < 0)                                                       \
        src = dst % (NDST / 2);                                          \
                                                                         \
      switch (MERGE(dst,src))                                            \
      {                                                                  \
      case MERGE(DST_C, SRC_b): /* bC, read byte into unsigned char */   \
      case MERGE(DST_c, SRC_b): /* bc, read byte into signed char */     \
        {                                                                \
          uint8_t *a;                                                    \
                                                                         \
          a = va_arg(args, uint8_t *);                                   \
          memcpy(a, bp, n);                                              \
          bp += n;                                                       \
        }                                                                \
        break;                                                           \
                                                                         \
      case MERGE(DST_S, SRC_b): /* bS, read byte into unsigned short */  \
        {                                                                \
          uint16_t *a;                                                   \
                                                                         \
          a = va_arg(args, uint16_t *);                                  \
          while (n--)                                                    \
            *a++ = *bp++;                                                \
        }                                                                \
        break;                                                           \
                                                                         \
      case MERGE(DST_s, SRC_b): /* bs, read byte into signed short */    \
        {                                                                \
          int16_t *a;                                                    \
                                                                         \
          a = va_arg(args, int16_t *);                                   \
          while (n--)                                                    \
            *a++ = (signed char) *bp++;                                  \
        }                                                                \
        break;                                                           \
                                                                         \
      case MERGE(DST_I, SRC_b): /* bI, read byte into unsigned int */    \
        {                                                                \
          uint32_t *a;                                                   \
                                                                         \
          a = va_arg(args, uint32_t *);                                  \
          while (n--)                                                    \
            *a++ = *bp++;                                                \
        }                                                                \
        break;                                                           \
                                                                         \
      case MERGE(DST_i, SRC_b): /* bi, read byte into signed int */      \
        {                                                                \
          int32_t *a;                                                    \
                                                                         \
          a = va_arg(args, int32_t *);                                   \
          while (n--)                                                    \
            *a++ = (signed char) *bp++;                                  \
        }                                                                \
        break;                                                           \
                                                                         \
      case MERGE(DST_Q, SRC_b): /* bQ, read byte into unsigned long long */ \
        {                                                                \
          uint64_t *a;                                                   \
                                                                         \
          a = va_arg(args, uint64_t *);                                  \
          while (n--)                                                    \
            *a++ = *bp++;                                                \
        }                                                                \
        break;                                                           \
                                                                         \
      case MERGE(DST_q, SRC_b): /* bq, read byte into signed long long */ \
        {                                                                \
          int64_t *a;                                                    \
                                                                         \
          a = va_arg(args, int64_t *);                                   \
          while (n--)                                                    \
            *a++ = (signed char) *bp++;                                  \
        }                                                                \
        break;                                                           \
                                                                         \
                                                                         \
      case MERGE(DST_S, SRC_h): /* hS, read halfword into unsigned short */ \
      case MERGE(DST_s, SRC_h): /* hs, read halfword into signed short */   \
        {                                                                \
          uint16_t *a;                                                   \
                                                                         \
          a = va_arg(args, uint16_t *);                                  \
          while (n--)                                                    \
          {                                                              \
            *a++ = (uint16_t) (bp[o0] | (bp[o1] << 8));                  \
            bp += 2;                                                     \
          }                                                              \
        }                                                                \
        break;                                                           \
                                                                         \
      case MERGE(DST_I, SRC_h): /* hI, read halfword into unsigned int */ \
        {                                                                \
          uint32_t *a;                                                   \
                                                                         \
          a = va_arg(args, uint32_t *);                                  \
          while (n--)                                                    \
          {                                                              \
            *a++ = (uint32_t) (bp[o0] | (bp[o1] << 8));                  \
            bp += 2;                                                     \
          }                                                              \
        }                                                                \
        break;                                                           \
                                                                         \
      case MERGE(DST_i, SRC_h): /* hi, read halfword into signed int */  \
        {                                                                \
          int32_t *a;                                                    \
                                                                         \
          a = va_arg(args, int32_t *);                                   \
          while (n--)                                                    \
          {                                                              \
            *a++ = (int32_t) (bp[o0] | (bp[o1] << 8));                   \
            bp += 2;                                                     \
          }                                                              \
        }                                                                \
        break;                                                           \
                                                                         \
      case MERGE(DST_Q, SRC_h): /* hQ, read halfword into unsigned long long */ \
        {                                                                \
          uint64_t *a;                                                   \
                                                                         \
          a = va_arg(args, uint64_t *);                                  \
          while (n--)                                                    \
          {                                                              \
            *a++ = (uint64_t) (bp[o0] | (bp[o1] << 8));                  \
            bp += 2;                                                     \
          }                                                              \
        }                                                                \
        break;                                                           \
                                                                         \
      case MERGE(DST_q, SRC_h): /* hq, read halfword into signed long long */ \
        {                                                                \
          int64_t *a;                                                    \
                                                                         \
          a = va_arg(args, int64_t *);                                   \
          while (n--)                                                    \
          {                                                              \
            *a++ = (int64_t) (bp[o0] | (bp[o1] << 8));                   \
            bp += 2;                                                     \
          }                                                              \
        }                                                                \
        break;                                                           \
                                                                         \
                                                                         \
      case MERGE(DST_I, SRC_w): /* wI, read word into unsigned int */    \
      case MERGE(DST_i, SRC_w): /* wi, read word into signed int */      \
        {                                                                \
          uint32_t *a;                                                   \
                                                                         \
          a = va_arg(args, uint32_t *);                                  \
          while (n--)                                                    \
          {                                                              \
            *a++ = (uint32_t) (bp[o0] | (bp[o1] << 8) | (bp[o2] << 16) | (bp[o3] << 24)); \
            bp += 4;                                                     \
          }                                                              \
        }                                                                \
        break;                                                           \
                                                                         \
      case MERGE(DST_Q, SRC_w): /* wQ, read word into unsigned long long */ \
        {                                                                \
          uint64_t *a;                                                   \
                                                                         \
          a = va_arg(args, uint64_t *);                                  \
          while (n--)                                                    \
          {                                                              \
            *a++ = (uint32_t) (bp[o0] | (bp[o1] << 8) | (bp[o2] << 16) | (bp[o3] << 24)); \
            bp += 4;                                                     \
          }                                                              \
        }                                                                \
        break;                                                           \
                                                                         \
      case MERGE(DST_q, SRC_w): /* wq, read word into signed long long */ \
        {                                                                \
          int64_t *a;                                                    \
                                                                         \
          a = va_arg(args, int64_t *);                                   \
          while (n--)                                                    \
          {                                                              \
            *a++ = (int32_t) (bp[o0] | (bp[o1] << 8) | (bp[o2] << 16) | (bp[o3] << 24)); \
            bp += 4;                                                     \
          }                                                              \
        }                                                                \
        break;                                                           \
                                                                         \
                                                                         \
      case MERGE(DST_Q, SRC_d): /* dQ, read double word into unsigned long long */ \
      case MERGE(DST_q, SRC_d): /* dq, read double word into signed long long */   \
        {                                                                \
          uint64_t *a;                                                   \
                                                                         \
          a = va_arg(args, uint64_t *);                                  \
          while (n--)                                                    \
          {                                                              \
            *a++ = ((uint64_t) bp[o0] | ((uint64_t) bp[o1] << 8) | ((uint64_t) bp[o2] << 16) | ((uint64_t) bp[o3] << 24) | ((uint64_t) bp[o4] << 32) | ((uint64_t) bp[o5] << 40) | ((uint64_t) bp[o6] << 48) | ((uint64_t) bp[o7] << 56)); \
            bp += 8;                                                     \
          }                                                              \
        }                                                                \
        break;                                                           \
                                                                         \
                                                                         \
      default:                                                           \
        assert("unpack: Conversion would truncate" == NULL);             \
        return 0;                                                        \
      }                                                                  \
    }                                                                    \
    else                                                                 \
    {                                                                    \
      if (isdigit(c))                                                    \
      {                                                                  \
        /* get repeat count, if any */                                   \
        n = c - '0';                                                     \
        for (;;)                                                         \
        {                                                                \
          c = *p++;                                                      \
          if (!isdigit(c)) break;                                        \
          n = n * 10 + (c - '0');                                        \
        }                                                                \
      }                                                                  \
      else                                                               \
      {                                                                  \
        /* default repeat count */                                       \
        n = 1;                                                           \
      }                                                                  \
                                                                         \
      /* decode source size specifier where present */                   \
                                                                         \
      switch (c)                                                         \
      {                                                                  \
      case 'b': src = SRC_b; break;                                      \
      case 'h': src = SRC_h; break;                                      \
      case 'w': src = SRC_w; break;                                      \
      case 'd': src = SRC_d; break;                                      \
      default:  src = -1; break;                                         \
      }                                                                  \
                                                                         \
      if (src >= 0)                                                      \
        c = *p++;                                                        \
                                                                         \
      /* decode destination size specifier where present */              \
                                                                         \
      switch (c)                                                         \
      {                                                                  \
      case 'C': dst = DST_C; break;                                      \
      case 'S': dst = DST_S; break;                                      \
      case 'I': PTRCHR_32 dst = DST_I; break;                            \
      case 'Q': PTRCHR_64 dst = DST_Q; break;                            \
      case 'c': dst = DST_c; break;                                      \
      case 's': dst = DST_s; break;                                      \
      case 'i': dst = DST_i; break;                                      \
      case 'q': dst = DST_q; break;                                      \
      default:                                                           \
        assert("unpack: Illegal format specifier" == NULL);              \
        return 0;                                                        \
      }                                                                  \
                                                                         \
      /* allow lone C/S/I/Q/c/s/i/q specifiers to mean bC/hS/wI/dQ/bc/hs/wi/dq */ \
                                                                         \
      if (src < 0)                                                       \
        src = dst % (NDST / 2);                                          \
                                                                         \
      switch ((dst << 2) + src)                                          \
      {                                                                  \
      /* formatting dilemma! */                                          \
      uint8_t  *pC;                                                      \
      uint16_t *pS;                                                      \
      uint32_t *pI;                                                      \
      uint64_t *pQ;                                                      \
      int16_t  *ps;                                                      \
      int32_t  *pi;                                                      \
      int64_t  *pq;                                                      \
                                                                         \
      case MERGE(DST_C, SRC_b): /* bC, read byte into unsigned char */   \
      case MERGE(DST_c, SRC_b): /* bc, read byte into signed char */     \
        while (n--)                                                      \
        {                                                                \
          pC = va_arg(args, uint8_t *);                                  \
          *pC = *bp++;                                                   \
        }                                                                \
        break;                                                           \
                                                                         \
      case MERGE(DST_S, SRC_b): /* bS, read byte into unsigned short */  \
        while (n--)                                                      \
        {                                                                \
          pS = va_arg(args, uint16_t *);                                 \
          *pS = *bp++;                                                   \
        }                                                                \
        break;                                                           \
                                                                         \
      case MERGE(DST_s, SRC_b): /* bs, read byte into signed short */    \
        while (n--)                                                      \
        {                                                                \
          ps = va_arg(args, int16_t *);                                  \
          *ps = (int8_t) *bp++;                                          \
        }                                                                \
        break;                                                           \
                                                                         \
      case MERGE(DST_I, SRC_b): /* bI, read byte into unsigned int */    \
        while (n--)                                                      \
        {                                                                \
          pI = va_arg(args, uint32_t *);                                 \
          *pI = *bp++;                                                   \
        }                                                                \
        break;                                                           \
                                                                         \
      case MERGE(DST_i, SRC_b): /* bi, read byte into signed int */      \
        while (n--)                                                      \
        {                                                                \
          pi = va_arg(args, int32_t *);                                  \
          *pi = (int8_t) *bp++;                                          \
        }                                                                \
        break;                                                           \
                                                                         \
      case MERGE(DST_Q, SRC_b): /* bQ, read byte into unsigned long long */ \
        while (n--)                                                      \
        {                                                                \
          pQ = va_arg(args, uint64_t *);                                 \
          *pQ = *bp++;                                                   \
        }                                                                \
        break;                                                           \
                                                                         \
      case MERGE(DST_q, SRC_b): /* bq, read byte into signed long long */ \
        while (n--)                                                      \
        {                                                                \
          pq = va_arg(args, int64_t *);                                  \
          *pq = (int8_t) *bp++;                                          \
        }                                                                \
        break;                                                           \
                                                                         \
                                                                         \
      case MERGE(DST_S, SRC_h): /* hS, read halfword into unsigned short */ \
      case MERGE(DST_s, SRC_h): /* hs, read halfword into signed short */ \
        while (n--)                                                      \
        {                                                                \
          pS = va_arg(args, uint16_t *);                                 \
          *pS = (uint16_t) (bp[o0] | (bp[o1] << 8));                     \
          bp += 2;                                                       \
        }                                                                \
        break;                                                           \
                                                                         \
      case MERGE(DST_I, SRC_h): /* hI, read halfword into unsigned int */ \
        while (n--)                                                      \
        {                                                                \
          pI = va_arg(args, uint32_t *);                                 \
          *pI = (uint16_t) (bp[o0] | (bp[o1] << 8));                     \
          bp += 2;                                                       \
        }                                                                \
        break;                                                           \
                                                                         \
      case MERGE(DST_i, SRC_h): /* hi, read halfword into signed int */  \
        while (n--)                                                      \
        {                                                                \
          pi = va_arg(args, int32_t *);                                  \
          *pi = (int16_t) (bp[o0] | (bp[o1] << 8));                      \
          bp += 2;                                                       \
        }                                                                \
        break;                                                           \
                                                                         \
      case MERGE(DST_Q, SRC_h): /* hQ, read halfword into unsigned long long */ \
        while (n--)                                                      \
        {                                                                \
          pQ = va_arg(args, uint64_t *);                                 \
          *pQ = (uint16_t) (bp[o0] | (bp[o1] << 8));                     \
          bp += 2;                                                       \
        }                                                                \
        break;                                                           \
                                                                         \
      case MERGE(DST_q, SRC_h): /* hq, read halfword into signed long long */  \
        while (n--)                                                      \
        {                                                                \
          pq = va_arg(args, int64_t *);                                  \
          *pq = (int16_t) (bp[o0] | (bp[o1] << 8));                      \
          bp += 2;                                                       \
        }                                                                \
        break;                                                           \
                                                                         \
                                                                         \
      case MERGE(DST_I, SRC_w): /* wI, read word into unsigned int */    \
      case MERGE(DST_i, SRC_w): /* wi, read word into signed int */      \
        while (n--)                                                      \
        {                                                                \
          pI = va_arg(args, uint32_t *);                                 \
          *pI = (uint32_t) (bp[o0] | (bp[o1] << 8) | (bp[o2] << 16) | (bp[o3] << 24)); \
          bp += 4;                                                       \
        }                                                                \
        break;                                                           \
                                                                         \
      case MERGE(DST_Q, SRC_w): /* wQ, read word into unsigned long long */ \
        while (n--)                                                      \
        {                                                                \
          pQ = va_arg(args, uint64_t *);                                 \
          *pQ = (uint32_t) (bp[o0] | (bp[o1] << 8) | (bp[o2] << 16) | (bp[o3] << 24)); \
          bp += 4;                                                       \
        }                                                                \
        break;                                                           \
                                                                         \
      case MERGE(DST_q, SRC_w): /* wq, read word into signed long long */ \
        while (n--)                                                      \
        {                                                                \
          pq = va_arg(args, int64_t *);                                  \
          *pq = (int32_t) (bp[o0] | (bp[o1] << 8) | (bp[o2] << 16) | (bp[o3] << 24)); \
          bp += 4;                                                       \
        }                                                                \
        break;                                                           \
                                                                         \
                                                                         \
      case MERGE(DST_Q, SRC_d): /* dQ, read double word into unsigned long long */ \
      case MERGE(DST_q, SRC_d): /* dq, read double word into signed long long */   \
        while (n--)                                                      \
        {                                                                \
          pQ = va_arg(args, uint64_t *);                                  \
          *pQ = ((uint64_t) bp[o0] | ((uint64_t) bp[o1] << 8) | ((uint64_t) bp[o2] << 16) | ((uint64_t) bp[o3] << 24) | ((uint64_t) bp[o4] << 32) | ((uint64_t) bp[o5] << 40) | ((uint64_t) bp[o6] << 48) | ((uint64_t) bp[o7] << 56)); \
          bp += 8;                                                       \
        }                                                                \
        break;                                                           \
                                                                         \
                                                                         \
      default:                                                           \
        assert("unpack: Conversion would truncate" == NULL);             \
        return 0;                                                        \
      }                                                                  \
    }                                                                    \
  }                                                                      \
                                                                         \
  return (size_t) (bp - buf);                                            \
}

DECLARE_UNPACKER(vunpack_le, 0, 1, 2, 3, 4, 5, 6, 7)
DECLARE_UNPACKER(vunpack_be, 7, 6, 5, 4, 3, 2, 1, 0)

#define NATIVE_UNPACKER vunpack_le

size_t vunpack(const unsigned char *buf, const char *fmt, va_list args)
{
  size_t (*unpacker)(const unsigned char *, const char *, va_list);
  
  unpacker = NATIVE_UNPACKER;
  
  if (*fmt == '<')
  {
    unpacker = vunpack_le;
    fmt++;
  }
  else if (*fmt == '>')
  {
    unpacker = vunpack_be;
    fmt++;
  }

  return unpacker(buf, fmt, args);
}

size_t unpack(const unsigned char *buf, const char *fmt, ...)
{
  va_list args;
  size_t  c;

  va_start(args, fmt);

  c = vunpack(buf, fmt, args);

  va_end(args);

  return c;
}
