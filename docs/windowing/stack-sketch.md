# Box-stack layout (design sketch)

Design notes for the `stack` module. **Implemented** as `include/geom/stack.h`
/ `libraries/geom/stack/`; kept here as the design record.

## Motivation

Laying out the *contents* of a window: icons, labels, buttons and fields
arranged into a dialog. This is client-side layout — the stuff a window's
owner draws inside the work area — not window furniture. `wuss`'s own
furniture (titlebar, close and toggle buttons, scrollbars) stays as the
per-item formulas in `libraries/wuss/furniture/*.c` and is out of scope for
this module.

Without something like this, every dialog hand-codes an inset chain per icon:
readable for three icons, a maintenance sink for thirty, and impossible to
reflow when the window resizes or the font metrics change. A box stack turns a
dialog into a nested description that the solver turns into boxes.

Not built yet. Build it when the first real dialog needs it.

## Relationship to `geom/layout`

There is already a `geom/layout.h` (`layout_place`). It is a **line-flow**
layout: an element list of boxes and `NEWLINE`s, placed by `geom/packer`,
wrapping like text. One `spacing` and one `leading` scalar for the whole spec,
no nesting, no flex.

This sketch is a different tool — a **box-stack solver**: nested HBox/VBox,
flex weights, cross-axis alignment, per-edge padding. The Monitor dialog below
(aligned label column, nested group box, spread button row) is not expressible
as line-flow. Hence a separate module with a separate name; `layout_place`
stays as-is for flowing text runs.

## Scope

In:

- Nested horizontal and vertical stacks.
- Per-item fixed size, or a flex weight for sharing leftover space.
- Per-item main-axis `min` / `max` clamps.
- Cross-axis alignment (start, centre, end, fill).
- Spacers (a flex item with no output consumer).
- Container inner padding (per edge) and inter-child gap.
- Integer geometry throughout, remainder pixels distributed so child extents
  sum exactly to the parent.

Out (add only when a real layout needs it, each its own follow-up):

- Shared column widths across independent rows (a label column where every
  field starts at the same x). Nested stacks solve each row alone, so the
  caller pre-measures: loop over the labels, find the widest, set every label
  leaf to that fixed `size`. It is a handful of lines at the call site and
  needs nothing from the module. If callers keep writing that loop, the clean
  fix is a `GRID` kind with a max-content track — real code, deferred until a
  caller exists.
- Grid with row/column spans and per-track sizing. Uniform grids are
  expressible as a stack of stacks; true 2D alignment across rows is not, and
  that is the trigger to add a real `GRID` kind.
- Linear constraints (`a.right == b.left + 8` solved by Cassowary-style
  simplex). Only if a layout genuinely cannot be nested into stacks.
- A retained node tree the caller mutates and re-solves. `wuss` already owns
  window state and dirty-region redraw; a second tree to keep in sync is not
  worth it.
- A `GROUP` kind that draws its own border and caption. A group box is a
  `VBOX` with a wider top `pad` for the caption; the border and caption text
  are draw-time work for the caller. The module computes rectangles, not
  pixels — no strings, no drawing.
- Chained cross-axis flex (a leaf's cross size driving a sibling's).

## Model

One-shot. The caller fills a flat array describing the tree, calls the solver,
and reads a parallel array of boxes. No allocation, no tree pointers, no
retained state. Rebuild the array each resize (or each frame) and solve again;
the solve is cheap.

The tree is expressed by parent index. `items[0]` is the root. Every other
item names its parent, which must appear earlier in the array (parents before
children). A container's children are every item naming it as parent, in array
order — that order is the stacking order.

```c
typedef enum stack_kind
{
  stack_KIND_HBOX,    /* stack children left to right               */
  stack_KIND_VBOX,    /* stack children top to bottom               */
  stack_KIND_SPACER,  /* flexible gap; no output box read by anyone */
  stack_KIND_LEAF     /* a thing the caller will position           */
}
stack_kind_t;

typedef enum stack_align
{
  stack_ALIGN_START,  /* against the low cross-axis edge   */
  stack_ALIGN_CENTRE, /* centred on the cross axis         */
  stack_ALIGN_END,    /* against the high cross-axis edge  */
  stack_ALIGN_FILL    /* span the container's cross extent */
}
stack_align_t;

typedef struct stack_item
{
  stack_kind_t  kind;
  int           parent;  /* index of the containing item; -1 for the root */

  int           size;    /* fixed main-axis extent in px; 0 means "use flex" */
  int           flex;     /* weight for sharing leftover main-axis space; 0 means "do not grow" */
  int           min;     /* main-axis lower clamp in px; 0 means none */
  int           max;     /* main-axis upper clamp in px; 0 means unbounded */

  stack_align_t align;   /* cross-axis placement of this item within its container */

  int           gap;     /* containers only: px between adjacent children */
  int           pad_l;   /* containers only: inner inset, left edge   */
  int           pad_t;   /* containers only: inner inset, top edge    */
  int           pad_r;   /* containers only: inner inset, right edge  */
  int           pad_b;   /* containers only: inner inset, bottom edge */
}
stack_item_t;

/* Solve `items[0..n)` into `out[0..n)`. `out[i]` is the computed box for
 * `items[i]`, in the same coordinate space as `root`. `root` is the area the
 * root item is laid into. Returns result_OK, or result_STACK_BAD_TREE if the
 * parent indices are not a valid parents-before-children forest rooted at 0. */
result_t stack_solve(const stack_item_t *items,
                     int                 n,
                     const box_t        *root,
                     box_t              *out);
```

