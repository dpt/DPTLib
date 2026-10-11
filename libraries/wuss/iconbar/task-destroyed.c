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

  for (i = wuss->iconbar.n - 1; i >= 0; i--)
  {
    if (wuss->iconbar.icons[i]->owner == task)
      wuss_iconbar_icon_destroy(wuss, wuss->iconbar.icons[i]);
  }
}

void wuss__iconbar_free(wuss_t *wuss)
{
  int i;

  for (i = 0; i < wuss->iconbar.n; i++)
  {
    wuss__free(wuss, (char *) wuss->iconbar.icons[i]->spec.text);
    wuss__free(wuss, wuss->iconbar.icons[i]);
  }

  wuss__free(wuss, wuss->iconbar.icons);
  wuss->iconbar.icons     = NULL;
  wuss->iconbar.n    = 0;
  wuss->iconbar.cap = 0;
}

#endif /* WUSS_ICONBAR */
