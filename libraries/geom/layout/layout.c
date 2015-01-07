/* layout.c -- laying out elements using the packer */

#include <stddef.h>

#include "utils/minmax.h"
#include "geom/packer.h"

#include "geom/layout.h"

/* ----------------------------------------------------------------------- */

result_t layout_place(const layout_spec_t    *spec,
                      const layout_element_t *elements,
                      int                     nelements,
                      box_t                  *boxes,
                      int                     nboxes)
{
  result_t err = result_OK;
  int      j;
  int      i;
  int      chosen_width[32];

  j = 0;

  for (i = 0; i < nelements; )
  {
    int nextw;
    int first;
    int clear;
    int k;

    nextw = packer_next_width(spec->packer, spec->loc);

    first = i; /* upper bound 'i' is exclusive */

    clear = 0;

    if (elements[first].type == layout_BOX)
    {
      int remaining;
      int need;

      remaining = nextw;

      /* fit as many items on the line as possible */

      /* skip the padding on the initial element */
      need = elements[i].data.box.min_width;
      while (remaining >= need)
      {
        /* MAYBE if (need > elements[i].data.box.max_width) bad spec; */

        chosen_width[i] = MIN(remaining, elements[i].data.box.max_width);
        remaining -= chosen_width[i];

        if (++i >= nelements)
          break;

        if (elements[i].type != layout_BOX)
          break;

        need = spec->spacing + elements[i].data.box.min_width;
      }

      if (remaining < need)
        clear = 1;



      /* place vertical padding */

      if (first > 0 || /* don't pad at the top */
          elements[0].type == layout_NEWLINE /* unless explicit */)
      {
        err = packer_place_by(spec->packer,
                              spec->loc,
                              nextw,
                              spec->leading,
                              NULL);
        if (err == result_PACKER_DIDNT_FIT)
          break;
        else if (err)
          goto failure;

        if (elements[first].type == layout_NEWLINE)
          continue;
      }

      for (k = first; k < i; k++)
      {
        const box_t *placed;

        if (k > first)
        {
          /* place horizontal padding */

          err = packer_place_by(spec->packer,
                                spec->loc,
                                spec->spacing,
                                elements[k].data.box.height,
                                NULL);
          if (err == result_PACKER_DIDNT_FIT)
            break;
          else if (err)
            goto failure;
        }

        /* place element */

        err = packer_place_by(spec->packer,
                              spec->loc,
                              chosen_width[k],
                              elements[k].data.box.height,
                             &placed);
        if (err == result_PACKER_DIDNT_FIT)
          break;
        else if (err)
          goto failure;

        if (placed)
        {
          if (j == nboxes)
          {
            err = result_LAYOUT_BUFFER_FULL;
            goto failure;
          }

          boxes[j++] = *placed;
        }
      }

      if (err == result_PACKER_DIDNT_FIT)
        break;
      else if (err)
        goto failure;
    }
    else if (elements[first].type == layout_NEWLINE)
    {
      i++;
      clear = 1;
    }

    if (clear)
    {
      /* there's space, but it's not enough for the next element - start a
       * new line */

      err = packer_clear(spec->packer, spec->clear);
      if (err)
        goto failure;
    }
  }

  return result_OK;


failure:

  return err;
}
