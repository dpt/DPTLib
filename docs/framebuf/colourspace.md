# [DPTLib](https://github.com/dpt/DPTLib) > framebuf > colourspace

`colourspace` converts colours between sRGB and the CMYK, HSL, HSV and OKLab
models. It is the maths behind Wuss's true-colour colour picker, but has no
dependence on Wuss.

## Design

**Hub and spoke.** Each model has one struct of doubles and one pair of
functions converting it to and from `colourspace_rgb_t`, an sRGB triple. There
are no direct conversions between other models: to go from HSV to OKLab,
convert HSV to RGB, then RGB to OKLab. Adding a model costs two functions, not
one per existing model.

**Doubles, not bytes.** Working values are doubles so that a caller holding a
colour in one model (say a hue at zero saturation) can convert through RGB and
back without losing it. Only `colourspace_to_colour` quantises, to 8 bits per
channel.

**No clamping on the way through.** RGB values outside 0..1 pass through
every conversion unchanged, so an out-of-gamut OKLab colour converts to RGB and
back again intact. `colourspace_to_colour` clamps each channel to 0..1. That
is a plain per-channel clip, which can shift hue for colours far outside the
gamut; reducing chroma towards grey until the colour fits would be the
upgrade.

## Ranges

| Model | Channels | Range |
| ----- | -------- | ----- |
| RGB   | r, g, b  | 0..1 (sRGB, gamma-encoded) |
| CMYK  | c, m, y, k | 0..1 |
| HSL   | h; s, l  | 0..360 degrees; 0..1 |
| HSV   | h; s, v  | 0..360 degrees; 0..1 |
| OKLab | l; a, b  | 0..1; about -0.4..0.4 for sRGB colours |

Hue wraps on input, so 480 is 120. Greys give hue and saturation 0.

## Models

**CMYK** is the naive formula: K = 1 - max(R, G, B), then each of C, M and Y
is that channel's shortfall from 1 - K, divided by 1 - K. Black is all K. There
is no ink, dot gain or ICC profile modelling.

**OKLab** is [Björn Ottosson's](https://bottosson.github.io/posts/oklab/),
computed from linear sRGB. The sRGB transfer function is extended to negative
values by odd symmetry, so out-of-gamut colours survive a round trip.

## Hex colours

`colourspace_parse_hex` accepts `RRGGBB` or `RGB`, either with an optional
leading `#`, in any case. The short form expands each digit, so `#f80` is
`#ff8800`.

## Tests

`colourspace` in `DPTLibTest`: every model round-trips a coarse RGB grid back
to the same 8-bit colour; OKLab matches published values for white, red, green
and blue; an out-of-gamut OKLab colour survives a round trip; hex parsing
accepts and rejects the expected forms.
