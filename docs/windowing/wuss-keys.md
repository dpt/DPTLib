# wuss > key reference

Survey of every key the wuss demo (`apps/wuss`) and its libraries react to, split into **system** keys (handled by wuss or the launcher whatever is focused) and **application** keys (handled by one task when its window has the focus). Ends with proposed standard keys (RISC OS style first, macOS where the guide is silent).

## How a key travels

1. The frontend (`frontend-sdl.c`, `frontend-riscos.c`) turns a key press into a Unicode code point or a `wuss_KEY_*` constant plus modifiers (Shift, Ctrl, Alt). On macOS Cmd is folded into Ctrl, so "Ctrl" below means Cmd there.
2. `wuss_key` (`core/key.c`) offers it in order to:
   1. an active window drag (Escape cancels it);
   2. an open menu chain (Escape dismisses it);
   3. the caret's writable icon (editing keys);
   4. the focused window's task, as `wuss_EVENT_KEY`.
3. A task that does not want the key returns `result_WUSS_KEY_UNCLAIMED`. `main.c` then passes the key to `tasks_launcher_key`, which handles F2 and tries the Wuss menu's shortcut labels.

A task's menu shortcuts only fire while one of its windows has the focus (`wuss_menu_dispatch_shortcut`). Labels match as follows: `^X` needs Ctrl; a bare label forbids Ctrl; Alt never matches; Shift is required by a `WUSS_MENU_SHIFT` prefix and forbidden for SPACE and F1..F12.

## System keys

| Key      | Action                              | Where                  |
| -------- | ----------------------------------- | ---------------------- |
| Escape   | Cancel a window drag or resize      | `core/key.c`           |
| Escape   | Dismiss the open menu chain         | `menu/menu.c`          |
| Return   | Press the dialogue's default button | `component/dialogue.c` |
| Escape   | Press the dialogue's Cancel button  | `component/dialogue.c` |
| F1       | Debug > Redraw                      | `apps/wuss/tasks.c`    |
| Shift-F1 | Debug > Garbage                     | `apps/wuss/tasks.c`    |
| F2       | Zoom in                             | `tasks_launcher_key`   |
| Shift-F2 | Zoom out                            | `tasks_launcher_key`   |
| F3       | Debug > Pixel Stress                | `apps/wuss/tasks.c`    |
| F4       | Quit Wuss (not on Emscripten)       | `apps/wuss/tasks.c`    |

Notes:

- F1..F4 are temporary allocations. They clash with the RISC OS standard keys and move to Shift-F keys (see "Changes to existing keys").
- F1..F4 are only reached when the focused task leaves the key unclaimed.
- The frontend's window-close request (SDL quit) also quits Wuss.
- There is no key to close, hide, raise or cycle windows or tasks.

### Text editing (writable icons)

Active whenever a writable icon holds the caret. Alt-anything is ignored.

| Key                      | Action                                 |
| ------------------------ | -------------------------------------- |
| Left / Right             | Move one character                     |
| Shift-Left / Shift-Right | Move one word                          |
| Home / End               | Start / end of text                    |
| Ctrl-Left / Ctrl-Right   | Start / end of text                    |
| Tab / Shift-Tab          | Next / previous writable in the window |
| Backspace                | Delete before caret                    |
| Delete                   | Delete after caret                     |
| Ctrl-U                   | Clear the field                        |
| Any printable            | Insert at caret                        |

## Application keys

Menu shortcuts (fire when the task has the focus):

| Key    | Action         | Tasks                                                                                                                                    |
| ------ | -------------- | ---------------------------------------------------------------------------------------------------------------------------------------- |
| F3     | Save / Save as | ball chars checker clock compositing curving doughnut fern fireworks gradient greebling oscillating orbing painting patterns saturn sofa |
| Ctrl-R | Randomise      | checker doughnut fern orbing oscillating patterns saturn sofa                                                                            |
| Ctrl-Z | Undo           | painting                                                                                                                                 |
| Ctrl-P | Pause          | zxtask                                                                                                                                   |
| Ctrl-M | Mono           | zxtask                                                                                                                                   |

Direct keys:

