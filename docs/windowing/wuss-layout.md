# Wuss window layout style guide

Derived from the dialogues in `libraries/wuss/tasks/` (`display.c`,
`fireworks.c`, `saturn.c`, `orbing.c`) and `painting/painting-new.c`.

## Method

1. Describe the window as a `stack_item_t` table (`geom/stack.h`): never
   hand-place boxes with literal coordinates.
2. Copy the table to a local array, then fill in each `axis_size` that
   depends on text: `task_text_width()` for labels, `task_button_width()`
   for buttons (`common.h`).
3. `stack_smallest()` gives the window size; `stack_solve()` gives a box
   per item. Create the window at that size, then build icons from the
   boxes.

## Spacing

- Root is a `stack_KIND_VBOX` with `.gap = wuss_STD_GAP` and
  `.pad = wuss_STD_INSETS`.
- Every gap between siblings, vertical or horizontal, is `wuss_STD_GAP`.
- No private margin or row-pitch constants.

## Rows

- One `STACK_HBOX` per row, `wuss_STD_GAP` between its children, height
  set to the tallest child (`wuss_STD_SLIDER_HEIGHT`, `wuss_STD_GLYPH_SIZE`
  or the field height).
- Children are centred on the cross axis (`stack_ALIGN_CENTRE`).
- A row holds everything that belongs to it: a caption and its radios share
  one row, never a caption above a row.

## Labels

- A label is a leaf in the row's first column, flagged
  `wuss_ICON_FLAGS_JUSTIFY_RIGHT`, so the labels abut their controls.
- All labels in one dialogue share a size group (`STACK_LEAF_GROUP`), so
  the controls start at the same x. Each sets `axis_size` to its own text
  width; the group widens the rest.
- Buttons and sliders need no label column of their own: use
  `STACK_SPACER_GROUP` with the same group to align with it.

## Controls

- Radios and options: height `wuss_STD_GLYPH_SIZE`, width glyph plus
  `wuss_STD_GAP` plus text width.
- Sliders: `wuss_STD_SLIDER_HEIGHT`, flexible on the main axis.
- Writable fields: fixed width, centred text, field height from the font.

## Button row

- Last row of the root. `STACK_HBOX` with `stack_ALIGN_END`, height
  `wuss_STD_PRIMARY_BUTTON_HEIGHT`.
- First child is `STACK_SPACER(..., 1)` to push the buttons right.
- Then Cancel (`wuss_STD_SECONDARY_BUTTON_HEIGHT`), then the default
  button (`wuss_STD_PRIMARY_BUTTON_HEIGHT`), `wuss_STD_GAP` apart.
- Axis sizes from `task_button_width()` with
  `wuss_STD_SECONDARY_BUTTON_BORDER` / `wuss_STD_PRIMARY_BUTTON_BORDER`.

## Default button

- RETURN activates the dialogue's default button (`wuss_dialogue_handle_key`),
  so a default button only makes sense when something can hold the input
  focus: the dialogue must contain a writable icon.
- A dialogue with no writable icons, e.g. only sliders, must NOT have a
  default button. Its action buttons are all ordinary
  (`wuss_STD_SECONDARY_BUTTON_HEIGHT` and `_BORDER`, `wuss_ICON_FLAGS_DEFAULT`
  clear), and the button row is `wuss_STD_SECONDARY_BUTTON_HEIGHT` tall.
- Such a dialogue is also not focusable: call
  `wuss_dialogue_set_focusable(dialogue, 0)` after creating it.

## Cancel button

- ESCAPE activates the dialogue's Cancel button (`wuss_dialogue_handle_key`),
  as a Select click on it would. Name the button with
  `wuss_dialogue_set_cancel` after registering the actions.
- Like RETURN, it is a key event, so it reaches only a focusable dialogue.
  A dialogue with no writable icons is not focusable and so is cancelled
  with the mouse alone.

## Frames

Contents of a `wuss_ICON_TYPE_FRAME` use `wuss_STD_FRAME_INSETS` as the pad.
