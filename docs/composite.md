DPTLib > framebuf > composite
=============================
"composite" is a sub-library of DPTLib for performing [Porter-Duff bitmap compositing](https://keithp.com/~keithp/porterduff/p253-porter.pdf).

The code is written to make use of degenerate cases where pixels are fully transparent or fully opaque.

Compositing
-----------

Given input source `src` and destination `dst` bitmaps every pixel is composited according to the chosen `rule` and written back to `dst`.

The bitmaps given must have alpha channels: `pixelfmt_rgba8888` and `pixelfmt_bgra8888` formats are currently supported.

``` C
result_t composite(composite_rule_t rule,
                   const bitmap_t  *src,
                   bitmap_t        *dst);
```

Rules
-----

* `composite_RULE_CLEAR`
  * [0, 0]
* `composite_RULE_SRC`
  * [Sa, Sc]
* `composite_RULE_DST`
  * [Da, Dc]
* `composite_RULE_SRC_OVER`
  * [Sa + Da·(1 – Sa), Sc + Dc·(1 – Sa)]
* `composite_RULE_DST_OVER`
  * [Da + Sa·(1 – Da), Dc + Sc·(1 – Da)]
* `composite_RULE_SRC_IN`
  * [Sa·Da, Sc·Da]
* `composite_RULE_DST_IN`
  * [Da·Sa, Dc·Sa]
* `composite_RULE_SRC_OUT`
  * [Sa·(1 – Da), Sc·(1 – Da)]
* `composite_RULE_DST_OUT`
  * [Da·(1 – Sa), Dc·(1 – Sa)]
* `composite_RULE_SRC_ATOP`
  * [Da, Sc·Da + Dc·(1 – Sa)]
* `composite_RULE_DST_ATOP`
  * [Sa, Dc·Sa + Sc·(1 – Da)]
* `composite_RULE_XOR`
  * [Sa + Da – 2·Sa·Da, Sc·(1 – Da) + Dc·(1 – Sa)]

Limitations
-----------
The 'destination out' rule can presently suffer with some distortions.

