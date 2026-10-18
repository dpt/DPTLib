# [DPTLib](https://github.com/dpt/DPTLib) > geom > stage

"stage" is a sub-library of DPTLib for laying out the background of a document: a page inside a margin, with a stroked border and an optional drop shadow, floating on a pasteboard that fills the rest of the work area. The page stays centred as the work area grows.

It draws nothing. It emits a list of boxes, each tagged with what belongs there, and the caller fills them. This keeps it independent of any graphics system.

It was written for PrivateEye's document windows, where the work area is scrolled and resized by the window manager.

## Layout

```
+------------------------------------+
| pasteboard                         |
|   +--------------------------+     |
|   | stroke                   |     |
|   |  +--------------------+  |     |
|   |  | margin             |  |     |
|   |  |  +--------------+  |  |     |
|   |  |  |   content    |  |  |     |
|   |  |  +--------------+  |  |     |
|   |  +--------------------+  |     |
|   +--------------------------+ shadow
|     shadow                         |
+------------------------------------+
```

The stroke is drawn atop the outer edge of the margin, so it adds nothing to the page's footprint. The shadow is carved out of the pasteboard, so it does not either.

## Setup

#### Configure:

`stageconfig_init()` fills in the defaults: a shadow, a pasteboard of at least 16, a stroke of 1, a margin of 8 and a shadow offset of 4, all in pixels.

All sizes are in the caller's units. Set `log2_pixel_x` and `log2_pixel_y` to the log2 of the size of a device pixel in those units so that edges fall on whole pixels. They default to 0, for units that are pixels. RISC OS callers using OS units set them to 1.

Clear `stage_FLAG_SHADOW` in `flags` for no shadow.

## Measuring

`stage_get_fixed()` returns the width and height the stage needs whatever the page size: twice the margin plus the minimum pasteboard.

## Generating

```C
void stage_generate(const stageconfig_t *config,
                    int                  workarea_width,
                    int                  workarea_height,
                    int                  content_width,
                    int                  content_height,
                    int                 *min_workarea_width,
                    int                 *min_workarea_height,
                    stagebox_t          *boxes,
                    size_t              *nboxes);
```

`boxes` must hold `stage_MAX_BOXES` entries. On return `nboxes` is the number written.

- The first box is always the content (`stageboxkind_CONTENT`). The others run from the top left.
- Content is rounded down to whole pixels and the other dimensions are rounded up. The pasteboard absorbs the difference.
- If the work area is too small for the page and the minimum pasteboard, the layout uses the smallest size that fits. `min_workarea_width` and `min_workarea_height` return it (either may be NULL). Use them to set the document's extent.
- The boxes cover the whole of `min_workarea_width` by `min_workarea_height`. Boxes of the same kind may overlap, as at the corners of the stroked border.

Box kinds:

- `stageboxkind_PASTEBOARD`
- `stageboxkind_STROKE`
- `stageboxkind_MARGIN`
- `stageboxkind_CONTENT`
- `stageboxkind_SHADOW`

Call `stage_generate()` again whenever the work area or the page size changes.