| Task                                | Key                 | Action                                                  |
| ----------------------------------- | ------------------- | ------------------------------------------------------- |
| ball chars clock fireworks patterns | menu shortcuts only | as above                                                |
| checker                             | Left / Right        | Previous / next pattern                                 |
| checker                             | Up / Down           | Wider / narrower bands                                  |
| compositing                         | Left / Right        | Previous / next operator                                |
| curving                             | Left / Right        | Previous / next curve kind                              |
| curving                             | Up / Down           | Scroll (wheel equivalent)                               |
| doughnut                            | Arrows              | Rotate (a / b)                                          |
| doughnut                            | `[` `]`             | Major density down / up                                 |
| doughnut                            | `,` `.`             | Minor density down / up                                 |
| doughnut                            | `-` `=` `+`         | Tube density down / up                                  |
| fern                                | Left / Right        | Previous / next seed                                    |
| greebling                           | Left / Right        | Previous / next palette (Right = Adjust)                |
| greebling                           | Space               | Select                                                  |
| gradient                            | S                   | Next shape                                              |
| gradient                            | D                   | Next dither matrix                                      |
| minesweeping                        | Arrows              | Move cursor                                             |
| minesweeping                        | Space               | Flag (Adjust)                                           |
| oscillating                         | Left / Right        | Previous / next frequency pair                          |
| orbing                              | Arrows              | Nudge light (Shift = bigger step)                       |
| orbing                              | O                   | Toggle the current light                                |
| painting                            | B E L R X F P S O   | Brush Eraser Line Rect RectFill Fill Spray Select Lasso |
| painting                            | `[` `]`             | Size down / up                                          |
| painting                            | Escape              | Drop selection                                          |
| saturn                              | Left / Right        | Previous / next seed                                    |
| sofa                                | Left / Right        | Previous / next model                                   |
| sofa                                | Up / Down           | Step rotation (paused only)                             |
| zxtask                              | Arrows              | Kempston joystick                                       |
| zxtask                              | `.`                 | Kempston fire                                           |
| zxtask                              | Other ASCII         | Spectrum keyboard (Shift = Caps Shift)                  |
| painting-new                        | Return / Escape     | Dialogue OK / Cancel                                    |

Tasks with no key handling (all keys unclaimed): resizing viewing palette effecting configuring icons display reading dithering filing tiling. `keys` is the key-test task and echoes everything.

## Collisions to avoid

Rules for the proposals below:

1. The RISC OS 3 Style Guide (pp. 68 to 73) wins wherever it defines a key.
2. macOS chords fill the gaps where the guide is silent. Ctrl means Cmd on macOS.
3. Never use Alt in a shortcut. The guide reserves it for international characters. Use Ctrl-Shift instead.
4. Never use F12, Shift-F12, Ctrl-F12 or Ctrl-Shift-F12. RISC OS owns them.
5. Shift modifies a function key into a related action. Ctrl gives an unrelated one.

Taken today by existing tasks: F3 (Save) F8 (painting Undo) Ctrl-R P M U; Escape; Return. Ctrl-Z (painting Undo) clashes with the guide and moves to F8 (see below). Ctrl-P and Ctrl-M (zxtask) are safe: the guide uses the Print key and does not define Ctrl-P or Ctrl-M.

## Standard keys: RISC OS style

Adopt all of these as defined by the guide. Keys marked "new" have no code today.

### Function keys

| Key        | Action                                   |
| ---------- | ---------------------------------------- |
| F1         | Help                                     |
| F2         | Load named document                      |
| Shift-F2   | Insert named document                    |
| Ctrl-F2    | Close window (`wuss_window_try_close`)   |
| F3         | Save document                            |
| F4         | Find / search and replace                |
| F5         | Go to                                    |
| F6         | Sort                                     |
| F8         | Undo                                     |
| F9         | Redo                                     |
| F7 F10 F11 | Free for apps and the system (see below) |

Done: F3 is the Save shortcut on every task with a Save row (Ctrl-S is gone), and painting Undo is F8. The other keys need a function no task has yet: Help (F1), Load (F2), Insert (Shift-F2), Find (F4), Go to (F5), Sort (F6) and Redo (F9). Wire each when a task gains the function: give its menu row the shortcut label and the shortcut dispatcher does the rest.

### Ctrl keys

Ctrl-D, Ctrl-T, Ctrl-B and Ctrl-I are text-editor keys. The guide applies them only to applications that offer those functions. Other tasks may reuse them (see Duplicate and Get info below).

