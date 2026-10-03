/* framebuf/effects/tonemap.c -- apply a tone map to a bitmap */

#include "base/result.h"
#include "framebuf/bitmap.h"
#include "framebuf/tonemap.h"

#include "framebuf/effects.h"

#include "impl.h"

/* ----------------------------------------------------------------------- */

result_t effects_tonemap_apply(const bitmap_t *src,
                               bitmap_t       *dst,
                               tonemap_t      *map)
{
  result_t               rc;
  effects_layout_t       layout;
  const tonemap_table_t *red;
  const tonemap_table_t *green;
  const tonemap_table_t *blue;
  const tonemap_table_t *alpha;

  if (map == NULL)
    return result_BAD_ARG;

  rc = effects__check(src, dst, &layout);
  if (rc != result_OK)
    return rc;

  tonemap_get_tables(map, &red, &green, &blue, &alpha);

  effects__remap(src, dst, &layout,
                 red->level, green->level, blue->level, alpha->level);

  return result_OK;
}
