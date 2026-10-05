# Drag and drop saving

Design record for RISC OS-style drag and drop saving in wuss: a Save As
dialogue whose file icon is dragged onto a Filer directory window. The design
below is implemented.

## Scope

- v1 is **save only**: drag the Save As icon onto a Filer directory window.
- Deferred, but the protocol leaves room for them: load from the Filer,
  task-to-task transfer and RAM transfer.
- wuss messaging is internal. It is not bridged to the real RISC OS Wimp; on
  RISC OS the whole of wuss runs inside one Wimp task.

## Where things live

| Piece | Location |
|---|---|
| Messaging, drag session, time, double-click | `libraries/wuss/core/` |
| File types | `io/filetype` (new) |
| Directory listing | `io/dirlist` (new) |
| Leafname validation | `io/path` |
| Grid view | `include/wuss/gadget/gridview.h` (`WUSS_GADGETS`) |
| Save As | `include/wuss/component/saveas.h` (`WUSS_COMPONENTS`) |
| Filer | `libraries/wuss/test/tasks/filer.{c,h}`, in `g_system_tasks` beside Display and Palette |

## Design

### `io/filetype`

- A struct holding both a RISC OS filetype (`uint16`) and an extension.
- A static mapping table: png↔0xB60, txt↔0xFFF, plus directory and
  application specials. Unknown maps to 0xFFD / `""`. Extension lookup is
  case-insensitive.
- `filetype_set(path, t)` sets the filetype attribute on RISC OS and is a
  no-op elsewhere. The saver calls it after writing.

### `io/dirlist`

- A dynamic list of `{leaf, is_dir, filetype}` entries.
- Sorted by name, case-insensitively. Dotfiles are filtered out.
- No cap and no silent skipping. (`io/namelist` is unsuitable: it uses a
  fixed table, skips long names silently, sorts with `strcmp` and carries no
  directory flag.)

### `io/path` additions

- Leafname validation: non-empty, contains no platform separator, and is
  neither `.` nor `..`.
- A "is this a full path" test, so callers never hard-code `/`.

### Inter-task messaging (core)

- Messages are fixed 1 KiB blocks. The header holds
  `{size, action, sender, target window, my_ref, your_ref}`. Blocks are
  copied into an array queue; there is no per-message allocation.
- A payload that does not fit (e.g. an over-long path) is an explicit error;
  paths are never truncated.
- Delivery kinds: plain send, recorded send and acknowledge. Recorded
  messages that nobody acknowledges bounce back to the sender as
  `wuss_EVENT_MESSAGE_BOUNCED`.
- A recorded message counts as acknowledged if, during its handler, the
  recipient sends any message with `your_ref == my_ref` or an explicit
  acknowledge. The bounce decision is made when the handler returns.
- `my_ref` comes from a global counter that skips 0 on wrap.
- Addressing is point-to-point (by task) or broadcast. Broadcast goes to
  every task except the sender, in creation order. For a recorded broadcast
  the first acknowledgement stops further delivery. Broadcast has no v1
  user but is tested.
- **Priority.** Sending always queues and delivery never nests inside another
  handler. Core keeps a dispatch-depth counter: whenever a public entry point
  (`wuss_mouse_click`, `wuss_mouse_move`, `wuss_key`, `wuss_idle`) returns to
  depth 0, the queue is drained to empty in FIFO order, including messages
  sent during the drain. `wuss_idle` also drains before and after delivering
  IDLE events. The queue is therefore always empty when control returns to
  the frontend. There is no public pump.
- **Livelock cap.** More than 256 deliveries in one drain logs a warning and
  defers the remainder to the next entry point. ASan builds also assert.
- **Dead endpoints.** On delivery, core checks the header's target window
  still exists (by scanning the window list) and bounces the message if not.
  `wuss_task_destroy` purges the queue: recorded messages addressed to the
  destroyed task bounce to their senders; anything addressed to a destroyed
  sender is dropped.
- Actions 0x0000-0xFFFF are reserved for core (`include/wuss/message.h`);
  tasks use 0x10000 upwards.

### Transfer protocol

Every step is a recorded send:

1. The saver sends `DataSave(point, estimated size or -1, filetype,
   leafname)` to the drop target; the target window is in the header.
2. The target replies `DataSaveAck(full path)`.
3. The saver writes the file, calls `filetype_set`, then sends
   `DataLoad(path)`.
4. The target replies `DataLoadAck`.

Because of the priority rule, the whole handshake completes within the
`wuss_mouse_click` that ends the drag.

### Time and double-click (core)

- `wuss_set_time(wuss, ms)` is called by the frontend once per frame (SDL:
  `SDL_GetTicks`; RISC OS: monotonic time). Tests set it explicitly. The
  `wuss_mouse_click` signature is unchanged.
- A double-click is a normal DOWN carrying a `wuss_BUTTON_DOUBLE` flag, so
  handlers that ignore it are unaffected. It requires the same button and
  the same window, within the configured time and distance.
- New `wuss_config_t` fields: `double_click_ms`, `double_click_px` and
  `drag_threshold_px`, with 0 meaning the defaults (400 ms, 4 px, 4 px).
  They are live-changeable via `wuss_set_config` and exposed in the
  Configure task.

### Drag session (core)

- `wuss_drag_start(task, box, ...)` begins a drag.
  `wuss_EVENT_DRAG_END {icon, drop window, point, cancelled}` goes to the
  originating task.
- Drawn as a marching-ants box, painted last in `wuss_redraw_dirty`. Only the
  four 1 px edges are invalidated, and the ants step every N frames.
