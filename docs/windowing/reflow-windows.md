# [DPTLib](https://github.com/dpt/DPTLib) > wuss > reflowable windows

A *reflowable* window lays its content out as a grid of equal cells and
re-wraps that grid to fit whatever width the user gives it. It never shows a
horizontal scrollbar. Two tasks in the `wuss` demo are of this kind:

- **Filer** (`libraries/wuss/tasks/filing.c`): one icon-and-name cell per
  directory entry, laid out by `wuss_gridview` (`libraries/wuss/gadget/gridview.c`).
  Any column count that fits is used.
- **Chars** (`libraries/wuss/tasks/chars.c`): one cell per codepoint of a
  256-codepoint page. Only power-of-two column counts are used, so a page
  divides into whole rows.

This document lists what the two share and how core and `wuss_gridview`
support it. "Open points" lists what is not done.

## Terms

- **Cell width** (`cell_w`) — the width of one cell. The window opens a whole
  number of cells wide, plus a fixed inset.
- **Natural width** — the width at which every item sits in one row, or the
  task's preferred starting width if that is smaller.
- **Layout width** — the content width for a given column count `n`:
  `n * cell_w + 2 * inset`.
- **Largest layout width** — the layout width for the largest column count
  that can ever be shown: the smaller of the item count and what the screen
  allows.
- **Screen content ceiling** — the largest content width any window can have
  on the current screen, after outline and scrollbar carve are subtracted.
  `wuss_window_get_max_content` returns it.

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
| Filer | `MAX((content_w - 2 * inset) / cell_w, 1)`, at most the item count |
| Chars | largest power of two `n` such that `n * cell_w + 2 * inset <= content_w`, at least 1 and at most one row of a page |

`wuss_gridview_set_layout` sets the inset, an optional column limit and the
power-of-two policy. Row count follows from the column count, and the task
then updates its document extent (`wuss_window_set_doc`) and invalidates the
visible content. It does not resize the window. If neither the column count
nor the extent changed, nothing is done.

`wuss_EVENT_OPEN` is not sent at create, so a task lays out explicitly once
after create. `wuss_gridview_create` does this itself.

Hit-testing and drawing both use the same cell geometry, so what is drawn is
what is hit.

### 3. No horizontal scrollbar; the window can always be dragged wider

The window is created without `wuss_WINDOW_HSCROLL`. Because there is no
scrollbar, a window narrower than the layout would lose content, so the
layout wraps instead.

Core caps a resize-drag at the document extent (`doc`, see
`docs/windowing/wuss.md` "Windows"). For the user to be able to drag a window
wider than its current layout, `doc.w` must therefore be the *largest layout
width*, not the current one. It is the smaller of two limits, because more
columns than either can never be used:

- the item count (one row holding every item), and
- the screen content ceiling, from `wuss_window_get_max_content`.

Gridview and Chars both do this. `doc.h` is the height of the current layout
(`rows * cell_h`), so the vertical scrollbar shows the right proportion. With
no items the grid is one column and one row.

### 4. Window width follows the pointer

A resize-drag is not snapped to whole columns. The last column may be partly
blank until the window reaches a column boundary.

### 5. Opening: size to the natural layout, capped to the screen

When a window is opened it takes the natural layout width:

1. Take the screen content ceiling, `wuss_window_get_max_content`.
2. From it, derive the largest column count that fits and the largest layout
   width. Gridview does this when it lays out.
3. Choose the starting column count: the task's preferred count (Chars: 16;
   Filer: `FILING_COLUMNS`), capped by the item count and by what fits.
4. Create the window at that layout width, with `doc.w` set to the largest
   layout width (rule 3).

`wuss_gridview_natural_size(gv, columns, max_rows)` gives the content size for
step 3: `columns` wide (or one column per item if fewer) and as many rows as
the items need, up to `max_rows`.

Filer creates its window, creates the gridview, then resizes the window to the
natural size. A resize is capped to the screen from the window's top-left, so
it can stop part way across a column. Filer then resizes again to the whole
columns that fit. Chars caps its starting columns by the screen width and
rewraps once after create.

### 6. Screen resolution change

`wuss_resize` moves every window back on-screen, shrinks any that no longer
fit. It never grows a window. It then sends
`wuss_EVENT_RELAYOUT` to every window, whether or not core moved it.

On `wuss_EVENT_RELAYOUT` a task recomputes the screen content ceiling and so
the largest layout width and `doc.w`:

- **Shrunk:** the columns rewrap as for `wuss_EVENT_OPEN`.
- **Screen grew:** `doc.w` rises, so the window can be dragged to use the room.
- **Screen shrank with no move:** `doc.w` falls to match.

Gridview does this itself from `wuss_gridview_handle_event`. Chars does it
from its own handler, forcing a new extent.

