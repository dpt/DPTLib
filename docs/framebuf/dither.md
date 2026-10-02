# [DPTLib](https://github.com/dpt/DPTLib) > framebuf > dither

`dither` reduces a deep (`rgbx8888` or `rgba8888`) bitmap to a paletted one
using error diffusion. Each pixel is mapped to its nearest palette entry and
the difference, the quantisation error, is spread over neighbouring pixels
not yet visited.

## API

```C
typedef struct dither_options
{
  dither_method_t method;
  const colour_t *palette;
  int             ncolours;   /* 1 to 256 */
  int             strength;   /* 0 to 100 */
  int             serpentine;
}
dither_options_t;

result_t dither_bitmap(const bitmap_t         *src,
                       const dither_options_t *opts,
                       bitmap_t              **dst);
```

`dither_bitmap` allocates the result. The output format is the smallest
paletted format which holds `ncolours` entries: `p1` for up to 2, `p2` for up
to 4, `p4` for up to 16 and `p8` beyond. The output palette is `palette`
padded with opaque black to the format's full size. The caller frees the
result's `base`, its `palette` and the bitmap itself.

Alpha is ignored. To build a palette suited to the image, see
[octree](octree.md).

## Kernels

Weights apply to the pixels right of and below the current pixel `*`, and
sum to the divisor except for Atkinson.

| Method                       | Divisor | Row 0       | Row 1       | Row 2       |
|------------------------------|--------:|-------------|-------------|-------------|
| `dither_NONE`                |       - | -           | -           | -           |
| `dither_FLOYD_STEINBERG`     |      16 | `* 7`       | `3 5 1`     |             |
| `dither_JARVIS_JUDICE_NINKE` |      48 | `* 7 5`     | `3 5 7 5 3` | `1 3 5 3 1` |
| `dither_STUCKI`              |      42 | `* 8 4`     | `2 4 8 4 2` | `1 2 4 2 1` |
| `dither_ATKINSON`            |       8 | `* 1 1`     | `1 1 1`     | `1` (below) |
| `dither_BURKES`              |      32 | `* 8 4`     | `2 4 8 4 2` |             |
| `dither_SIERRA`              |      32 | `* 5 3`     | `2 4 5 4 2` | `2 3 2`     |

Atkinson's six weights sum to 6/8, so it drops a quarter of the error. That
gives its typical higher contrast and loss of detail in the darkest and
lightest areas.

## Serpentine scanning

With `serpentine` set, odd rows run right to left. The kernel is mirrored by
negating each weight's horizontal offset, not by reversing the coefficient
rows, so off-centre kernels such as Atkinson stay correct. Serpentine order
avoids the diagonal "worm" artefacts that plain raster order produces.

## Strength

`strength` is the percentage of the error diffused. 100 is the full kernel;
0 matches `dither_NONE`. Intermediate values give a softer dither with more
banding.

## Implementation notes

- Errors are integers, kept per channel in sRGB. Each stored error is
  `error * weight * strength` and is divided by `divisor * 100`, with
  rounding, only when read. This avoids accumulating rounding loss.
- The error buffer holds one row per kernel row, used circularly, with two
  padding columns on each side to absorb error that falls off the image.
- Nearest-colour lookup uses [pixelmap](pixelmap.md). Its table quantises the
  source to a few bits per channel, so the "nearest" entry is approximate
  for close palette colours.
