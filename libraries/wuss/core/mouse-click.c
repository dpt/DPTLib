/* wuss/mouse-click.c -- wuss - minimal window manager */

#include "impl.h"

#ifdef WUSS_FURNITURE
/* Move the held-down highlight to "region" (NONE to clear it), repainting
 * the drawn box of whichever region it leaves and whichever it lands on. */
static void set_pressed_region(wuss_window_t          *win,
                               wuss_furniture_region_t region)
{
  wuss_t *wuss;
  box_t   pressed;

  wuss = win->wuss;

  if (wuss->furniture.pressed_region != wuss_FURNITURE_NONE)
  {
    wuss__furniture_pressed_box(win, wuss->furniture.pressed_region, &pressed);
    wuss__invalidate_clipped(win, &pressed);
  }

  wuss->furniture.pressed_region = region;

  if (region != wuss_FURNITURE_NONE)
  {
    wuss__furniture_pressed_box(win, region, &pressed);
    wuss__invalidate_clipped(win, &pressed);
  }
}

/* Hold down a furniture icon that starts no drag: arm "dragging" purely so
 * the MOUSE_UP release path clears the highlight. Not a scroll arrow unless
 * the caller says so afterwards, so no auto-repeat. */
static void press_furniture(wuss_window_t          *win,
                            wuss_furniture_region_t region)
{
  win->wuss->furniture.dragging    = win;
  win->wuss->furniture.drag_kind   = wuss_FURNITURE_DRAG_NONE;
  win->wuss->furniture.repeat_step = 0;
  set_pressed_region(win, region);
}
#endif

