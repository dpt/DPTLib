# wuss > key reference

Survey of every key the wuss demo (`apps/wuss`) and its libraries react to, split into **system** keys (handled by wuss or the launcher whatever is focused) and **application** keys (handled by one task when its window has the focus). Ends with proposals for new system shortcuts.

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

| Key | Action | Where |
|---|---|---|
| Escape | Cancel a window drag or resize | `core/key.c` |
| Escape | Dismiss the open menu chain | `menu/menu.c` |
| Return | Press the dialogue's default button | `component/dialogue.c` |
| Escape | Press the dialogue's Cancel button | `component/dialogue.c` |
| F1 | Debug > Redraw | `apps/wuss/tasks.c` |
| Shift-F1 | Debug > Garbage | `apps/wuss/tasks.c` |
| F2 | Zoom in | `tasks_launcher_key` |
| Shift-F2 | Zoom out | `tasks_launcher_key` |
| F3 | Debug > Pixel Stress | `apps/wuss/tasks.c` |
| F4 | Quit Wuss (not on Emscripten) | `apps/wuss/tasks.c` |

Notes:

- F1..F4 are only reached when the focused task leaves the key unclaimed.
- The frontend's window-close request (SDL quit) also quits Wuss.
- There is no key to close, hide, raise or cycle windows or tasks.

### Text editing (writable icons)

Active whenever a writable icon holds the caret. Alt-anything is ignored.

| Key | Action |
|---|---|
| Left / Right | Move one character |
| Shift-Left / Shift-Right | Move one word |
| Home / End | Start / end of text |
| Ctrl-Left / Ctrl-Right | Start / end of text |
| Tab / Shift-Tab | Next / previous writable in the window |
| Backspace | Delete before caret |
| Delete | Delete after caret |
| Ctrl-U | Clear the field |
| Any printable | Insert at caret |

## Application keys

Menu shortcuts (fire when the task has the focus):

| Key | Action | Tasks |
|---|---|---|
| Ctrl-S | Save / Save as | ball chars checker clock compositing curving doughnut fern fireworks gradient greebling oscillating orbing painting patterns saturn sofa |
| Ctrl-R | Randomise | checker doughnut fern orbing oscillating patterns saturn sofa |
| Ctrl-Z | Undo | painting |
| Ctrl-P | Pause | zxtask |
| Ctrl-M | Mono | zxtask |

Direct keys:

| Task | Key | Action |
|---|---|---|
| ball chars clock fireworks patterns | menu shortcuts only | as above |
| checker | Left / Right | Previous / next pattern |
| checker | Up / Down | Wider / narrower bands |
| compositing | Left / Right | Previous / next operator |
| curving | Left / Right | Previous / next curve kind |
| curving | Up / Down | Scroll (wheel equivalent) |
| doughnut | Arrows | Rotate (a / b) |
| doughnut | `[` `]` | Major density down / up |
| doughnut | `,` `.` | Minor density down / up |
| doughnut | `-` `=` `+` | Tube density down / up |
| fern | Left / Right | Previous / next seed |
| greebling | Left / Right | Previous / next palette (Right = Adjust) |
| greebling | Space | Select |
| gradient | S | Next shape |
| gradient | D | Next dither matrix |
| minesweeping | Arrows | Move cursor |
| minesweeping | Space | Flag (Adjust) |
| oscillating | Left / Right | Previous / next frequency pair |
| orbing | Arrows | Nudge light (Shift = bigger step) |
| orbing | O | Toggle the current light |
| painting | B E L R X F P S O | Brush Eraser Line Rect RectFill Fill Spray Select Lasso |
| painting | `[` `]` | Size down / up |
| painting | Escape | Drop selection |
| saturn | Left / Right | Previous / next seed |
| sofa | Left / Right | Previous / next model |
| sofa | Up / Down | Step rotation (paused only) |
| zxtask | Arrows | Kempston joystick |
| zxtask | `.` | Kempston fire |
| zxtask | Other ASCII | Spectrum keyboard (Shift = Caps Shift) |
| painting-new | Return / Escape | Dialogue OK / Cancel |

Tasks with no key handling (all keys unclaimed): resizing viewing palette effecting configuring icons display reading dithering filing tiling. `keys` is the key-test task and echoes everything.

## Collisions to avoid

Free for system use today: Ctrl plus any letter except S R Z P M U; Ctrl-Tab; Alt plus anything (menu shortcuts and writables ignore it); F5..F12.

Taken: Ctrl-S R Z P M U; F1 F2 F3 F4; Escape; Return.

## Proposed system shortcuts

Windows and Linux reserve Alt-Tab and Alt-F4, and browsers reserve Ctrl-W, Ctrl-Q and Ctrl-Tab, so every proposal has a function-key fallback usable under Emscripten. Ctrl means Cmd on macOS.

| Action | Primary | Fallback | Notes |
|---|---|---|---|
| Quit app (close all windows of the focused task) | Ctrl-Q | Shift-F4 | Goes through `wuss_window_try_close` for each window so unsaved-data prompts still run. |
| Close window | Ctrl-W | F9 | Same path as the close icon (`wuss_window_try_close`). |
| Hide app (all its windows) | Ctrl-H | F10 | Needs a hidden state (see below). |
| Hide others | Ctrl-Alt-H | Shift-F10 | Hides every task but the focused one. |
| Show all / restore | Ctrl-Shift-H | F11 | Restores every hidden task; focus returns to the last one hidden. |
| Next window in app | Ctrl-\` | F6 | Cycles the focused task's windows back to front. |
| Previous window in app | Ctrl-Shift-\` | Shift-F6 | |
| Next app | Ctrl-Tab | F5 | Raises the front window of the next task in recency order. |
| Previous app | Ctrl-Shift-Tab | Shift-F5 | |
| Bring app to front | Ctrl-Alt-Tab | F7 | Raises every window of the focused task above all others, keeping their relative order. Same effect as `wuss_task_raise_children`. |
| Quit Wuss | Ctrl-Alt-Q | F4 (kept) | Replaces nothing; F4 stays. |

Dispatch changes:

- Add a system layer in `main.c` ahead of `wuss_key`, matching only the chords above. Because the layer sits before the focused task, an app cannot swallow them. No existing app uses any of these chords.
- Writable icons are safe: they ignore every Ctrl chord except Ctrl-U and Ctrl-Left/Right.
- Do not route through menu shortcut labels. Those need a focused window and ignore Alt, so Ctrl-Alt chords could not work there.

Missing library support:

- **Hide and restore**: wuss has window state `TOGGLED` (maximise) but no hidden state. Add a `wuss__window_*` flag `HIDDEN` that skips the window in draw, hit-test and focus selection, with `wuss_task_hide` / `wuss_task_show` helpers. Hidden tasks stay alive; the AppsFiler or a Show-all key brings them back.
- **Cycle and raise**: z-order is kept by `wuss_window_restack`. Add `wuss_task_front(wuss, task)` (generalising `wuss_task_raise_children`) and a task recency list for Ctrl-Tab.
- **Quit app**: add `wuss_task_close_windows(task)` that loops `wuss_window_try_close` and stops at the first veto.

Build order: close window and quit app first (no new state), then raise and cycle, then hide.