`SPACER` gets an `out` box like any item; nothing is expected to read it. It
exists so "push the rest to the right" is `{ .kind = stack_KIND_SPACER,
.flex = 1 }` rather than a special case in the solver.

## Algorithm

Recurse over containers, root first. For each container, lay out its direct
children along its main axis; the container's own box is already known (the
root's is `root` inset by its four `pad_*`; a child container's was set when
its parent was processed).

Let `avail` be the container's main-axis extent, minus the two `pad_*` on that
axis, minus `gap * (childcount - 1)`.

**Pass 1 — fixed and minimum.** For each child, take its base main extent:
`size` if non-zero, else `min` if non-zero, else 0. Sum these into `used`.

**Pass 2 — distribute the remainder.** `slack = avail - used`.

- If `slack > 0`: share it across children with `flex > 0` in proportion to
  their weights. Integer division leaves a remainder of at most
  `flexchildren - 1` px; hand those out one per child, earliest first, so the
  children's extents sum to exactly `avail`. Apply each child's `max` clamp; px
  removed by a clamp go back into `slack` for another distribution pass over
  the still-unclamped flex children (at most a few passes; stop when no child
  clamps).
- If `slack < 0` (children's minimums overflow the container): the container
  is smaller than its contents. Do not shrink below `min`. Let children
  overflow the high edge. `min` is a hard floor; a caller that wants
  shrink-to-fit sets `min` to 0.
- If `slack == 0`: nothing to distribute.

**Placement.** Walk the children along the main axis from the container's low
edge plus the leading `pad_*` for that axis, advancing by each child's resolved
extent plus `gap`. On the cross axis, place each child within the container
inset by the two cross-axis `pad_*`, per its `align`: `START` at the low edge,
`END` at the high edge, `CENTRE` at `(container_cross - child_cross) / 2`
rounding down, `FILL` sets the child's cross extent to the whole span. A
non-`FILL` child's cross extent is its `size` if the cross axis has one to give
— otherwise `FILL` is the sensible default and a caller wanting an intrinsic
cross size supplies it out of band.

Write each child's `out` box, then recurse into it if it is a container.

## Worked example — the Monitor configuration dialog

The RISC OS dialog it is modelled on:

```
+------------------------------------------------+
|      Driver [ Vpod                        ][v] |
|     Monitor [ Samsung 23" (SAM0473)       ][v] |
|                                                |
| +- Monitor options --------------------------+ |
| | [ ] Use mode definition file               | |
| |    Monitor type [ Samsung SyncMaster  ]    | |
| |        Colours  [ 64 thousand         ][v] | |
| |      Resolution [ 1680 x 1050         ][v] | |
| |      DPMS level [ Monitor standby     ][v] | |
| +--------------------------------------------+ |
|                                                |
| [ Refresh ]                 [ Cancel ] [ Set ] |
+------------------------------------------------+
```

Structure:

- The root is a `VBOX`: two label/field rows, the group box, then the button
  row.
- **Aligned label column.** "Driver", "Monitor", "Monitor type", "Colours",
  "Resolution", "DPMS level" all right-align to one column so the fields start
  at a common x. The caller measures the six label strings, takes the widest as
  `LW`, and sets every label leaf to `.size = LW, .align = stack_ALIGN_END`.
  Rows solved independently still line up because the caller fed them one width.
- **Each row** is an `HBOX`: label leaf (`size = LW`), field leaf (`flex = 1`),
  and — only on rows that have one — a fixed-size pop-up icon. A row without the
  pop-up just lets the field run to the edge; alignment holds either way.
- **Group box** is a `VBOX` with `.pad_t` big enough for the caption and
  border, `.pad_l/r/b` the border inset. The caller draws the frame and the
  "Monitor options" text; the module only insets the children.
- **Button row** is an `HBOX`: `Refresh`, a `SPACER`, `Cancel`, a `SPACER`,
  `Set`. Two spacers spread the three buttons the way the dialog does.

Sketched as an array (row children elided after the first for length; `RH` row
height, `PU` pop-up width, `BW`/`BH` button size, `M`/`G` margin and gap,
`CAP` caption height):

```c
enum
{
  ROOT,
  DRV_ROW,  DRV_LBL,  DRV_FLD,  DRV_PU,
  MON_ROW,  MON_LBL,  MON_FLD,  MON_PU,
  GROUP,
  USE_ROW,  USE_OPT,                       /* option (check) icon + its label */
  TYP_ROW,  TYP_LBL,  TYP_FLD,             /* no pop-up on this row */
  COL_ROW,  COL_LBL,  COL_FLD,  COL_PU,
  RES_ROW,  RES_LBL,  RES_FLD,  RES_PU,
  DPM_ROW,  DPM_LBL,  DPM_FLD,  DPM_PU,
  BTN_ROW,  B_REFRESH, GAP1, B_CANCEL, GAP2, B_SET,
  N_ITEMS
};

stack_item_t items[N_ITEMS] =
{
  [ROOT]    = { .kind = stack_KIND_VBOX, .parent = -1,
                .pad_l = M, .pad_t = M, .pad_r = M, .pad_b = M, .gap = G },

  /* --- Driver row --- */
  [DRV_ROW] = { .kind = stack_KIND_HBOX, .parent = ROOT,    .size = RH, .gap = G, .align = stack_ALIGN_FILL },
  [DRV_LBL] = { .kind = stack_KIND_LEAF, .parent = DRV_ROW, .size = LW, .align = stack_ALIGN_END  },
  [DRV_FLD] = { .kind = stack_KIND_LEAF, .parent = DRV_ROW, .flex = 1,  .align = stack_ALIGN_FILL },
  [DRV_PU]  = { .kind = stack_KIND_LEAF, .parent = DRV_ROW, .size = PU, .align = stack_ALIGN_CENTRE },
  /* Monitor row: same shape, parent = ROOT */

  /* --- Monitor options group --- */
  [GROUP]   = { .kind = stack_KIND_VBOX, .parent = ROOT,   .flex = 1,
                .pad_l = M, .pad_t = CAP, .pad_r = M, .pad_b = M, .gap = G, .align = stack_ALIGN_FILL },
  /* USE_ROW .. DPM_ROW: HBOX rows, parent = GROUP.
   * TYP_ROW carries no *_PU child; its field runs to the group's right pad. */

  /* --- Button row --- */
  [BTN_ROW]  = { .kind = stack_KIND_HBOX,   .parent = ROOT,    .size = BH, .gap = G, .align = stack_ALIGN_FILL },
  [B_REFRESH]= { .kind = stack_KIND_LEAF,   .parent = BTN_ROW, .size = BW, .align = stack_ALIGN_FILL },
  [GAP1]     = { .kind = stack_KIND_SPACER, .parent = BTN_ROW, .flex = 1 },
  [B_CANCEL] = { .kind = stack_KIND_LEAF,   .parent = BTN_ROW, .size = BW, .align = stack_ALIGN_FILL },
  [GAP2]     = { .kind = stack_KIND_SPACER, .parent = BTN_ROW, .flex = 1 },
  [B_SET]    = { .kind = stack_KIND_LEAF,   .parent = BTN_ROW, .size = BW, .align = stack_ALIGN_FILL },
};

box_t out[N_ITEMS];

stack_solve(items, N_ITEMS, &work, out);
/* Every *_FLD box starts at work.x0 + M + LW + G, so the fields line up down
 * the whole dialog including the group. Resize the window and the fields
 * stretch; the labels, pop-ups and buttons keep their fixed widths. */
```

The hand-coded equivalent recomputes an inset chain per icon and repeats the
label-column arithmetic in every row. The array form measures the labels once
and reflows the rest for free.

## Notes for the implementer

- Follows the module split: `include/geom/stack.h` for the API,
  `libraries/geom/stack/` for the solver, one function per file where it reads
  naturally. Add sources by hand to `CMakeLists.txt`. Sits next to
  `geom/layout` (line-flow) and `geom/packer`.
- No dependency on `wuss` or SDL. `geom` (`box_t`, `point_t`, `size2d_t`) and
  `base/result.h` only. Testable as a plain core test: build an array, solve,
  assert boxes.
- Reserve a `result_BASE_STACK` block in `include/base/result.h` (0x0C00 is the
  next free base after `result_BASE_WUSS`); the only code needed at first is
  `result_STACK_BAD_TREE` (parent indices not a valid forest).
- Integer only. The rounding rule (remainder px to earliest children) is the
  one behaviour a test must pin: three flex-1 children in a 100px box get
  34 / 33 / 33, and the boxes abut with no gap or overlap. A second test pins
  the aligned-label pattern: two rows, label leaves both `size = LW`, assert
  the two field boxes share an `x0`.
- `wuss` need not depend on this. A window's owner calls `stack_solve` in its
  own redraw code and positions its icons from `out[]`; the module sits
  alongside `wuss`, not inside it.
```