### 7. Furniture and font changes

The maximum content width depends on outline width, scrollbar carve and
titlebar height. The cell size depends on the system font (for index text
and labels) and on the shown font (Chars).

`wuss_set_config` can change furniture colours and sizes. It broadcasts
`wuss_EVENT_PALETTE`, invalidates the screen and sends `wuss_EVENT_RELAYOUT`
to every window. There is no API to change the system font after create.

`wuss_gridview_remeasure` re-measures the cell, reflows and redraws. The
gadget calls it on `wuss_EVENT_PALETTE` and `wuss_EVENT_RELAYOUT`. Only
`wuss_EVENT_RELAYOUT` is certain to reach a window handler. The palette
broadcast may have no window, so a task that must react to it should forward
it by hand.

### 8. Redraw and selection touch only what changed

Gridview redraws only the rows the clip reaches. A selection change marks
only the cells that changed as dirty. A MENU click changes neither the
selection nor the display. An optional draw callback
(`wuss_gridview_set_draw`) replaces the glyph-and-label painter, and
`wuss_gridview_cell_box` gives a cell's document box.

## Differences between the two

| | Filer | Chars |
|---|---|---|
| Item count | variable (directory contents) | fixed (one page) |
| Columns | any integer | power of two |
| Cell content | bitmap + label | index text + glyph + rulers |
| Cell size changes | on `wuss_gridview_remeasure` | with font |
| Doc width | smaller of the item and screen limits | same |
| Gadget used | `wuss_gridview` | `wuss_gridview` |

Chars uses `wuss_gridview` through the draw callback, a fixed cell size, a
background override and the power-of-two policy.
It keeps its own mouse handling (page step, Menu).

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
`wuss_EVENT_OPEN` fires after the change, and not at all at create. Wuss
does not snap widths, so a pre-open event is not needed.

**Extent width uses both limits.** It is `hpad + maxcolumns * (object_width +
hpad)` where `maxcolumns` is limited by `nobjects` *and* the screen width
(`read_screen_dimensions`). `wuss_gridview` now does the same with
`wuss_window_get_max_content`.

**Drawing is a callback.** `filerwin_redrawfn(redraw, x, y, index, selected,
opaque)` draws each cell, and `map()` calls it only for cells that intersect
the clip box. `wuss_gridview` now has `wuss_gridview_set_draw` and culls to
the visible rows.

**Gaps in `filerwin`.**

- It reads the screen size only inside `layout()`, and writes the extent only
  when the rows or columns change, so a screen mode change leaves the extent
  stale until the next resize. Wuss has `wuss_EVENT_RELAYOUT` for this.
- It opens at a default 1024x512 and lays out afterwards, not at the natural
  size.
- Font and furniture changes are not handled.
- `filerwin_set_nobjects` does not relayout, where `wuss_gridview_set_count`
  does.
- With `nobjects == 0` the `CLAMP(columns, 1, nobjects)` has its minimum above
  its maximum.

**Features outside the layout.** `filerwin` also has rubber-band selection
with auto-scroll, Select and Adjust selection semantics, a Menu-click callback
and Adjust-close. The wuss Filer has none of these yet.

## Core and gadget API

| Item | Where |
|------|-------|
| `wuss_window_get_max_content(window, &max)` | `include/wuss/window.h` |
| `wuss_window_set_min_size(window, min)` (`wuss_KEEP` leaves an axis) | `include/wuss/window.h` |
| `wuss_window_set_doc` (changes `doc` after create) | `include/wuss/window.h` |
| `wuss_EVENT_RELAYOUT` | `include/wuss/task.h` |
| `wuss_gridview_set_layout`, `_set_draw` | `include/wuss/gadget/gridview.h` |
| `wuss_gridview_remeasure`, `_natural_size`, `_width_for_columns`, `_cell_box`, `_get_columns` | same |

`core/relayout.c` sends `wuss_EVENT_RELAYOUT` after `wuss_resize` and
`wuss_set_config`.

## Open points

- Filer has no rubber-band selection, Menu-click callback or Adjust-close.

## Test checklist for a reflowable window

- Opens with no blank strip at the right, at a width that is `inset + n * cell_w`.
- Dragging wider adds columns (at each column boundary) and
  stops at the largest layout width.
- Dragging narrower removes columns; at the minimum width there is one
  column, not zero.
- The window can be dragged wider after `wuss_resize` enlarges the screen.
- After `wuss_resize` shrinks the screen below the window's width the grid
  rewraps and nothing is clipped.
- A font change (where supported) re-measures and resizes to the natural
  layout.
- An empty window opens one column wide.
- No horizontal scrollbar is ever shown.
- Hit-testing and drawing agree at every column count.
- A selection change redraws only the changed cells; a MENU click redraws
  nothing.
