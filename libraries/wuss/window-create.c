/* window-create.c -- wuss - minimal window manager */

#include <assert.h>
#include <stdlib.h>
#include <string.h>

#ifdef FORTIFY
#include "fortify/fortify.h"
#endif

#include "impl.h"

result_t wuss_window_create(wuss_t *wuss, const box_t *content, const char *title, wuss_window_flags_t flags, const wuss_client_t *client, wuss_window_t **window)
{
  wuss_window_t *win;
  int             width, height, outline_px, titlebar_height;

  assert(wuss    != NULL);
  assert(content != NULL);
  assert(window  != NULL);

  width  = content->x1 - content->x0;
  height = content->y1 - content->y0;
  if (!wuss__size_ok(width, height))
    return result_WUSS_TOO_SMALL;

  win = malloc(sizeof(*win));
  if (win == NULL)
    return result_OOM;

  outline_px      = wuss__outline_px_for(flags);
  titlebar_height = wuss__titlebar_height_for(wuss, flags);

  win->wuss       = wuss;
  win->visible.x0 = content->x0 - outline_px;
  win->visible.y0 = content->y0 - outline_px - titlebar_height;
  win->visible.x1 = content->x1 + outline_px;
  win->visible.y1 = content->y1 + outline_px;
  win->flags      = flags;

  if (client != NULL)
    win->client = *client;
  else
    memset(&win->client, 0, sizeof(win->client));

  if (client == NULL)
    win->client.bg = wuss_NO_BACKGROUND;
  else if (client->bg != wuss_NO_BACKGROUND &&
           (client->bg < 0 || client->bg >= wuss->npalette))
  {
    free(win);
    return result_WUSS_BAD_COLOUR;
  }

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

  wuss_invalidate(wuss, &win->visible);

  *window = win;

  return result_OK;
}
