/* wuss/furniture/click.c -- wuss - pointer clicks on window furniture */

#include "../core/types.h"
#include "../core/dispatch.h"
#include "../core/geometry.h"
#include "../core/invalidate.h"

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

wuss_window_t *wuss__furniture_release(wuss_t             *wuss,
                                       wuss_mouse_action_t action)
{
  wuss_window_t *win;

  win = wuss->furniture.dragging;
  if (action != wuss_MOUSE_UP || win == NULL)
    return NULL;

  set_pressed_region(win, wuss_FURNITURE_NONE);

  wuss->furniture.dragging  = NULL;
  wuss->furniture.drag_kind = wuss_FURNITURE_DRAG_NONE;

  return win;
}

int wuss__furniture_click(wuss_window_t      *win,
                          point_t             p,
                          wuss_button_t       button,
                          wuss_mouse_action_t action,
                          result_t           *rc)
{
  wuss_t                 *wuss;
  wuss_furniture_region_t region;
  wuss_event_t            event;

  wuss = win->wuss;

  region = wuss->furniture_ops->hit_test(win, p);

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
    *rc = wuss_window_try_close(win);
    return 1;
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
    *rc = wuss__deliver(win->task, win, &event);
    return 1;
  }

  if (region == wuss_FURNITURE_BACK && action == wuss_MOUSE_DOWN)
  {
    if (button & wuss_BUTTON_SELECT)
      wuss_window_restack(win, wuss_ZORDER_BACK);
    else if (button & wuss_BUTTON_ADJUST)
      wuss_window_restack(win, wuss_ZORDER_FRONT);

    if (button & (wuss_BUTTON_SELECT | wuss_BUTTON_ADJUST))
      press_furniture(win, region);
    *rc = result_OK;
    return 1;
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
    *rc = result_OK;
    return 1;
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
      wuss->furniture.drag.x     = p.x - content.x0;
      wuss->furniture.drag.y     = p.y - content.y0;
    }
    *rc = result_OK;
    return 1;
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
          if (p.y < sausage.y0)
          {
            wuss__scroll_step(win, POINT(0, -page * sign));
            paged = 1;
          }
          else if (p.y > sausage.y1)
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
          if (p.x < sausage.x0)
          {
            wuss__scroll_step(win, POINT(-page * sign, 0));
            paged = 1;
          }
          else if (p.x > sausage.x1)
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
        wuss->furniture.drag.x            = p.x;
        wuss->furniture.drag.y            = p.y;
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
          wuss->furniture.drag_offset.x = p.x - content.x1;
          wuss->furniture.drag_offset.y = p.y - content.y1;

          set_pressed_region(win, region);
        }
      }
    }
    *rc = result_OK;
    return 1;
  }

  return 0;
}
