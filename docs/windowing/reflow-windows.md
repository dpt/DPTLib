# [DPTLib](https://github.com/dpt/DPTLib) > wuss > reflowable windows

A *reflowable* window lays its content out as a grid of equal cells and
re-wraps that grid to fit whatever width the user gives it. It never shows a
horizontal scrollbar. Two tasks in the `wuss` demo are of this kind:

- **Filer** (`libraries/wuss/test/tasks/filer.c`): one icon-and-name cell per
  directory entry, laid out by `wuss_gridview` (`libraries/wuss/gadget/gridview.c`).
  Any column count that fits is used.
- **Chars** (`libraries/wuss/test/tasks/chars.c`): one cell per codepoint of a
  256-codepoint page. Only power-of-two column counts are used, so a page
  divides into whole rows.

This document lists what the two share, says which parts are implemented and
which are not, and proposes where the shared parts should live. It is a
design note: where it says "should", the behaviour is not yet in the code.

## Terms

- **Quantum** — the width of one cell (`cell_w`). The content area of a
  reflowable window is a whole number of quanta wide, plus a fixed inset.
- **Natural width** — the width at which every item sits in one row, or the
  task's preferred starting width if that is smaller.
- **Layout width** — the content width for a given column count `n`:
  `n * cell_w + 2 * wuss_STD_INSET` (Chars) or `n * cell_w` (gridview).
- **Largest layout width** — the layout width for the largest column count
  that can ever be shown on the current screen.
- **Screen content ceiling** — the largest content width any window can have
  on the current screen, after outline and scrollbar carve are subtracted.
  Core computes it as `wuss__max_content_anywhere_on_screen`; it is private
  to `libraries/wuss/core`.

## Common behaviour

### 1. The content is a grid of fixed-size cells

Cell size is fixed by the font and the item type, not by the item contents.
Gridview measures it once at create time from the largest glyph and a fixed
number of label characters, so one long name cannot inflate the grid later.
Chars measures it from the shown font's cell metrics plus the system font's
index text, and re-measures it when the font changes.

Items are placed row-major: item `i` is at column `i % columns` and row
`i / columns`.

### 2. The column count is derived from the width

On every `wuss_EVENT_OPEN` (the window moved or resized) the task re-reads
its content width and recomputes the column count:

| Task | Rule |
|------|------|
| Filer | `MAX(content_w / cell_w, 1)` |
| Chars | largest power of two `n` such that `n * cell_w + 2 * inset <= content_w`, at least 1 and at most one row of a page |

Row count follows from the column count, and the task then updates its
document extent (`wuss_window_set_doc`) and invalidates the visible content.
It does not resize the window. If the column count did not change, nothing
is done.

Hit-testing and drawing both use the same cell geometry, so what is drawn is
what is hit.

### 3. No horizontal scrollbar; the window can always be dragged wider

The window is created without `wuss_WINDOW_HSCROLL`. Because there is no
scrollbar, a window narrower than the layout would lose content, so the
layout wraps instead.

Core caps a resize-drag at the document extent (`doc`, see `docs/windowing/wuss.md`
"Windows"). For the user to be able to drag a window wider than its current
layout, `doc.w` must therefore be the *largest layout width*, not the current
one. Both tasks do this:

- Gridview sets `doc.w = count * cell_w`, the ceiling at which every item is
  in one row.
- Chars sets `doc.w` to the width of the widest layout the screen can show,
  from the screen width.

`doc.h` is the height of the current layout (`rows * cell_h`), so the vertical
scrollbar shows the right proportion.

### 4. Window width is meant to be a multiple of the quantum

The intent: if ten cells fit, the content width is `10 * cell_w` (plus any
fixed inset), so there is no ragged blank strip at the right. A drag should
snap to the nearest layout width at or below the pointer, and a toggle-size
or a screen change should do the same.

**Not implemented.** Core resizes to any pixel width. Both tasks wrap
correctly at any width, but leave a blank strip up to one quantum wide
(for Chars up to one *doubling* wide, which is much more). The only snapping
is that a task resizes itself to its natural width when it changes font or
page (Chars `chars_resize`).

### 5. Opening: size to the natural layout, capped to the screen

When a window is opened it should take the natural layout width:

1. Compute the screen content ceiling.
2. From it, derive the largest column count that fits and the largest layout
   width.
3. Choose the starting column count: the task's preferred count (Chars: 16;
   Filer: enough for a comfortable window), capped by the largest.