| Key    | Action                          |
| ------ | ------------------------------- |
| Ctrl-C | Copy selection to the clipboard |
| Ctrl-X | Cut selection to the clipboard  |
| Ctrl-V | Paste at the caret              |
| Ctrl-A | Select all                      |
| Ctrl-Z | Clear selection                 |
| Ctrl-U | Delete line                     |
| Ctrl-K | Delete (for example a file)     |
| Ctrl-D | Insert date                     |
| Ctrl-T | Insert time                     |
| Ctrl-B | Bold                            |
| Ctrl-I | Italic                          |

### Named keys

| Key             | Action                                                       |
| --------------- | ------------------------------------------------------------ |
| Escape          | Cancel operation                                             |
| Return          | New line in an editor; default action in a dialogue          |
| Print           | Print document                                               |
| Tab / Shift-Tab | Tab stop in an editor; next / previous field in a dialogue   |
| Insert          | Paste at the caret                                           |
| Backspace       | Delete left of the caret; with no caret, cut the selection   |
| Delete          | As Backspace                                                 |
| Copy            | Delete right of the caret; with no caret, copy the selection |
| Home            | Start of document                                            |
| Page Up / Down  | Scroll a page                                                |
| Left / Right    | Move one character                                           |
| Up / Down       | Move one line; in a dialogue, previous / next field          |

The guide also requires keys for word, page, line and document movement, but the arrow glyphs are lost in the PDF text. Wuss already uses Shift-Left / Right for a word and Ctrl-Left / Right for the ends of text. Check the printed guide before relying on the rest. On a PC keyboard the Copy key becomes End and Backspace / Delete never cut.

## Standard keys: macOS fill-ins

Used only where the guide is silent.

| Key             | Action                 | Notes                                                                                                     |
| --------------- | ---------------------- | --------------------------------------------------------------------------------------------------------- |
| Ctrl-N          | New window             | Task decides what "new" means (document or view).                                                         |
| Ctrl-G          | Find next              | Repeats the F4 find.                                                                                      |
| Ctrl-Shift-G    | Find previous          |                                                                                                           |
| Ctrl-E          | Use selection for find |                                                                                                           |
| Ctrl-=          | Increase size          | Zoom in, larger text or brush. Ctrl-Plus also works. Unclaimed, it zooms the whole desktop (replaces F2). |
| Ctrl--          | Decrease size          | As above (replaces Shift-F2).                                                                             |
| Ctrl-0          | Actual size            |                                                                                                           |
| Ctrl-,          | Choices                |                                                                                                           |
| Ctrl-[ / Ctrl-] | Back / forward         |                                                                                                           |
| Ctrl-Shift-Up   | Parent directory       | Filer only.                                                                                               |
| Ctrl-Shift-N    | New directory          | Filer only.                                                                                               |
| Ctrl-D          | Duplicate              | Text editors use Ctrl-D for Insert date instead.                                                          |
| Ctrl-I          | Get info               | Text editors use Ctrl-I for Italic instead.                                                               |
| Ctrl-Shift-F    | Toggle maximise        |                                                                                                           |
| Ctrl-Shift-3    | Save framebuffer PNG   |                                                                                                           |
| Ctrl-Shift-Esc  | Kill the focused task  | No unsaved-data prompt.                                                                                   |

Dropped, because the guide already covers them: Ctrl-O (F2), Ctrl-F (F4), Ctrl-L (F5), Ctrl-Shift-S (F3 opens the save dialogue), Ctrl-Shift-Z and Ctrl-Y (F9), Ctrl-W (Ctrl-F2), Ctrl-/ (F1) and Ctrl-Backspace for trash (Ctrl-K).

## System shortcuts: window and task management

The guide is silent on these, so macOS chords lead. Each has a function-key fallback for Emscripten, where browsers reserve Ctrl-Q and Ctrl-Tab. The fallbacks use only F7, F10 and F11 plus their Shift and Ctrl forms, which the guide leaves free.

