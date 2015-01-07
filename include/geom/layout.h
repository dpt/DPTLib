/* layout.h -- laying out elements using the packer */

#ifndef GEOM_LAYOUT_H
#define GEOM_LAYOUT_H

#include "base/result.h"
#include "geom/box.h"
#include "geom/packer.h"

/* ----------------------------------------------------------------------- */

#define result_LAYOUT_BUFFER_FULL (result_BASE_LAYOUT + 0)

/* ----------------------------------------------------------------------- */


typedef struct layout_element
{
  enum
  {
    layout_BOX,
    layout_NEWLINE,
  }
  type;

  union
  {
    struct
    {
      int min_width, max_width;
      int height;
    }
    box;
  }
  data;
}
layout_element_t;

typedef struct layout_spec
{
  packer_t       *packer;
  packer_loc      loc;
  packer_cleardir clear;
  int             spacing;
  int             leading;
}
layout_spec_t;

result_t layout_place(const layout_spec_t    *spec,
                      const layout_element_t *elements,
                      int                     nelements,
                      box_t                  *boxes,
                      int                     nboxes);

#endif /* GEOM_LAYOUT_H */