4. Create the window at that layout width and with `doc.w` set to the
   largest layout width (rule 3).

**Partly implemented.** Chars caps its starting columns by `screen.w` and
rewraps once after creation. Filer opens at a fixed 320x240 and relies on the
first `wuss_EVENT_OPEN` to reflow. Neither uses the exact screen content
ceiling, because it is not exposed outside core; `screen.w` is a safe upper
bound that over-estimates by the outline and carve.

Note that `wuss_EVENT_OPEN` fires from inside `wuss_window_create_placed`,
before the caller has the `wuss_window_t *`. A handler that tests
`window == task->window` cannot act on that first event. Either set the
window pointer first (not possible with the current API) or rewrap
explicitly after create, as Chars does.

### 6. Screen resolution change

`wuss_resize` moves every window back on-screen and shrinks any that no
longer fit. It never grows one, and it does not tell the task anything other
than the resulting `wuss_EVENT_OPEN` for a window it moved or resized.

What a reflowable window needs after a resolution change:

- **Shrunk:** the `wuss_EVENT_OPEN` it receives already causes a rewrap.
- **Screen grew:** the *largest layout width* is now larger than `doc.w`, so
  the window cannot be dragged to use the extra room. The task must recompute
  `doc.w`. No event tells it to.
- **Screen shrank so the window did not need to move:** no event is raised,
  but `doc.w` is now larger than the screen can show. This is harmless
  (core caps the resize), but a stale `doc.w` also skews the scrollbar if
  `wuss_WINDOW_HSCROLL` is ever turned on.

**Not implemented.** Neither task recomputes `doc.w` after creation, except
Chars on font and page changes, and gridview on `wuss_gridview_set_count`.
The proposed fix is in "Proposed shared helper".

### 7. Furniture and font changes

The maximum content width depends on outline width, scrollbar carve and
titlebar height. The cell size depends on the system font (for index text
and labels) and on the shown font (Chars). A change to any of these
invalidates the largest layout width, the cell size, or both.

`wuss_set_config` can change furniture colours and sizes; it broadcasts
`wuss_EVENT_PALETTE` to every task and invalidates the screen. It does not
re-flow windows, and the titlebar height is fixed at `wuss_create`. There is
no API to change the system font after create.

**Not implemented.** On `wuss_EVENT_PALETTE` a reflowable task should
re-measure the cell and recompute the layout as in rule 6. Gridview
deliberately fixes the cell for the gadget's lifetime, so a font change needs
a new `wuss_gridview_remeasure` (or destroy and create).

## Differences between the two

| | Filer | Chars |
|---|---|---|
| Item count | variable (directory contents) | fixed (one page) |
| Columns | any integer | power of two |
| Cell content | bitmap + label | index text + glyph + rulers |
| Cell size changes | never | with font |
| Doc width | `count * cell_w` | screen-derived |
| Gadget used | `wuss_gridview` | own code |

Chars cannot use `wuss_gridview` as it stands: the gridview draws a bitmap
and a label with its own cell painter and has no column-count policy hook.

## Prior art: PrivateEye `filerwin`

`PrivateEye/libs/appengine/gadgets/filerwin/filerwin.c` is the RISC OS WIMP
version of the same window. It agrees with the rules above on the cell, the
quantum, the column count and the missing horizontal scrollbar:

- The quantum is `object_width + hpad` and the inset is `hpad`, so a layout
  is `hpad + n * (object_width + hpad)` wide.
- `columns = CLAMP((width - hpad) / (object_width + hpad), 1, nobjects)`.
- The window has `VSCROLL` only.
- The extent is reset, and the window force-redrawn, only when the row or
  column count changes.

It differs from wuss in these ways.

**Open is a request, not a notification.** `layout()` runs from
`wimp_OPEN_WINDOW_REQUEST` *before* the handler calls `wimp_open_window`, so
the extent is already right when the WIMP clamps the window. In wuss,
`wuss_EVENT_OPEN` fires after the change, and during create it fires before
the task has its window. A pre-open event that lets the task veto or adjust
the new size would fix both, and would give core the place to snap the width
to the quantum (rule 4). `filerwin` could snap `open->visible.x1` at that
point but does not.

**Extent width uses both limits.** It is `hpad + maxcolumns * (object_width +
hpad)` where `maxcolumns` is limited by `nobjects` *and* the screen width
(`read_screen_dimensions`). `gridview` limits by item count only and Chars by
screen width only. The smaller of the two is right: more columns than items,
or than the screen can show, can never be used.

