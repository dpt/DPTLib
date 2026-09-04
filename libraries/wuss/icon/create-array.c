/* wuss/icon/create-array.c -- create several work-area icons at once */

#include <assert.h>
#include <stddef.h>

#ifdef FORTIFY
#include "fortify/fortify.h"
#endif

#include "../core/impl.h"

result_t wuss_icon_create_array(wuss_window_t          *window,
                                const wuss_icon_spec_t *specs,
                                int                     nspecs,
                                wuss_icon_t           **icons)
{
  result_t rc;
  int      i;
  int      j;

  assert(window != NULL);
  assert(specs  != NULL || nspecs == 0);

  for (i = 0; i < nspecs; i++)
  {
    wuss_icon_t *it;

    rc = wuss_icon_create(window, &specs[i], &it);
    if (rc != result_OK)
    {
      /* all-or-nothing: unwind the icons this call already created. They are
       * the last (i) entries on the window's icon list, newest last. */
      for (j = 0; j < i; j++)
        wuss_icon_delete(window->icons[window->nicons - 1]);
      return rc;
    }

    if (icons != NULL)
      icons[i] = it;
  }

  return result_OK;
}
