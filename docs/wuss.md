# [DPTLib](https://github.com/dpt/DPTLib) > wuss

"wuss" is a minimal window manager. It owns window creation, positioning, sizing, z-ordering, mouse/scroll event routing and dragging. Window contents are entirely delegated to tasks, which supply a redraw callback and (optionally) mouse and scroll callbacks.

- It draws windows back-to-front, with an optional titlebar and 1px outline per window.
- It hit-tests and routes mouse-down/up/move and scroll events, including titlebar drag-to-move.
- It tracks a single dirty region, accumulated automatically by window management (create/destroy/move/resize/bring-to-front) and manually by tasks, for partial redraws.
- Content drawing and mouse/scroll handling are entirely task-supplied; wuss only fills the content background before calling the task's redraw callback (unless the task opts out with `wuss_NO_BACKGROUND`).

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

`font` and `palette` may both be NULL, for unlabelled titlebars and a built-in default palette respectively. `config` may be NULL for default titlebar height/colours.

Destroy with `wuss_destroy`, which also destroys any windows still open on it.

## Windows

Create a window with a content bounding box, optional title, appearance flags and a task delegate:

```C
result_t wuss_window_create(wuss_t *wuss, const box_t *content, const char *title,
                            wuss_window_flags_t flags, const wuss_task_t *task,
                            wuss_window_t **window);
```

Furniture is additional to `content`, not carved out of it: the window's content area always ends up exactly the box requested, and its on-screen footprint (`wuss_window_get_visible_bounds`) is `content` expanded outward by whatever furniture flags request — a titlebar above, and/or a 1px outline around all four sides.

`wuss_task_t` holds the task's callbacks and its content background:

```C
typedef struct wuss_task
{
  wuss_redraw_fn_t *redraw;      /* NULL => content area left blank */
  wuss_mouse_fn_t  *mouse;       /* NULL => content mouse events dropped */
  wuss_scroll_fn_t *scroll;      /* NULL => content scroll events dropped; not set by wuss_task_make */
  void             *task_data;
  wuss_colour_t     bg;          /* filled by wuss before redraw, or wuss_NO_BACKGROUND */
}
wuss_task_t;
```

`wuss_task_make` builds one from `redraw`/`mouse`/`task_data`/`bg`; `scroll` isn't a parameter, so assign it directly on the result if the task needs it.

`flags` combines, by bitwise OR:

- `wuss_WINDOW_NONE` — default furniture: titlebar and outline.
- `wuss_WINDOW_NO_TITLEBAR` — no titlebar, and no drag handle.
- `wuss_WINDOW_NO_OUTLINE` — no 1px border around the window.

Other window operations: `wuss_window_destroy`, `wuss_window_move`, `wuss_window_resize` (preserves content top-left), `wuss_window_bring_to_front`, `wuss_window_send_to_back`, `wuss_window_get_visible_bounds` (full on-screen footprint), `wuss_window_get_content_bounds`, `wuss_window_set_background`.

## Task callbacks

```C
typedef result_t (wuss_redraw_fn_t)(wuss_window_t *window, screen_t *scr,
                                    const box_t *content, void *task_data);

typedef result_t (wuss_mouse_fn_t)(wuss_window_t *window, wuss_mouse_action_t action,
                                   int x, int y, wuss_button_t button, void *task_data);

typedef result_t (wuss_scroll_fn_t)(wuss_window_t *window, int x, int y,
                                    int delta, void *task_data);
```

`redraw` is called with `scr->clip` already set to the on-screen, clipped content area; `content` gives the window's full (unclipped) content box in screen space, for context.

`mouse` and `scroll` receive `x`/`y` in window-local content coordinates: the content area's top-left is `(0,0)` plus the window's current scroll offset (see "Scrolling" below), for `wuss_MOUSE_DOWN`/`wuss_MOUSE_UP`/`wuss_MOUSE_MOVE` and scroll respectively. A titlebar click never reaches a task's mouse callback: it starts a drag (and, for `wuss_BUTTON_SELECT`, brings the window to front) instead. A content click, even on a `wuss_WINDOW_NO_TITLEBAR` window with no drag handle, never changes z-order — only a titlebar click raises a window — so tasks are free to use content clicks for their own purposes without wuss reordering windows underneath them.

## Mouse and scroll routing

Feed mouse events in with `wuss_mouse_click` (action `wuss_MOUSE_DOWN` or `wuss_MOUSE_UP`) and `wuss_mouse_move`, and scroll events with `wuss_scroll`, each hit-testing the topmost window at `(x, y)` and delivering to its task in window-local coordinates. All three take an optional `wuss_window_t **hit` out-parameter naming the window under the pointer (or being dragged). `wuss_scroll` is dropped if the hit window has no scroll callback, or the pointer is over its titlebar.

## Scrolling

Each window carries a scroll offset, `(0, 0)` by default: the point in the task's virtual content space that appears at the content area's top-left. `wuss_window_set_scroll(window, x, y)` moves it (invalidating the content area so the next redraw picks it up); `wuss_window_get_scroll` reads it back. wuss itself doesn't clip, pan pixels, or know how big a task's content actually is — a task determines its own extent and clamps as it sees fit — it just plumbs the offset through:

- window-local `x`/`y` delivered to `mouse`/`scroll` callbacks (and expected in `wuss_window_invalidate`'s `local_box`) are in virtual content space, i.e. already shifted by the scroll offset.
- `redraw` still receives `content` as the on-screen (unscrolled) content box; a task reads the offset itself via `wuss_window_get_scroll` to work out which part of its content to paint there.

## Redrawing

- `wuss_redraw` repaints every window, back-to-front, unconditionally.
- `wuss_invalidate` / `wuss_window_invalidate` mark a screen-space or window-local region dirty; window management calls these automatically for its own changes, but a task must call one of them itself whenever its content changes on its own (e.g. an animation), passing the union of the old and new areas that need repainting.
- `wuss_redraw_dirty` repaints only the accumulated dirty region, then clears it. wuss only repaints windows, not the background between/behind them, so a caller whose invalidation can expose background (e.g. after a window move) should clear that region itself first.
- `wuss_get_dirty` fetches the current accumulated dirty region without redrawing.

## Limitations

- No menus: `wuss_BUTTON_MENU` is defined and routed like any other button, but wuss has no built-in menu widget.
- No window resizing gesture (drag-to-resize) built in; `wuss_window_resize` exists but callers must drive it themselves.
- No overlapping-window damage tracking finer than each window's own bounding box.