**Drawing is a callback.** `filerwin_redrawfn(redraw, x, y, index, selected,
opaque)` draws each cell, and `map()` calls it only for cells that intersect
the clip box. `gridview` takes an `item_fn` that returns a glyph and a label
and paints them itself, which is why Chars cannot use it. An optional
per-cell draw callback, and a column policy such as `pow2`, would let Chars
share `gridview`.

**Shared gaps.** `filerwin` has the same holes as the wuss tasks:

- It reads the screen size only inside `layout()`, and writes the extent only
  when the rows or columns change, so a screen mode change leaves the extent
  stale until the next resize (rule 6).
- It opens at a default 1024x512 and lays out afterwards, not at the natural
  size (rule 5).
- Font and furniture changes are not handled (rule 7).
- `filerwin_set_nobjects` does not relayout, where `wuss_gridview_set_count`
  does.
- With `nobjects == 0` the `CLAMP(columns, 1, nobjects)` has its minimum above
  its maximum.

**Features outside the layout.** `filerwin` also has rubber-band selection
with auto-scroll, Select and Adjust selection semantics, a Menu-click callback
and Adjust-close. The wuss Filer has none of these yet.

## Proposed shared helper

Everything in rules 2, 3, 5 and 6 depends only on `cell_w`, an item count and
a column policy. A small layout helper in `libraries/wuss/gadget/` (or
`core`, to reach the private screen ceiling) would serve both tasks:

```c
typedef struct wuss_reflow
{
  size2d_t cell;      /* quantum and row height */
  int      inset;     /* fixed margin each side of the grid */
  int      count;     /* items */
  int      max_cols;  /* page or count limit; 0 for none */
  int      pow2;      /* non-zero: columns restricted to powers of two */
  int      columns;   /* result of the last wuss_reflow_update */
}
wuss_reflow_t;

/* columns that fit content_w under the policy; at least 1 */
int  wuss_reflow_columns(const wuss_reflow_t *r, int content_w);

/* content width for a given column count */
int  wuss_reflow_width(const wuss_reflow_t *r, int columns);

/* recompute columns from the window's content width and set the document
 * extent: doc.w from the largest layout the screen content ceiling allows,
 * doc.h from the rows now needed. Returns non-zero if columns changed. */
int  wuss_reflow_update(wuss_reflow_t *r, wuss_window_t *window);
```

`wuss_reflow_update` would be called from `wuss_EVENT_OPEN`, from
`wuss_EVENT_PALETTE` after re-measuring, and once explicitly after create.
Gridview would hold one; Chars would hold one and drop `chars_cols_for_width`
and `chars_doc_size`.

## Core changes needed

These are the pieces the tasks cannot do themselves:

1. **Quantised resize.** A per-window `resize_quantum` (width step) and
   `resize_inset`, honoured by the resize-drag, toggle-size and
   `wuss_resize` clamps, so the content width is always
   `inset + n * quantum`. Core has the pointer position and the clamps; the
   task has only the width after the fact. Because the quantum can change
   (Chars on font change), it needs a setter, not a creation-time argument.
2. **Expose the screen content ceiling.** A public
   `wuss_window_get_max_content(window, &max)` over
   `wuss__max_content_anywhere_on_screen`, so tasks need not guess from
   `screen.w`.
3. **A relayout event.** Raised to every task with windows after
   `wuss_resize` and after `wuss_set_config`, saying the screen ceiling or
   furniture metrics changed. `wuss_EVENT_OPEN` is not enough: it fires only
   for windows core moved or resized.
4. **Documentation fix.** `docs/windowing/wuss.md` says `doc` and `min_doc`
   are set once at creation and immutable. `wuss_window_set_doc` exists and
   both tasks rely on it, so the text is out of date.
5. **Create-time event ordering.** Either defer `wuss_EVENT_OPEN` for a new
   window until `wuss_window_create_placed` has returned, or document that it
   arrives early.

## Test checklist for a reflowable window

- Opens with no blank strip at the right, at a width that is `inset + n * cell_w`.
- Dragging wider adds columns at each quantum boundary and stops at the
  largest layout width.
- Dragging narrower removes columns; at the minimum width there is one
  column, not zero.
- The window can be dragged wider after `wuss_resize` enlarges the screen.
- After `wuss_resize` shrinks the screen below the window's width the grid
  rewraps and nothing is clipped.
- A font change (where supported) re-measures and resizes to the natural
  layout.
- No horizontal scrollbar is ever shown.
- Hit-testing and drawing agree at every column count.
