/* utils/pack/test/pack-test.c -- tests for pack/unpack */

#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#ifdef FORTIFY
#include "fortify/fortify.h"
#endif

#include "base/result.h"
#include "base/utils.h"

#include "utils/pack.h"

#include "test/all-tests.h"

result_t pack_test(const char *resources)
{
  NOT_USED(resources);

  printf("test: pack and unpack\n");

  {
    static const int  data[] = { -0x80, 0x7f,
                                 -0x8000, 0x7fff,
                                 INT_MIN, INT_MAX
                               };

    static const char fmt[] = "2c2s2i";

    const size_t      sz = 14;
    unsigned char     buf[100];
    size_t            n;
    signed char       c1, c2;
    short             s1, s2;
    int               l1, l2;

    n = pack(buf, fmt, data[0], data[1], data[2], data[3], data[4], data[5]);
    if (n != sz)
    {
      printf("Failure: pack() returned unexpected size\n");
      goto Failure;
    }

    n = unpack(buf, fmt, &c1, &c2, &s1, &s2, &l1, &l2);
    if (n != sz || c1 != data[0] || c2 != data[1] ||
        s1 != data[2] || s2 != data[3] ||
        l1 != data[4] || l2 != data[5])
    {
      printf("Failure: unpack():\n"
             "%d %d, %d %d, %d %d, %d %d, %d %d, %d %d\n",
             c1, data[0], c2, data[1],
             s1, data[2], s2, data[3],
             l1, data[4], l2, data[5]);
      goto Failure;
    }
  }

  printf("test: unpack (array mode)\n");

  {
    static const short         shorts[]       = { -0x8000, 0x7fff,
                                                  0x0000, 0x5555
                                                };
    static const unsigned char packedshorts[] = {  0x80, 0x00, 0x7f, 0xff,
                                                   0x00, 0x00, 0x55, 0x55
                                                };

    short  unpackedshorts[4];
    size_t n;

    /* unpacking a big-endian array of shorts */

    n = unpack(packedshorts, ">*s", NELEMS(unpackedshorts), unpackedshorts);
    if (n != 8 || unpackedshorts[0] != shorts[0] ||
        unpackedshorts[1] != shorts[1] ||
        unpackedshorts[2] != shorts[2] ||
        unpackedshorts[3] != shorts[3])
    {
      printf("Failure: unpack() array:\n"
             "%d %d, %d %d, %d %d, %d %d\n",
             unpackedshorts[0], shorts[0],
             unpackedshorts[1], shorts[1],
             unpackedshorts[2], shorts[2],
             unpackedshorts[3], shorts[3]);
      goto Failure;
    }
  }

  printf("test: pack (array mode)\n");

  {
    static const unsigned char  cdata[2]     = { 0x00, 0xff };
    static const unsigned short sdata[2]     = { 0x5500, 0xf0ff };
    static const unsigned int   ldata[2]     = { 0x00005500, 0xff00ffff };
    static const unsigned char  expected[14] = { 0x00, 0xff,
                                                 0x00, 0x55,
                                                 0xff, 0xf0,
                                                 0x00, 0x55,
                                                 0x00, 0x00,
                                                 0xff, 0xff,
                                                 0x00, 0xff
                                               };
    static const char fmt[]        = "*c*s*i";

    const size_t      sz = 2 + 4 + 8;
    unsigned char     buf[100];
    size_t            n;

    n = pack(buf, fmt, 2, cdata, 2, sdata, 2, ldata);
    if (n != sz)
    {
      printf("Failure: pack() using '*' returned unexpected size (%zu)\n", n);
      goto Failure;
    }

    if (memcmp(buf, expected, sz) != 0)
    {
      printf("Failure: pack() using '*'\n");
      goto Failure;
    }
  }

  printf("test: pack quads and repeat counts\n");

  {
    static const uint64_t      qdata[2]     = { 0x1122334455667788ull,
                                                0x8877665544332211ull
                                              };
    static const unsigned char expected[37] = { 0xAB,
                                                0x08, 0x07, 0x06, 0x05,
                                                0x04, 0x03, 0x02, 0x01,
                                                0, 1, 2, 3, 4, 5, 6, 7, 8, 9,
                                                10, 11,
                                                0x88, 0x77, 0x66, 0x55,
                                                0x44, 0x33, 0x22, 0x11,
                                                0x11, 0x22, 0x33, 0x44,
                                                0x55, 0x66, 0x77, 0x88
                                              };

    unsigned char buf[100];
    size_t        n;

    /* a lone character is a count of one; counts may run to two digits */
    n = pack(buf, "cq12c*q", 0xAB, 0x0102030405060708ull,
             0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 2, qdata);
    if (n != sizeof(expected) || memcmp(buf, expected, n) != 0)
    {
      printf("Failure: pack() quads and counts (%zu)\n", n);
      goto Failure;
    }
  }

  printf("test: unpack widens with zero or sign extension\n");

  {
    /* every source and destination size unpack accepts: bytes of all ones
     * read as all ones when unsigned and -1 when signed */
    static const struct
    {
      const char *single;
      const char *array;
      int         srcbytes;
      int         dstbytes;
      int         is_signed;
    }
    combos[] =
    {
      { "bC", "*bC", 1, 1, 0 }, { "bc", "*bc", 1, 1, 1 },
      { "bS", "*bS", 1, 2, 0 }, { "bs", "*bs", 1, 2, 1 },
      { "bI", "*bI", 1, 4, 0 }, { "bi", "*bi", 1, 4, 1 },
      { "bQ", "*bQ", 1, 8, 0 }, { "bq", "*bq", 1, 8, 1 },
      { "hS", "*hS", 2, 2, 0 }, { "hs", "*hs", 2, 2, 1 },
      { "hI", "*hI", 2, 4, 0 }, { "hi", "*hi", 2, 4, 1 },
      { "hQ", "*hQ", 2, 8, 0 }, { "hq", "*hq", 2, 8, 1 },
      { "wI", "*wI", 4, 4, 0 }, { "wi", "*wi", 4, 4, 1 },
      { "wQ", "*wQ", 4, 8, 0 }, { "wq", "*wq", 4, 8, 1 },
      { "dQ", "*dQ", 8, 8, 0 }, { "dq", "*dq", 8, 8, 1 },
    };

    unsigned char ones[16];
    unsigned char out[24];
    int           i;
    int           j;
    size_t        n;
    uint64_t      want;
    uint64_t      got;
    int           k;

    memset(ones, 0xFF, sizeof(ones));

    for (i = 0; i < (int) NELEMS(combos); i++)
    {
      want = (combos[i].is_signed || combos[i].srcbytes == 8)
           ? UINT64_MAX : (1ull << (8 * combos[i].srcbytes)) - 1;
      if (combos[i].dstbytes < 8)
        want &= (1ull << (8 * combos[i].dstbytes)) - 1;

      /* single then array mode: two elements, then guard bytes */
      for (j = 0; j < 2; j++)
      {
        memset(out, 0xAA, sizeof(out));
        if (j == 0)
          n = unpack(ones, combos[i].single, out) +
              unpack(ones, combos[i].single, out + combos[i].dstbytes);
        else
          n = unpack(ones, combos[i].array, 2, out);

        if (n != (size_t) (2 * combos[i].srcbytes) ||
            out[2 * combos[i].dstbytes] != 0xAA)
        {
          printf("Failure: unpack() \"%s\" size\n",
                 j ? combos[i].array : combos[i].single);
          goto Failure;
        }

        for (k = 0; k < 2; k++)
        {
          got = 0;
          memcpy(&got, out + k * combos[i].dstbytes, combos[i].dstbytes);
          if (got != want)
          {
            printf("Failure: unpack() \"%s\" gave %llx\n",
                   j ? combos[i].array : combos[i].single,
                   (unsigned long long) got);
            goto Failure;
          }
        }
      }
    }
  }

  printf("test: unpack byte order prefixes\n");

  {
    static const unsigned char be[2] = { 0x12, 0x34 };

    unsigned short le_s, be_s, native_s;

    if (unpack(be, "<S", &le_s) != 2 || unpack(be, ">S", &be_s) != 2 ||
        unpack(be, "S", &native_s) != 2 ||
        le_s != 0x3412 || be_s != 0x1234 || native_s != 0x3412)
    {
      printf("Failure: unpack() byte order\n");
      goto Failure;
    }
  }

  return result_TEST_PASSED;

Failure:

  return result_TEST_FAILED;
}
