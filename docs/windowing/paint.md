# [DPTLib](https://github.com/dpt/DPTLib) > wuss > paint

Paint is a basic painting task in the `wuss` demo. It is also a worked example
of a task that draws into its own bitmap, captures the mouse during a stroke
and saves and loads PNG files.

- `libraries/wuss/test/tasks/paint-doc.c`: the canvas, the drawing operations
  and undo. It has no wuss dependency, so `paintdoc_test` runs headless.
- `libraries/wuss/test/tasks/paint.c`: the windows, the toolbox and input.

## Using it

Run `wuss -t Paint` (or pick Paint from the demo's icon bar) and click the
icon. The New dialogue opens above the icon bar. Menu > New... on the icon
bar opens the same dialogue.

- **Width** and **Height** are 1 to 2048 pixels, starting at 640 x 480. A
  value out of range is clamped when you click Create.
- **Background** is white, black or grey. The pen starts in a colour that
  shows on it.
- **Create** opens two windows: the canvas and a toolbox. Adjust-click Create
  keeps the dialogue open, so you can open several canvases in a row.
  **Cancel** closes the dialogue.

Closing the canvas closes the toolbox and ends that instance. Each Create
makes an instance of its own.

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

The canvas is a `bgrx8888` bitmap of the size and colour the New dialogue
asks for. The window shows up to 400 x 300 of it and scrolls. Drawing is
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
