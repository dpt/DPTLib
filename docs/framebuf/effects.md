# [DPTLib](https://github.com/dpt/DPTLib) > framebuf > effects

"effects" is a sub-library of DPTLib for applying whole-bitmap effects. It was ported from the sprite effects in AppEngine (PrivateEye), moving them from RISC OS sprites to `bitmap_t`.

- It provides blur, sharpen, blend, grey, clear, expand, equalise and tone mapping.
- It works on the 32bpp formats: `rgbx8888`, `bgrx8888`, `xbgr8888`, `xrgb8888`, `rgba8888`, `bgra8888`, `abgr8888` and `argb8888`.
- Channel order is taken from the pixel format.
- Blur and sharpen use fixed-point lookup tables, with unrolled inner loops for kernels of up to three stages.

## Using an effect

The caller supplies a destination bitmap of the same size and format as the source. Source and destination can be the same bitmap.

```C
result_t effects_blur_apply(const bitmap_t       *src,
                            bitmap_t             *dst,
                            effects_blur_method_t method,
                            int                   amount);
```

Every function returns `result_BAD_ARG` if the bitmaps differ in size or format or if the format is unsupported. Rows must be a whole number of 32-bit words (`rowbytes` a multiple of 4).

Apart from `effects_blend` and `effects_tonemap_apply`, the fourth byte of each pixel (alpha or padding) is copied unchanged.

| Function                 | Effect                                                   |
|--------------------------|----------------------------------------------------------|
| `effects_blur_apply`     | Box or Gaussian blur, `amount` 2 to 48                   |
| `effects_sharpen_apply`  | Fixed sharpening kernel                                  |
| `effects_blend`          | Weighted mix of two bitmaps, alpha 0 to 256              |
| `effects_clear_apply`    | Fill the colour channels, keeping alpha                  |
| `effects_grey_apply`     | Luma greyscale (libjpeg weights)                         |
| `effects_expand_apply`   | Stretch the used levels to the full range                |
| `effects_equalise_apply` | Equalise each channel's histogram                        |
| `effects_tonemap_apply`  | Per-channel lookup from a `tonemap_t` (see below)        |

## Blur

Kernels are separable. A blur runs along each row and then down each column, replicating edge pixels. `amount` is clamped to `effects_blur_MIN_AMOUNT` (2, a 5-point kernel) to `effects_blur_MAX_AMOUNT` (48, a 95-point kernel).

The kernel is held as integer weights in units of 1/256 which sum to exactly 256, so a flat colour passes through unchanged at every `amount`.

## Tone maps

`tonemap_t` (`framebuf/tonemap.h`) holds a 256-entry table for each of red, green, blue and optionally alpha. Each table is built from gamma, brightness, contrast, midpoint, bias and gain, with optional inversion and reflection. Tables are built when first needed and cached until the next change.

```C
tonemap_t *map = tonemap_create();
tonemap_spec_t spec;

tonemap_get_values(map, tonemap_CHANNEL_RGB, &spec);
spec.gamma = 150;
tonemap_set(map, tonemap_CHANNEL_RGB, &spec);

effects_tonemap_apply(&src, &dst, map);
tonemap_destroy(map);
```
