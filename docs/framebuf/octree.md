# [DPTLib](https://github.com/dpt/DPTLib) > framebuf > octree

`octree` builds an optimised palette for a deep (`rgbx8888` or `rgba8888`)
bitmap using the Gervautz–Purgathofer octree quantiser. Pair it with
[dither](dither.md) to reduce an image to the palette.

## API

```C
#define octree_MIN_COLOURS (2)
#define octree_MAX_COLOURS (256)

result_t octree_make_palette(const bitmap_t *src,
                             int             ncolours,
                             colour_t       *palette,
                             int            *nfound);
```

`palette` must have room for `ncolours` entries. An image with no more than
`ncolours` distinct colours yields exactly those colours. Otherwise the
palette holds exactly `ncolours` entries. Each entry is the mean of the
pixels it represents. Alpha is ignored and every entry is opaque.

Errors: `result_BAD_ARG` for an unsupported format or palette size,
`result_OOM` on allocation failure and `result_TOO_BIG` when the image has
too many pixels for the channel sums to fit an `unsigned long`.

## How it works

The tree has eight levels. At level _n_ a pixel takes the child picked by bit
7−_n_ of its red, green and blue values, so each node covers a cube of RGB
space and its children split that cube into octants. Every node on a pixel's
path adds the pixel to its channel sums and count. A node therefore always
knows the mean of all the pixels below it.

Leaves are the palette entries. Each internal level keeps a list of its
nodes, which are the candidates for reduction.

**Folding.** A fold turns the deepest reducible node with the fewest pixels
into a leaf, removing its children. Choosing the deepest node merges the most
similar colours. Choosing the smallest one there sacrifices the fewest
pixels.

**During insertion** the tree is folded whenever it holds more than 256
leaves. This bounds memory to a few thousand nodes whatever the image.

**After insertion** the tree is reduced to `ncolours`. A fold removes up to
seven leaves, so it can overshoot. Where it would, the node's two smallest
children are merged instead, removing exactly one leaf. That merged colour
sits in a slot later pixels could no longer reach, so this step is only safe
once every pixel is in. It is what makes the count exact.
