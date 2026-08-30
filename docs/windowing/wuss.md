# [DPTLib](https://github.com/dpt/DPTLib) > wuss

"wuss" is a minimal window manager. It owns window creation, positioning, sizing, z-ordering, mouse/scroll event routing and dragging. Window contents are entirely delegated to tasks, which supply a single event-handling callback.

- It draws windows back-to-front, with an optional titlebar and 1px outline per window.
- It hit-tests and routes mouse-down/up/move and scroll events, including titlebar drag-to-move.
- It tracks a single dirty region, accumulated automatically by window management (create/destroy/move/resize/bring-to-front) and manually by tasks, for partial redraws.
- Content drawing and mouse handling are entirely task-supplied; Wuss only fills the content background before delivering a redraw event to the task's handle callback (unless the task opts out with `wuss_NO_BACKGROUND`). Scrolling is Wuss's own default action (see "Scrolling" below); a task may additionally react to `wuss_EVENT_SCROLL` for its own purposes.

## Setup

Create a window manager onto a `screen_t`, with an optional font for titlebar labels and an optional system palette:

```C
result_t wuss_create(screen_t             *scr,
                     bmfont_t             *font,
                     const colour_t       *palette,
                     int                   npalette,
                     const wuss_config_t  *config,
                     wuss_t              **wuss);
```

`font` and `palette` may both be NULL, for unlabelled titlebars and a built-in default palette respectively. `config` may be NULL for default titlebar height/colours. `config->backdrop` sets a desktop background colour painted behind windows on every redraw, or `wuss_NO_BACKGROUND` (the default when `config` is NULL) to leave the background untouched and require the caller to repaint it itself. `wuss_get_font` reads back the font passed in (or NULL), for a task that wants to draw its own content in the same face as titlebars.

Destroy with `wuss_destroy`, which also destroys any windows still open on it.

## Windows

Create a window with a content bounding box, optional title, appearance flags, a content background and a task delegate:

```C
result_t wuss_window_create(wuss_t *wuss, const box_t *content, const char *title,
                            wuss_window_flags_t flags, wuss_colour_t bg,
                            const wuss_task_t *task,
                            size2d_t doc, size2d_t min_doc,
                            wuss_window_t **window);
```

`bg` is filled in by wuss before each redraw event, or `wuss_NO_BACKGROUND` for the task to draw its own background (avoids a redundant fill behind an opaque task); changeable later via `wuss_window_set_background`.

`doc` is the virtual document extent behind the horizontal/vertical scrollbars' sausage proportion; pass `content`'s own width/height for a window with nothing to scroll. It is also the ceiling a resize-drag or toggle-size grows the content area to. Set once at creation, immutable thereafter.

`min_doc` is the opposite end: the smallest content size a resize-drag or toggle-size will shrink to. Pass `(0, 0)` for the built-in floor. It is clamped both to that built-in floor, so a window can never be squeezed too small to grab, and to `doc`, so it can never demand a window larger than the document it shows. Also set once at creation.

Furniture is additional to `content`, not carved out of it: the window's content area always ends up exactly the box requested, and its on-screen footprint (`wuss_window_get_visible_bounds`) is `content` expanded outward by whatever furniture flags request — a titlebar above, and/or a 1px outline around all four sides.

`wuss_task_t` holds the task's event callback:

```C
typedef struct wuss_task
{
  wuss_event_fn_t *handle;       /* NULL => task receives no events */
  void            *task_data;
}
wuss_task_t;
```

`wuss_task_start` builds one from `handle`/`task_data`.

`flags` combines, by bitwise OR:

- `wuss_WINDOW_NONE` — default: every furniture region drawn.
- `wuss_WINDOW_NO_TITLEBAR` — no titlebar, and no drag handle.
- `wuss_WINDOW_NO_OUTLINE` — no 1px border around the window.
- `wuss_WINDOW_NO_CLOSE` — no close button in the titlebar.
- `wuss_WINDOW_NO_BACK` — no back button in the titlebar (`wuss_BUTTON_SELECT` sends the window to back, `wuss_BUTTON_ADJUST` brings it to front).
- `wuss_WINDOW_NO_TOGGLE_SIZE` — no toggle-size button in the titlebar.
- `wuss_WINDOW_NO_VSCROLL` — no vertical scrollbar on the right edge.
- `wuss_WINDOW_NO_HSCROLL` — no horizontal scrollbar on the bottom edge.
- `wuss_WINDOW_NO_RESIZE` — no resize button in the bottom-right corner.
- `wuss_WINDOW_NO_RESIZE_BLIT` — a resize (drag or toggle-size) always fully redraws the window's content instead of blitting the preserved region; for a task whose rendering depends on window size in ways a partial redraw can't patch (e.g. a layout that spans the whole window).

