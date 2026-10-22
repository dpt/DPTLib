# [DPTLib](https://github.com/dpt/DPTLib) > wuss > Menu standard

Every wuss app's window menu follows this arrangement, so the same thing sits
in the same place from one app to the next. It follows the RISC OS Style
Guide and PrivateEye's menus where they apply.

The icon bar menu is not covered here. `taskapp` builds it (Info, New...,
Hide, Quit). Help and Choices... will join it when wuss has help text and a
choices system.

## Order

A window menu is titled with the app's name. Its rows come in these groups,
top to bottom. Leave out any group the app has nothing for. Put a separator
between groups, never inside one.

1. **First item** — one row:
   - `File ⇒` if the app has a document (Info on it, and other
     document-wide items), as in PrivateEye;
   - otherwise the app's main action, such as `Pause`, `New game` or `Undo`.
     If there are two candidates, the one used more often goes here and the
     other goes in Settings.
2. **Save** — `Save ⇒`.
3. **Edit** — Undo (if not first), `Selection ⇒`, and so on. Keep these flat.
   Fold them into an `Edit ⇒` submenu once there are more than about four.
4. **Settings** — the app's own options: `Model ⇒`, `Shape ⇒`, `Gravity` and
   the like.
5. **Display** — how the content is shown: `Colours ⇒`, `Dithering ⇒`,
   scale, zoom, `Double size`.
6. **Generate** — `Mutate`, `Randomise`, `Reset`.

For example, Sofa:

```
Sofa
  Pause          Space
  ----------------------
  Save         ⇒ F3
  ----------------------
  Model        ⇒
  Auto-cycle
  ----------------------
  Colours      ⇒
  ----------------------
  Randomise      ^R
  Reset          R
```

## Save

`Save ⇒` opens the `wuss_saveas` box as its submenu (set the row's `window`
to `wuss_saveas_window`). `F3` opens the same box on its own through
`wuss_saveas_open`. Every app with Save binds `F3`.

The row is `Save`, not `Save PNG`. Once an app can save in a second format,
`Save ⇒` becomes a submenu of formats, each opening its save box, and `F3`
opens the box for the format used last (PNG to start with). Until then
`Save ⇒` goes straight to the PNG box.

## Labels

- Sentence case: `Select all`, `New game`, `Grid size`. Proper nouns keep
  their capitals.
- `...` only on a row that opens a dialogue box. A row with a submenu has no
  `...`.
- `Colours ⇒` for colour choices, including a lone background colour. There
  is no top-level `Background ⇒`.
- Running and stopping is one `Pause` row, ticked while paused. There is no
  `Animate`.
- Selection rows are `Select all` and `Clear selection`.

## Keys

These keys are reserved. Don't use them for anything else in a window.

| Row             | Key     |
|-----------------|---------|
| Save ⇒          | `F3`    |
| Pause           | `Space` |
| Undo            | `F8`    |
| Select all      | `^A`    |
| Clear selection | `^Z`    |
| Randomise       | `^R`    |
| Reset           | `R`     |
| New game        | `N`     |

Use `WUSS_MENU_ITEM_SHORTCUT` so the key is shown against the row.