static result_t mouse_click(wuss_t             *wuss,
                            point_t             p,
                            wuss_button_t       button,
                            wuss_mouse_action_t action,
                            wuss_window_t     **hit)
{
  wuss_window_t *win;
  wuss_event_t   event;
  int            x, y;

  x = p.x;
  y = p.y;

  wuss->pointer = p;

  /* Core owns all input while a drag is active: releasing the button that
   * started it (any button, if unknown) ends it, dropping on whatever's under
   * the pointer; any other click is swallowed rather than reaching a window
   * underneath the ants. */
  if (wuss->drag.window != NULL)
  {
    win = wuss__window_at(wuss, p);
    if (hit != NULL)
      *hit = win;

    if (action == wuss_MOUSE_UP &&
        (wuss->drag.button == wuss_BUTTON_NONE ||
         (button & wuss->drag.button) != 0))
      wuss__drag_end(wuss, p, win, 0);

    return result_OK;
  }

  /* A captured press owns the pointer until released: a further press or the
   * release goes to the capturing window, wherever it lands. */
  if (wuss->capture != NULL)
  {
    if (hit != NULL)
      *hit = wuss->capture;

    return wuss__capture_deliver(wuss, p, button, action);
  }

  if (action == wuss_MOUSE_DOWN)
    button = wuss__double_click(wuss, wuss__window_at(wuss, p), p, button);

  wuss__icon_release_held(wuss, p, action);

#ifdef WUSS_FURNITURE
  if (action == wuss_MOUSE_UP && wuss->furniture.dragging != NULL)
  {
    win = wuss->furniture.dragging;
    if (hit != NULL)
      *hit = win;

    set_pressed_region(win, wuss_FURNITURE_NONE);

    wuss->furniture.dragging  = NULL;
    wuss->furniture.drag_kind = wuss_FURNITURE_DRAG_NONE;

    return result_OK;
  }
#endif

  win = wuss__window_at(wuss, p);
  if (hit != NULL)
    *hit = win;

  /* A click can land the pointer on a window without a preceding move
   * (window opened under it, or a synthetic tap); keep enter/exit balanced. */
  wuss__pointer_set_window(wuss, win);

  if (wuss__menu_press(wuss, action, button, win))
    return result_OK;

  if (win == NULL)
    return result_OK;

#ifdef WUSS_FURNITURE
  {
    wuss_furniture_region_t region;

    region = wuss->furniture_ops->hit_test(win, POINT(x, y));

    if (region == wuss_FURNITURE_CLOSE  &&
        action == wuss_MOUSE_DOWN       &&
        (button & wuss_BUTTON_SELECT))
    {
      /* Lit even though the window may not survive the call below: a veto
       * leaves it armed for the ordinary MOUSE_UP release below, and a
       * successful close tears the window down and wuss_window_close
       * clears this pressed state. */
      press_furniture(win, region);

      /* User close-icon path: routes through try_close, so the task gets
       * PRE_CLOSE (may veto) then CLOSE and, if not vetoed, wuss tears the
       * window down. A veto's non-OK return propagates to the caller. */
      return wuss_window_try_close(win);
    }

    if (region == wuss_FURNITURE_CLOSE  &&
        action == wuss_MOUSE_DOWN       &&
        (button & wuss_BUTTON_ADJUST))
    {
      /* Lit until the ordinary MOUSE_UP release, as for SELECT; the task
       * may close the window from the handler, which clears it. */
      press_furniture(win, region);

      event.kind                        = wuss_EVENT_CLOSE_ADJUST;
      event.data.close_adjust.modifiers = wuss->modifiers;
      return wuss__deliver(win->task, win, &event);
    }

    if (region == wuss_FURNITURE_BACK && action == wuss_MOUSE_DOWN)
    {
      if (button & wuss_BUTTON_SELECT)
        wuss_window_restack(win, wuss_ZORDER_BACK);
      else if (button & wuss_BUTTON_ADJUST)
        wuss_window_restack(win, wuss_ZORDER_FRONT);

      if (button & (wuss_BUTTON_SELECT | wuss_BUTTON_ADJUST))
        press_furniture(win, region);
      return result_OK;
    }

    if (region == wuss_FURNITURE_TOGGLE_SIZE ||
        region == wuss_FURNITURE_VSCROLL_UP  ||
        region == wuss_FURNITURE_VSCROLL_DOWN ||
        region == wuss_FURNITURE_HSCROLL_LEFT ||
        region == wuss_FURNITURE_HSCROLL_RIGHT)
    {
      if (action == wuss_MOUSE_DOWN &&
          (button & (wuss_BUTTON_SELECT | wuss_BUTTON_ADJUST)))
      {
        /* Adjust-clicking a scroll arrow steps the opposite way to the arrow
         * it points, so one arrow can be worked in both directions without
         * moving the pointer. Toggle-size stays Select-only. */
        int step;

        /* Select wins a Select+Adjust chord, so a chord never scrolls
         * backwards unexpectedly. */
        step = (button & wuss_BUTTON_SELECT) ?  WUSS_SCROLL_STEP
                                             : -WUSS_SCROLL_STEP;

        if (region == wuss_FURNITURE_TOGGLE_SIZE)
        {
          if (button & wuss_BUTTON_SELECT)
            wuss->furniture_ops->toggle_size(win);
        }
        else
        {
          const wuss__furniture_element_t *element;

          element = wuss__furniture_element(region);
          wuss__scroll_step(win, POINT(element->step.x * step,
                                       element->step.y * step));
        }

        /* light the icon/arrow up while held; MOUSE_UP's generic
         * dragging==NULL check above clears it and repaints it plain.
         * Toggle-size takes no action on Adjust (above), so it stays
         * unlit for that button too. */
        if (region != wuss_FURNITURE_TOGGLE_SIZE || (button & wuss_BUTTON_SELECT))
          press_furniture(win, region);

        /* a held arrow auto-repeats from wuss_idle (see wuss__scroll_repeat) */
        if (region != wuss_FURNITURE_TOGGLE_SIZE)
        {
          wuss->furniture.repeat_step   = step;
          wuss->furniture.repeat_frames = WUSS_SCROLL_REPEAT_DELAY;
        }
      }
      return result_OK;
    }

    if (region == wuss_FURNITURE_CLOSE || region == wuss_FURNITURE_TITLE)
    {
      if (action == wuss_MOUSE_DOWN &&
          (button & (wuss_BUTTON_SELECT | wuss_BUTTON_ADJUST)))
      {
        box_t content;

        if (button & wuss_BUTTON_SELECT)
          wuss_window_restack(win, wuss_ZORDER_FRONT);

        wuss__content_box(win, &content);
        wuss->furniture.dragging   = win;
        wuss->furniture.drag_kind  = wuss_FURNITURE_DRAG_MOVE;
        wuss->furniture.drag.x     = x - content.x0;
        wuss->furniture.drag.y     = y - content.y0;
      }
      return result_OK;
    }

    if (region == wuss_FURNITURE_RESIZE ||
        region == wuss_FURNITURE_VSCROLL_WELL ||
        region == wuss_FURNITURE_HSCROLL_WELL)
    {
      if (action == wuss_MOUSE_DOWN &&
          (button & (wuss_BUTTON_SELECT | wuss_BUTTON_ADJUST)))
      {
        point_t scroll;
        int     paged = 0;

        /* Only resize raises the window; dragging a scrollbar well must not
         * reorder the stack. */
        if ((button & wuss_BUTTON_SELECT) && region == wuss_FURNITURE_RESIZE)
          wuss_window_restack(win, wuss_ZORDER_FRONT);

        /* A click landing in the well itself (not on the sausage) pages the
         * content one visible extent towards the click, RISC OS style, keeping
         * one step of overlap. ADJUST reverses that -- it pages away from the
         * click instead, matching RISC OS's SELECT/ADJUST mirroring elsewhere
         * (e.g. the arrow icons). A direct hit on the sausage itself pages
         * nothing and falls through to arm the drag below. */
        if (region == wuss_FURNITURE_VSCROLL_WELL ||
            region == wuss_FURNITURE_HSCROLL_WELL)
        {
          box_t sausage;
          box_t content;
          int   sign, page;

          wuss__content_box(win, &content);
          sign = (button & wuss_BUTTON_ADJUST) ? -1 : 1;

          if (region == wuss_FURNITURE_VSCROLL_WELL)
          {
            wuss__vscroll_sausage_box(win, &sausage);
            page = (content.y1 - content.y0) - WUSS_SCROLL_STEP;
            page = MAX(page, 1);
            if (y < sausage.y0)
            {
              wuss__scroll_step(win, POINT(0, -page * sign));
              paged = 1;
            }
            else if (y > sausage.y1)
            {
              wuss__scroll_step(win, POINT(0, page * sign));
              paged = 1;
            }
          }
          else
          {
            wuss__hscroll_sausage_box(win, &sausage);
            page = (content.x1 - content.x0) - WUSS_SCROLL_STEP;
            page = MAX(page, 1);
            if (x < sausage.x0)
            {
              wuss__scroll_step(win, POINT(-page * sign, 0));
              paged = 1;
            }
            else if (x > sausage.x1)
            {
              wuss__scroll_step(win, POINT(page * sign, 0));
              paged = 1;
            }
          }
        }

        /* A well click that paged must not also arm a sausage drag, or
         * holding the button after the page would start scrolling live as
         * though the pointer had grabbed the sausage itself. A direct hit on
         * the sausage (paged == 0 for a well region) still arms normally. */
        if (!paged)
        {
          wuss_window_get_scroll(win, &scroll);

          wuss->furniture.dragging          = win;
          wuss->furniture.drag_kind         = wuss__furniture_element(region)->drag_kind;
          wuss->furniture.drag.x            = x;
          wuss->furniture.drag.y            = y;
          wuss->furniture.drag_scroll_start = scroll;
          wuss->furniture.drag_both         = (button & wuss_BUTTON_ADJUST) != 0;

          /* Resize needs the pointer's offset from the content box's current
           * bottom-right corner, so the point grabbed on the resize icon stays
           * under the pointer as it moves, rather than that corner jumping to
           * meet the pointer on the very first move. */
          if (region == wuss_FURNITURE_RESIZE)
          {
            box_t content;

            wuss__content_box(win, &content);
            wuss->furniture.drag_offset.x = x - content.x1;
            wuss->furniture.drag_offset.y = y - content.y1;

            set_pressed_region(win, region);
          }
        }
      }
      return result_OK;
    }
  }
#endif /* WUSS_FURNITURE */

  /* A Select/Adjust press on a focusable window's content takes the input
   * focus, before the press itself is delivered. Menu never moves it. */
  if (action == wuss_MOUSE_DOWN                          &&
      (button & (wuss_BUTTON_SELECT | wuss_BUTTON_ADJUST)) &&
      (win->flags & wuss_WINDOW_FOCUSABLE))
    (void) wuss_set_focus(wuss, win);

#ifdef WUSS_ICONBAR
  if (win == wuss->iconbar.window)
  {
    (void) wuss__iconbar_icon_click(wuss, p, button, action);
    return result_OK;
  }
#endif

  if (win->task->handle != NULL)
  {
    box_t   content;
    point_t doc_point;

    wuss__content_box(win, &content);
    doc_point.x = x - content.x0 + win->scroll.x;
    doc_point.y = y - content.y0 + win->scroll.y;

    {
      result_t icon_rc;

      if (wuss__icon_click(win, p, doc_point, button, action, &icon_rc))
        return icon_rc;
    }

    /* Set before delivery: closing the window in the handler clears it. */
    if (action == wuss_MOUSE_DOWN &&
        (button & (wuss_BUTTON_SELECT | wuss_BUTTON_ADJUST)))
      wuss->capture = win;

    event.kind              = wuss_EVENT_MOUSE;
    event.data.mouse.action = action;
    event.data.mouse.point  = doc_point;
    event.data.mouse.button = button;
    return wuss__deliver(win->task, win, &event);
  }

  return result_OK;
}

result_t wuss_mouse_click(wuss_t             *wuss,
                          point_t             p,
                          wuss_button_t       button,
                          wuss_mouse_action_t action,
                          wuss_window_t     **hit)
{
  result_t rc;

  wuss__message_enter(wuss);
  rc = mouse_click(wuss, p, button, action, hit);
  wuss__message_leave(wuss);

  return rc;
}

void wuss_set_modifiers(wuss_t *wuss, wuss_key_modifiers_t modifiers)
{
  wuss->modifiers = modifiers;
}