`wuss_WINDOW_NO_CLOSE`/`NO_BACK`/`NO_TOGGLE_SIZE` are ignored if `flags` includes `wuss_WINDOW_NO_TITLEBAR`; `NO_VSCROLL`/`NO_HSCROLL`/`NO_RESIZE`/`NO_RESIZE_BLIT` apply regardless.

All furniture actions (back, toggle-size, resize-drag, scrollbar arrow/thumb) are handled entirely within Wuss via `wuss_mouse_click`/`wuss_mouse_move` — no new client events.

Other window operations: `wuss_window_close`, `wuss_window_move`, `wuss_window_resize` (preserves content top-left), `wuss_window_restack`, `wuss_window_get_visible_bounds` (full on-screen footprint), `wuss_window_get_content_bounds`, `wuss_window_set_background`.

## Task callbacks

```C
typedef enum wuss_event_kind
{
  wuss_EVENT_REDRAW,
  wuss_EVENT_MOUSE,
  wuss_EVENT_ICON,
  wuss_EVENT_SCROLL
}
wuss_event_kind_t;

typedef struct wuss_event
{
  wuss_event_kind_t kind;
  union
  {
    struct { screen_t *scr; const box_t *content; }                        redraw;
    struct { wuss_mouse_action_t action; point_t point; wuss_button_t button; } mouse;
    struct { wuss_icon_t *icon; wuss_mouse_action_t action; wuss_button_t button; } icon;
    struct { point_t point; int delta; }                                        scroll;
  }
  data;
}
wuss_event_t;

typedef result_t (wuss_event_fn_t)(wuss_window_t *window, const wuss_event_t *event,
                                   void *task_data);
```

A task supplies a single `handle` callback and dispatches on `event->kind`. Only the union member matching `kind` is valid.

For `wuss_EVENT_REDRAW`, `event->data.redraw.scr` is called with `scr->clip` already set to the on-screen, clipped content area; `event->data.redraw.content` gives the window's full (unclipped) content box in screen space, for context.

For `wuss_EVENT_MOUSE` and `wuss_EVENT_SCROLL`, `event->data.mouse.point` and `event->data.scroll.point` are window-local content coordinates: the content area's top-left is `(0,0)` plus the window's current scroll offset (see "Scrolling" below). `event->data.mouse.action` is `wuss_MOUSE_DOWN`/`wuss_MOUSE_UP`/`wuss_MOUSE_MOVE`. A titlebar click never reaches a task's handle callback: it starts a drag (and, for `wuss_BUTTON_SELECT`, brings the window to front) instead. A content click, even on a `wuss_WINDOW_NO_TITLEBAR` window with no drag handle, never changes z-order — only a titlebar click raises a window — so tasks are free to use content clicks for their own purposes without Wuss reordering windows underneath them.

`wuss_EVENT_ICON` is delivered instead of `wuss_EVENT_MOUSE` whenever the pointer is inside a `wuss_ICON_TYPE_BUTTON` icon's bounding box (see "Icons" below): `event->data.icon.icon` names the icon, `action` and `button` carry the same values a `wuss_EVENT_MOUSE` would. `wuss_ICON_TYPE_LABEL` icons, and hidden or disabled icons, never raise it — clicks over them fall through as `wuss_EVENT_MOUSE`.

## Mouse and scroll routing

Feed mouse events in with `wuss_mouse_click` (action `wuss_MOUSE_DOWN` or `wuss_MOUSE_UP`) and `wuss_mouse_move`, and scroll events with `wuss_scroll`, each hit-testing the topmost window at `(x, y)` and delivering to its task in window-local coordinates. All three take an optional `wuss_window_t **hit` out-parameter naming the window under the pointer (or being dragged). Events are dropped if the hit window has no handle callback, or (for scroll) the pointer is over its titlebar.

## Scrolling

Each window carries a scroll offset, `(0, 0)` by default: the point in the task's virtual content space that appears at the content area's top-left. `wuss_window_set_scroll(window, x, y)` moves it (invalidating the content area so the next redraw picks it up); `wuss_window_get_scroll` reads it back. `wuss_scroll` applies this offset itself as Wuss's default scroll action, clamped to `doc` (set at window creation), before also delivering `wuss_EVENT_SCROLL` to the task if it has a handle:

- window-local `x`/`y` delivered in mouse/scroll events (and expected in `wuss_window_invalidate`'s `local_box`) are in virtual content space, i.e. already shifted by the scroll offset.
- a redraw event's `content` is still the on-screen (unscrolled) content box; a task reads the offset itself via `wuss_window_get_scroll` to work out which part of its content to paint there.

## Redrawing

- `wuss_redraw` repaints every window, back-to-front, unconditionally, having first painted the configured backdrop colour (see Setup) behind them, if any.
- Within a window, Wuss paints in a fixed order: the window background colour, then its icons (see "Icons" below), then the task's `wuss_EVENT_REDRAW` handler — so a task always draws over the background and any icons, never under them.
- `wuss_invalidate` / `wuss_window_invalidate` mark a screen-space or window-local region dirty; window management calls these automatically for its own changes, but a task must call one of them itself whenever its content changes on its own (e.g. an animation), passing the union of the old and new areas that need repainting.
- `wuss_redraw_dirty` repaints only the accumulated dirty region, then clears it, painting the backdrop colour into each dirty region first if one was configured. Without a configured backdrop, Wuss only repaints windows, not the background between/behind them, so a caller whose invalidation can expose background (e.g. after a window move) should clear that region itself first.
- `wuss_get_dirty_count`/`wuss_get_dirty(wuss, index, out)` fetch the currently accumulated dirty regions (coalesced as they accumulate, up to a fixed cap after which further regions are merged into the last one) without redrawing.
- A window move or resize is clipped, piece by piece, against whatever's above it in the z-order, and any pixels a move can preserve are blitted directly rather than queued dirty; only the genuinely-changed pieces end up in the dirty region. This is an internal optimisation with no effect on a task's own redraw handling.

## Icons

Taking inspiration from RISC OS, a window can carry **icons**: rectangular UI elements Wuss draws and hit-tests inside the content area. v1 ships two types:

- `wuss_ICON_TYPE_LABEL` — static text. Clicks fall through to the task as `wuss_EVENT_MOUSE`.
- `wuss_ICON_TYPE_BUTTON` — a bevelled rectangle with a centred label and pressed-state feedback (the bevel inverts and the label shifts one pixel down-right while held). Clicks and hovers arrive as `wuss_EVENT_ICON`.

The enum is left open for sprite and editable-text types later.

Icons are dynamic and owned by their window:

- `wuss_icon_create(window, spec, &icon)` — returns an opaque `wuss_icon_t *`. The spec gives the bounding box, type, text (copied; `NULL` treated as `""`), foreground and background palette indices, and flags. A `wuss_ICON_TYPE_BUTTON` must pass a real `bg`; passing `wuss_NO_BACKGROUND` is rejected with `result_WUSS_BAD_ICON`. An unknown type is also `result_WUSS_BAD_ICON`; an out-of-range `fg`/`bg` is `result_WUSS_BAD_COLOUR`.
- `wuss_icon_delete(icon)` — NULL-safe.
- `wuss_icon_set_text(icon, text)`, `wuss_icon_set_hidden(icon, hidden)`.
- Getters: `wuss_icon_get_bbox`, `wuss_icon_get_type`, `wuss_icon_get_text` (never `NULL`), `wuss_icon_get_window`.

Flags: `wuss_ICON_FLAGS_HIDDEN` (not drawn, not hit-tested) and `wuss_ICON_FLAGS_DISABLED` (drawn greyed; clicks fall through as `wuss_EVENT_MOUSE`).

An icon's bounding box is in **virtual content space** — the same space as `wuss_EVENT_MOUSE`'s `point` and `wuss_window_invalidate`'s box — so icons scroll with the content. The on-screen box is `content-top-left - scroll + bbox`.

Wuss draws icons in creation order (later icons paint on top); hit-testing scans in reverse, so the topmost icon at a point wins. When the pointer leaves a pressed button its pressed state clears; v1 does not re-press on drag-back-in and does not track which mouse button is held.

The bevel's light (top/left) and dark (bottom/right) edge shades come from `config->bevel.light` / `config->bevel.dark` at `wuss_create` time, validated like the other furniture colours; both default to the titlebar fill colour when `config` is `NULL`.

## Glossary

Terms as this document and the API use them. Several are RISC OS conventions, which Wuss follows.

- **Adjust** — the secondary mouse button, `wuss_BUTTON_ADJUST`. Conventionally the variant of an action: Adjust on the back button brings a window to front rather than sending it back, and Adjust on a scroll arrow steps against the direction the arrow points.
- **Backdrop** — the desktop background colour painted behind all windows, set by `config->backdrop` at `wuss_create` time. `wuss_NO_BACKGROUND` leaves the area behind windows untouched, making it the caller's to repaint.
- **Button flags** — `wuss_button_t` values are flags (Select 4, Menu 2, Adjust 1), OR'd together so a chord can be reported. Test a reported button with `&`, never for equality.
- **Chord** — two or more mouse buttons held together, e.g. Select+Adjust. Wuss's own furniture handling resolves an ambiguous chord in Select's favour.
- **Content area** — the part of a window belonging to its task. Its bounds are exactly what was passed to `wuss_window_create`, furniture being added outside it; read back with `wuss_window_get_content_bounds`.
- **Dirty region** — the accumulated set of screen-space boxes needing repaint, coalesced as they accumulate. `wuss_redraw_dirty` repaints and clears it.
- **Document extent** — `doc`, the size of a task's virtual content space, fixed at window creation. Sets how far a window can scroll, the scrollbar sausages' proportions, and the size a resize-drag or toggle-size can grow the content area to.
- **Minimum extent** — `min_doc`, the smallest content size a resize-drag or toggle-size will leave a window at, fixed at window creation. `(0, 0)` means the built-in floor.
- **Furniture** — everything Wuss draws around a window's content: outline, titlebar and its buttons, scrollbars, resize button. Drawn outside the content area, never carved out of it. Furniture clicks are handled entirely within Wuss and never reach the task.
- **Furniture button** — a clickable furniture region in the titlebar or window corner: close, back, toggle-size, resize. (Called an "icon" in earlier revisions; that name now means the work-area element below.)
- **Handle callback** — a task's single `wuss_event_fn_t`, receiving every event kind and dispatching on `event->kind`. A window whose task has no handle receives no events at all.
- **Icon** — a rectangular UI element Wuss draws and hit-tests inside a window's content area: a static `wuss_ICON_TYPE_LABEL`, or a clickable bevelled `wuss_ICON_TYPE_BUTTON`. Created with `wuss_icon_create` and owned by its window. Its bounding box is in virtual content space, so it scrolls with the content; its screen position is `content-top-left - scroll + bbox`. See "Icons".
- **Invalidate** — mark a region dirty for the next `wuss_redraw_dirty`. Window management does this for its own changes; a task must do it for its own content changes.
- **Menu** — the middle mouse button, `wuss_BUTTON_MENU`. Routed like any other button; Wuss provides no menu widget of its own.
- **Outline** — the 1px border drawn around a window, suppressed by `wuss_WINDOW_NO_OUTLINE`.
- **Sausage** — the draggable thumb within a scrollbar well, sized in proportion to how much of the document extent the content area shows.
- **Screen space** — coordinates in the underlying `screen_t`, origin at its top-left. Visible and content bounds are in screen space.
- **Select** — the primary mouse button, `wuss_BUTTON_SELECT`. Performs the plain action, and raises a window when used on its titlebar.
- **Task** — the client of a window: an event callback plus an opaque `task_data` pointer, held in a `wuss_task_t`. Wuss owns the window; the task owns what's drawn inside it.
- **Titlebar** — the strip above the content area carrying the window's label and its buttons, and the drag handle for moving the window. Suppressed by `wuss_WINDOW_NO_TITLEBAR`.
- **Toggle size** — the titlebar button that switches a window between its normal size and a maximised size, and back.
- **Virtual content space** — the task's own full coordinate space, of size `doc`. Mouse and scroll events arrive in it, i.e. with the scroll offset already added.
- **Visible bounds** — a window's whole on-screen footprint, content plus furniture; `wuss_window_get_visible_bounds`.
- **Well** — the track a scrollbar's sausage slides along, between the two arrow buttons.
- **Window-local coordinates** — coordinates relative to the content area's top-left. `wuss_window_invalidate` takes its box in virtual content space, i.e. with the scroll offset already added.
- **Z-order** — the back-to-front stacking order of windows. Changed with `wuss_window_restack`, or by a Select click on a titlebar; content clicks never change it.

## Limitations

- No menus: `wuss_BUTTON_MENU` is defined and routed like any other button, but Wuss has no built-in menu widget.
