# [DPTLib](https://github.com/dpt/DPTLib) > wuss > paint

Paint is a basic painting task in the `wuss` demo. It is also a worked example
of a task that draws into its own bitmap, captures the mouse during a stroke
and saves and loads PNG files.

- `libraries/wuss/test/tasks/paint-doc.c`: the canvas, the drawing operations
  and undo. It has no wuss dependency, so `paintdoc_test` runs headless.
- `libraries/wuss/test/tasks/paint.c`: the windows, the toolbox and input.

## Using it

Run `wuss -t Paint` (or pick Paint from the demo's icon bar) and click the
icon. Two windows open: the canvas and a toolbox. Closing the canvas closes
the toolbox and ends the task.

- **Select** paints with the foreground colour, **Adjust** with the
  background colour. Clicking a swatch in the toolbox sets the foreground
  (Select) or the background (Adjust).
- Tools: brush, eraser (paints the background colour), line, rect and flood
  fill. Press `B` `E` `L` `R` or `F` to pick one. The Fill option makes rect
  solid. The slider sets the width, 1 to 32 pixels.
- Line and rect show a rubber band while the button is down. They draw into
  the canvas when it is released.
- `Ctrl-Z` or Menu > Undo steps back. There are 20 levels and no redo.
- Menu > Save PNG (`Ctrl-S`) opens the Save As dialogue.
- Drop a PNG on the canvas window or on the Paint icon to load it.

## The canvas

The canvas is a 640 x 480 `bgrx8888` bitmap, initially white. Drawing is
opaque and hard-edged. There is no alpha channel because `screen_t` has no
span table for the alpha formats, so drawing straight onto one asserts.

Every operation in `paint-doc.h` takes canvas pixels and returns the box it
may have changed. The task passes that box to `wuss_window_invalidate`.

Undo keeps RLE-compressed copies of the whole canvas. A stroke takes one
`paintdoc_checkpoint` when the button goes down. `paintdoc_rect` and
`paintdoc_fill` take their own.

## Loading

`paintdoc_load_png` replaces the canvas and resizes it to the image. Undo is
dropped, as a saved step cannot apply to a canvas of another size. An image
with an alpha channel is composited over white. A paletted image is converted
to `bgrx8888`, which loses any palette transparency. On failure the canvas is
left as it was.

## Open points

- The tool buttons are text labels. Sprites for them are not drawn yet.
- There is no dirty tracking: closing the canvas never asks to save.
- The pointer is the same over the canvas and the toolbox. A per-window
  pointer shape would let the canvas show a crosshair.