- During a drag core takes over input: the ending release is not delivered
  as `MOUSE_UP`; menu hover tracking pauses; `POINTER_ENTER`/`EXIT` are
  deferred and settled at drag end; Escape is intercepted in `wuss_key` and
  cancels; the pointer is clamped to the screen. The SDL frontend captures
  the mouse for the duration.
- Menu chains dismiss only on a press outside them, so releasing a drag
  outside a menu-leaf Save As does not close the chain.

### DRAGGABLE icon

- Implements the existing `wuss_ICON_TYPE_DRAGGABLE` stub.
- Draws its bitmap as `wuss_ICON_TYPE_BITMAP` does.
- Starts a core drag once the pointer moves beyond `drag_threshold_px` with
  a button held. A click without movement does nothing.

### Grid view gadget

- Lives on a caller's window without owning it, following the `stringset`
  pattern: the task forwards `REDRAW`, `MOUSE` and `OPEN` to
  `wuss_gridview_handle_event`.
- Items come from a count plus
  `item_fn(index, opaque) -> {glyph, label}`. The gadget draws the glyph over
  the label and truncates long labels with an ellipsis. (A per-cell draw hook
  is deferred until a caller needs one.)
- Cell size is the largest glyph combined with a fixed N-character label
  width in the current font, plus padding, so one long name cannot inflate
  the grid.
- Column count follows the visible width and reflows on `OPEN`. The gadget
  sets the window's document extent. Hit-testing is arithmetic; gaps
  between cells miss.
- It owns the selection (a `bitvec`) with RISC OS rules: Select selects only
  the item, Adjust toggles it, a click on empty space clears the selection.
  A double-click on an item calls `activate_fn(index)`.
- Intended for reuse by other tasks (e.g. a resources editor). Drag-out
  hooks in here later.

### Save As component

- API: `wuss_saveas_create(&sa, wuss, &filetype, leafname, save_fn, opaque)`
  with `save_fn(const char *path, void *opaque) -> result_t`, plus
  `wuss_saveas_set_leafname`, `wuss_saveas_set_filetype`,
  `wuss_saveas_get_path`, `wuss_saveas_open` (show the dialogue) and
  `wuss_saveas_window` (for use as a menu submenu). The dialogue runs on a task of its own, created by
  `wuss_saveas_create` and freed by `wuss_saveas_destroy`. That task forwards
  the dialogue window's events through
  `wuss_saveas_handle_event(sa, window, event)`, so callers forward nothing.
- Layout: DRAGGABLE file icon; writable leafname; Cancel and Save buttons;
  status line.
- The default leafname gets the platform extension (none on RISC OS).
- Return saves directly if the writable holds a full path; otherwise the
  status line reads "To save, drag the icon to a directory display".
- Transfer state lives in `wuss_saveas_t`. A drag while a transfer is in
  progress is ignored.
- The dialogue closes on `DataLoadAck`. A bounce is silent.
- Presented both as a menu submenu leaf and standalone via
  `wuss_saveas_open`, which callers invoke from a menu selection. After a
  real save the writable holds the full path.

### Filer task

- A System menu row starts the task, which places a drive icon on the icon
  bar. Clicking that icon, or choosing the menu row again while the Filer is
  running, opens the root window.
- The root comes from `-root`, defaulting to the current directory;
  Emscripten uses `/`.
- Each window is a grid view over an `io/dirlist`, titled with its full
  path. Double-clicking a directory opens a new window, or brings an existing
  window for that path to the front. Double-clicking a file does nothing.
- As a drop target it validates the leafname (a bad one is not acknowledged
  and so bounces), treats an over-long path as an error, overwrites existing
  files silently, holds no state between `DataSaveAck` and `DataLoad`, and
  rescans on `DataLoad` and when a window opens. The drop point is ignored:
  files always land in the window's own directory.
- Glyphs are one PNG per glyph (`file_unk.png`, `file_dir.png`,
  `file_b60.png`) in `resources/wuss/iconpool/`, each loaded as its own
  `bitmap_t`, since bitmap icons have no sub-rect support. A missing glyph
  logs a warning and is tolerated: the cell shows its label only.

## Testing

- Each layer lands with its own tests.
- Tests write files only into a dedicated temporary directory, removed
  afterwards.
- Test tasks build only under `BUILD_APPS`, so the Filer cannot be linked
  into `DPTLibTest`. Instead the end-to-end headless test in `wuss-test.c`
  uses a minimal fake receiver task defined in the test, covering
  messaging, drag, DRAGGABLE and Save As under `build-asan`. The Filer's own
  glue is checked by hand in `build-sdl`.

## Build order

All steps are complete. Each was committed separately, with its tests.

1. `io/filetype`.
2. `io/dirlist` and the `io/path` leafname validation.
3. Core messaging.
4. `wuss_set_time`, the double-click flag, the config fields and the
   Configure task UI.
5. Core drag session, plus SDL mouse capture.
6. DRAGGABLE icon.
7. Grid view gadget.
8. Save As component and the end-to-end headless test (fake receiver).
9. Filer task: System menu row, icon bar icon, grid windows, receiver,
   glyphs.
10. Pilot in the gradient task.
11. Route `snapshot_save_png` through Save As (now used by most test tasks).
12. Update `libraries/wuss/TODO.txt` (layer 5 and 6 items).

## Deferred

- Load from the Filer, task-to-task transfer and RAM transfer.
- Error and query boxes.
- Sprite drag visuals (after masked sprites).
- Filer copy, move, delete and rename; other views; Adjust-closes-parent.
- Grid drag-out and custom cell drawing.
- `saved_fn`, a public message pump and unsaved-changes tracking.
