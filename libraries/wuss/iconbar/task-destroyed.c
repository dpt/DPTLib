/* wuss/iconbar/task-destroyed.c -- remove a task's icon bar icons */

#include <string.h>

#ifdef FORTIFY
#include "fortify/fortify.h"
#endif

#include "../core/impl.h"

#ifdef WUSS_ICONBAR

void wuss__iconbar_task_destroyed(wuss_t *wuss, wuss_task_t *task)
{
  int i;

  for (i = wuss->niconbar_icons - 1; i >= 0; i--)
  {
    if (wuss->iconbar_icons[i]->owner == task)
      wuss_iconbar_icon_destroy(wuss, wuss->iconbar_icons[i]);
  }
}

void wuss__iconbar_free(wuss_t *wuss)
{
  int i;

  for (i = 0; i < wuss->niconbar_icons; i++)
  {
    wuss__free(wuss, (char *) wuss->iconbar_icons[i]->spec.text);
    wuss__free(wuss, wuss->iconbar_icons[i]);
  }

  wuss__free(wuss, wuss->iconbar_icons);
  wuss->iconbar_icons     = NULL;
  wuss->niconbar_icons    = 0;
  wuss->cap_iconbar_icons = 0;
}

#endif /* WUSS_ICONBAR */
