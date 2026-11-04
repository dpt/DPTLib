# [DPTLib](https://github.com/dpt/DPTLib) > framebuf > wu

`wu` builds an optimised palette for a deep (`rgbx8888` or `rgba8888`) bitmap
using a variant of Xiaolin Wu's greedy orthogonal bipartition (1991). It
usually gives lower error than [octree](octree.md), and uses more memory.
Pair it with [dither](dither.md) to reduce an image to the palette.

## API

```C
#define wu_MIN_COLOURS (2)
#define wu_MAX_COLOURS (256)

result_t wu_make_palette(const bitmap_t *src,
                         int             ncolours,
                         colour_t       *palette,
                         int            *nfound);
```

`palette` must have room for `ncolours` entries. Each entry is the mean of the
pixels it represents. Alpha is ignored and every entry is opaque.

Colours with the same top five bits of each channel share a histogram bin and
always merge. The palette holds `ncolours` entries or one per occupied bin,
whichever is fewer. An entry is exactly a colour of the image only if that
colour is alone in its bin. Use [octree](octree.md) when an image with few
colours must keep them exactly.

Errors: `result_BAD_ARG` for an unsupported format or palette size,
`result_OOM` on allocation failure and `result_TOO_BIG` when the image has too
many pixels for the channel sums to fit 32 bits.

## How it works

**Histogram.** Each pixel adds to one of 32×32×32 bins: a count and red, green
and blue sums. This costs no division.

**Cumulative tables.** Running totals along each axis turn the histogram into
a table where a cell holds the total of every bin at or below it. Eight
lookups then total any box of bins.

**Cutting.** A box's squared error is the sum of its pixels' squares less
(Σr² + Σg² + Σb²) / count. For each axis and each plane through the box, a cut
gives two groups. The best cut has the largest sum of that term over the two
groups. The whole image starts as one box. Each step splits the box whose best
cut removes the most error.

**Palette.** Each box's entry is the mean of its pixels.

## Difference from Wu's paper

The paper splits the box with the largest variance, which needs a further
table of pixel squares. This code splits the box whose best cut removes the
most error, which is the greedy choice for total error. The term above gives
that without pixel squares, so that table goes.

## Costs

The tables take about 575 KB during the call. A palette of _n_ entries costs
at most 2 × 2 × 3 × 31 floating-point divisions per split. Real boxes are much
smaller than the whole space, so real counts are well below this. The count
depends on the palette size, not the image size.
