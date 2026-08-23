# [DPTLib](https://github.com/dpt/DPTLib) > framebuf > composite

"composite" is a sub-library of DPTLib for performing [Porter-Duff bitmap compositing](https://keithp.com/~keithp/porterduff/p253-porter.pdf). It provides efficient alpha blending operations between source and destination bitmaps using mathematically defined compositing rules.

Porter-Duff compositing is a technique for combining images with transparency, allowing for sophisticated blending effects used in graphics applications, image editors, and rendering systems.

- It supports 12 standard Porter-Duff compositing rules.
- It works with RGBA and BGRA pixel formats with alpha channels.
- It's optimised for cases where pixels are fully transparent or fully opaque.
- Its performance should be efficient for most compositing scenarios.

## Setup

#### Prepare your bitmaps:

The bitmaps must have alpha channels. Currently supported formats are:

- `pixelfmt_rgba8888`
- `pixelfmt_bgra8888`

Both source and destination bitmaps should be properly initialised before compositing.

## Compositing

Use `composite()` to blend bitmaps according to Porter-Duff rules:

```C
result_t composite(composite_rule_t rule,
                   const bitmap_t  *src,
                   bitmap_t        *dst);
```

It requires a compositing rule, source bitmap, and destination bitmap. Every pixel in the source is composited with the corresponding pixel in the destination according to the chosen rule, with results written back to the destination bitmap.

## Compositing Rules

The following Porter-Duff rules are available:

- `composite_RULE_CLEAR`
  - [0, 0]
- `composite_RULE_SRC`
  - [Sa, Sc]
- `composite_RULE_DST`
  - [Da, Dc]
- `composite_RULE_SRC_OVER`
  - [Sa + Da·(1 – Sa), Sc + Dc·(1 – Sa)]
- `composite_RULE_DST_OVER`
  - [Da + Sa·(1 – Da), Dc + Sc·(1 – Da)]
- `composite_RULE_SRC_IN`
  - [Sa·Da, Sc·Da]
- `composite_RULE_DST_IN`
  - [Da·Sa, Dc·Sa]
- `composite_RULE_SRC_OUT`
  - [Sa·(1 – Da), Sc·(1 – Da)]
- `composite_RULE_DST_OUT`
  - [Da·(1 – Sa), Dc·(1 – Sa)]
- `composite_RULE_SRC_ATOP`
  - [Da, Sc·Da + Dc·(1 – Sa)]
- `composite_RULE_DST_ATOP`
  - [Sa, Dc·Sa + Sc·(1 – Da)]
- `composite_RULE_XOR`
  - [Sa + Da – 2·Sa·Da, Sc·(1 – Da) + Dc·(1 – Sa)]

## Limitations

- The 'destination out' rule can presently suffer with some distortions.
- Only RGBA and BGRA 32-bit formats are supported - no support for other colour depths or formats.
- No support for different sized source and destination bitmaps - they must be the same dimensions.
- No clipping or offset compositing - the entire source is composited onto the destination.
