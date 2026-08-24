/* window-create.c -- wuss - minimal window manager */

#include <assert.h>
#include <stdlib.h>
#include <string.h>

#ifdef FORTIFY
#include "fortify/fortify.h"
#endif

#include "impl.h"

result_t wuss_window_create(wuss_t *wuss, const box_t *visible, const char *title, const wuss_client_t *client, wuss_window_t **window)
{
  wuss_window_t *win;
  int             width, height;

  assert(wuss    != NULL);
  assert(visible != NULL);
  assert(window  != NULL);

  width  = visible->x1 - visible->x0;
  height = visible->y1 - visible->y0;
  if (!wuss__size_ok(width, height, wuss->titlebar_height))
    return result_WUSS_TOO_SMALL;

  win = malloc(sizeof(*win));
  if (win == NULL)
    return result_OOM;

  win->wuss    = wuss;
  win->visible = *visible;

  if (client != NULL)
    win->client = *client;
  else
    memset(&win->client, 0, sizeof(win->client));

  if (title != NULL)
  {
    strncpy(win->title, title, WUSS_TITLE_MAX);
    win->title[WUSS_TITLE_MAX] = '\0';
  }
  else
  {
    win->title[0] = '\0';
  }

  list_add_to_head(&wuss->z_order, &win->link);

  *window = win;

  return result_OK;
}
