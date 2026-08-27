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

`font` and `palette` may both be NULL, for unlabelled titlebars and a built-in default palette respectively. `config` may be NULL for default titlebar height/colours. `wuss_get_font` reads back the font passed in (or NULL), for a task that wants to draw its own content in the same face as titlebars.

Destroy with `wuss_destroy`, which also destroys any windows still open on it.

## Windows

Create a window with a content bounding box, optional title, appearance flags and a task delegate:

```C
result_t wuss_window_create(wuss_t *wuss, const box_t *content, const char *title,
                            wuss_window_flags_t flags, const wuss_task_t *task,
                            int doc_width, int doc_height,
                            wuss_window_t **window);
```

`doc_width`/`doc_height` are the virtual document extent behind the horizontal/vertical scrollbars' sausage proportion; pass `content`'s own width/height for a window with nothing to scroll. Set once at creation, immutable thereafter.

Furniture is additional to `content`, not carved out of it: the window's content area always ends up exactly the box requested, and its on-screen footprint (`wuss_window_get_visible_bounds`) is `content` expanded outward by whatever furniture flags request — a titlebar above, and/or a 1px outline around all four sides.

`wuss_task_t` holds the task's event callback and its content background:

```C
typedef struct wuss_task
{
  wuss_event_fn_t *handle;       /* NULL => task receives no events */
  void            *task_data;
  wuss_colour_t    bg;           /* filled by wuss before a redraw event, or wuss_NO_BACKGROUND */
}
wuss_task_t;
```

`wuss_task_make` builds one from `handle`/`task_data`/`bg`.

`flags` combines, by bitwise OR:

- `wuss_WINDOW_NONE` — default: every furniture region drawn.
- `wuss_WINDOW_NO_TITLEBAR` — no titlebar, and no drag handle.
- `wuss_WINDOW_NO_OUTLINE` — no 1px border around the window.
- `wuss_WINDOW_NO_CLOSE` — no close icon in the titlebar.
- `wuss_WINDOW_NO_BACK` — no back icon in the titlebar (`wuss_BUTTON_SELECT` sends the window to back, `wuss_BUTTON_ADJUST` brings it to front).
- `wuss_WINDOW_NO_TOGGLE_SIZE` — no toggle-size icon in the titlebar.
- `wuss_WINDOW_NO_VSCROLL` — no vertical scrollbar on the right edge.
- `wuss_WINDOW_NO_HSCROLL` — no horizontal scrollbar on the bottom edge.
- `wuss_WINDOW_NO_RESIZE` — no resize icon in the bottom-right corner.
- `wuss_WINDOW_NO_TOGGLE_BLIT` — toggle-size always fully redraws the window's content instead of blitting the preserved region; for a task whose rendering depends on window size in ways a partial redraw can't patch (e.g. a layout that spans the whole window).

`wuss_WINDOW_NO_CLOSE`/`NO_BACK`/`NO_TOGGLE_SIZE` are ignored if `flags` includes `wuss_WINDOW_NO_TITLEBAR`; `NO_VSCROLL`/`NO_HSCROLL`/`NO_RESIZE`/`NO_TOGGLE_BLIT` apply regardless.

All furniture actions (back, toggle-size, resize-drag, scrollbar arrow/thumb) are handled entirely within Wuss via `wuss_mouse_click`/`wuss_mouse_move` — no new client events.

Other window operations: `wuss_window_close`, `wuss_window_move`, `wuss_window_resize` (preserves content top-left), `wuss_window_restack`, `wuss_window_get_visible_bounds` (full on-screen footprint), `wuss_window_get_content_bounds`, `wuss_window_set_background`.

## Task callbacks

```C
typedef enum wuss_event_kind
{
  wuss_EVENT_REDRAW,
  wuss_EVENT_MOUSE,
  wuss_EVENT_SCROLL
}
wuss_event_kind_t;

typedef struct wuss_event
{
  wuss_event_kind_t kind;
  union
  {
    struct { screen_t *scr; const box_t *content; }                        redraw;
    struct { wuss_mouse_action_t action; int x, y; wuss_button_t button; } mouse;
    struct { int x, y, delta; }                                            scroll;
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

## Mouse and scroll routing

Feed mouse events in with `wuss_mouse_click` (action `wuss_MOUSE_DOWN` or `wuss_MOUSE_UP`) and `wuss_mouse_move`, and scroll events with `wuss_scroll`, each hit-testing the topmost window at `(x, y)` and delivering to its task in window-local coordinates. All three take an optional `wuss_window_t **hit` out-parameter naming the window under the pointer (or being dragged). Events are dropped if the hit window has no handle callback, or (for scroll) the pointer is over its titlebar.

## Scrolling

Each window carries a scroll offset, `(0, 0)` by default: the point in the task's virtual content space that appears at the content area's top-left. `wuss_window_set_scroll(window, x, y)` moves it (invalidating the content area so the next redraw picks it up); `wuss_window_get_scroll` reads it back. `wuss_scroll` applies this offset itself as Wuss's default scroll action, clamped to `doc_width`/`doc_height` (set at window creation), before also delivering `wuss_EVENT_SCROLL` to the task if it has a handle:

- window-local `x`/`y` delivered in mouse/scroll events (and expected in `wuss_window_invalidate`'s `local_box`) are in virtual content space, i.e. already shifted by the scroll offset.
- a redraw event's `content` is still the on-screen (unscrolled) content box; a task reads the offset itself via `wuss_window_get_scroll` to work out which part of its content to paint there.

## Redrawing

- `wuss_redraw` repaints every window, back-to-front, unconditionally.
- `wuss_invalidate` / `wuss_window_invalidate` mark a screen-space or window-local region dirty; window management calls these automatically for its own changes, but a task must call one of them itself whenever its content changes on its own (e.g. an animation), passing the union of the old and new areas that need repainting.
- `wuss_redraw_dirty` repaints only the accumulated dirty region, then clears it. Wuss only repaints windows, not the background between/behind them, so a caller whose invalidation can expose background (e.g. after a window move) should clear that region itself first.
- `wuss_get_dirty_count`/`wuss_get_dirty(wuss, index, out)` fetch the currently accumulated dirty regions (coalesced as they accumulate, up to a fixed cap after which further regions are merged into the last one) without redrawing.
- A window move or resize is clipped, piece by piece, against whatever's above it in the z-order, and any pixels a move can preserve are blitted directly rather than queued dirty; only the genuinely-changed pieces end up in the dirty region. This is an internal optimisation with no effect on a task's own redraw handling.

## Limitations

- No menus: `wuss_BUTTON_MENU` is defined and routed like any other button, but Wuss has no built-in menu widget.