| Action                                           | Primary        | Fallback  | Notes                                                                                   |
| ------------------------------------------------ | -------------- | --------- | --------------------------------------------------------------------------------------- |
| Close window                                     | Ctrl-F2        | none      | RISC OS standard. Same path as the close icon (`wuss_window_try_close`).                |
| Quit app (close all windows of the focused task) | Ctrl-Q         | Ctrl-F9   | Goes through `wuss_window_try_close` for each window so unsaved-data prompts still run. |
| Quit Wuss                                        | Ctrl-Shift-Q   | none      | Replaces F4. Not on Emscripten.                                                         |
| Hide app (all its windows)                       | Ctrl-H         | F11       | Needs a hidden state (see below).                                                       |
| Hide others                                      | Ctrl-Shift-H   | Shift-F11 | Hides every task but the focused one.                                                   |
| Show all / restore                               | none           | Ctrl-F11  | Restores every hidden task; focus returns to the last one hidden.                       |
| Next window in app                               | Ctrl-\`        | F7        | Cycles the focused task's windows back to front.                                        |
| Previous window in app                           | Ctrl-Shift-\`  | Shift-F7  |                                                                                         |
| Next app                                         | Ctrl-Tab       | F10       | Raises the front window of the next task in recency order.                              |
| Previous app                                     | Ctrl-Shift-Tab | Shift-F10 |                                                                                         |
| Bring app to front                               | none           | Ctrl-F10  | Same effect as `wuss_task_raise_children`. No free macOS chord.                         |

## Changes to existing keys

- Debug and desktop keys move off the RISC OS standard function keys:

  | Action               | Old      | New           |
  | -------------------- | -------- | ------------- |
  | Debug > Redraw       | F1       | Shift-F1      |
  | Debug > Garbage      | Shift-F1 | Ctrl-Shift-F1 |
  | Debug > Pixel Stress | F3       | Shift-F3      |
  | Zoom in              | F2       | Ctrl-=        |
  | Zoom out             | Shift-F2 | Ctrl--        |
  | Quit Wuss            | F4       | Ctrl-Shift-Q  |

- painting: Undo moves from Ctrl-Z to F8. Add Redo on F9. Ctrl-Z becomes Drop selection (clear selection); Escape keeps doing it too.
- Wuss menu shortcut labels need the guide's symbols: `^` for Ctrl, a Shift marker for Shift. F8 and F9 labels show as plain `F8` and `F9`.

## Dispatch changes

- Add a system layer in `main.c` ahead of `wuss_key`, matching only the window and task chords above plus Ctrl-F2, Ctrl-=, Ctrl--, Ctrl-Shift-Q and the debug keys. An app cannot swallow them.
- Standard editing and file keys (F1 to F9, Ctrl-C and friends) go to the focused task first. If it returns `result_WUSS_KEY_UNCLAIMED`, the launcher may handle them (Ctrl-= and Ctrl-- zoom the desktop).
- Done: writable icons handle Ctrl-A, Ctrl-C, Ctrl-X, Ctrl-V, Ctrl-Z and Insert, and Backspace and Delete remove the selection. The Copy key (End on RISC OS) is not wired, and Shift-arrows still move by word, so a selection comes only from Ctrl-A.
- **Caret owns Ctrl-D, Ctrl-I, Ctrl-K, Ctrl-T and Ctrl-B.** While a writable holds the caret (for example an inline rename in the filer), it swallows these item-level keys and the task never sees them, so Ctrl-D or Ctrl-I cannot trigger Duplicate or Get info mid-rename. Other Ctrl-letter keys such as Ctrl-R still reach the task, so a seed field does not block them. System-layer chords (Ctrl-F2, Ctrl-Q and so on) run before the caret and still work. Done in `icon/writable.c`.
- Do not route the system layer through menu shortcut labels. They need a focused window.
- The RISC OS frontend must pass F12 and its Shift and Ctrl forms to the OS untouched.

## Missing library support

- **Clipboard**: done. `wuss_clipboard_set`, `wuss_clipboard_get` and `wuss_clipboard_generation` hold the text, and the SDL frontend syncs it with the host clipboard around each key press. The RISC OS frontend has no host bridge.
- **Selection**: done for writables, as an anchor beside the caret index. There is no mouse drag, double-click or Shift-arrow selection yet.
- **Hide and restore**: done. `wuss_task_set_hidden` hides an app's windows (never the back stack, which holds the icon bar). `wuss_task_hide_app`, `wuss_task_hide_others` and `wuss_task_show_all` sit behind Ctrl-H, Ctrl-Shift-H and Ctrl-F11, and the F11 forms. Show all focuses the frontmost window rather than the last one hidden.
- **Cycle and raise**: done as `wuss_task_front`, `wuss_task_cycle_windows` and `wuss_cycle_tasks`, with no recency list. Next app raises the rearmost other app and previous app sends the front app behind the rest, so repeated presses walk the whole z-order. Only visible focusable regular windows take part, and an app is a task together with its child tasks.
- **Quit app**: done as `wuss_task_close_windows`.

Build order: every step is done except the macOS fill-ins and the remaining function keys.
