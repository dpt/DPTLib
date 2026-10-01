/* framebuf/bmfont/enumerate.c -- list the bitmap fonts under a directory */

#include <stddef.h>
#include <stdio.h>

#include "base/result.h"
#include "framebuf/bmfont.h"
#include "framebuf/bmfontfamily.h"
#include "io/dirscan.h"
#include "io/path.h"

/* ----------------------------------------------------------------------- */

typedef struct
{
  char                  dir[512]; /* own copy: see bmfont_enumerate */
  bmfont_enumerate_fn  *fn;
  void                 *opaque;
}
bmfont_enumerate_ctx_t;

static result_t bmfont_enumerate_entry(const char *leaf, void *opaque)
{
  bmfont_enumerate_ctx_t *ctx;
  result_t                rc;
  bmfontfamily_t         *family;
  const bmfontface_t     *face;
  int                     i;
  char                    family_dir[512];
  char                    name[512];

  ctx = opaque;

  /* Copied out of pathf's shared static buffer immediately: one call per
   * entry here would otherwise repeatedly clobber it from under the
   * caller's own "dir" pointer. */
  snprintf(family_dir, sizeof(family_dir), "%s",
           pathf("%s/%s", ctx->dir, leaf));

  rc = bmfontfamily_scan(family_dir, &family);
  if (rc == result_NOT_FOUND)
    return result_OK; /* a directory with no fonts in it: not a family */
  if (rc != result_OK)
    return rc;

  for (i = 0; i < bmfontfamily_count(family); i++)
  {
    face = bmfontfamily_face(family, i);
    snprintf(name, sizeof(name), "%s %s", bmfontfamily_name(family),
             face->style);

    rc = ctx->fn(name, face->path, ctx->opaque);
    if (rc != result_OK)
      break;
  }

  bmfontfamily_destroy(family);

  return rc;
}

result_t bmfont_enumerate(const char          *dir,
                          bmfont_enumerate_fn *fn,
                          void                *opaque)
{
  bmfont_enumerate_ctx_t ctx;

  if (dir == NULL || fn == NULL)
    return result_NULL_ARG;

  /* own copy: dir may point into pathf's shared static buffer, which the
   * per-entry pathf call below would otherwise clobber mid-walk */
  snprintf(ctx.dir, sizeof(ctx.dir), "%s", dir);
  ctx.fn     = fn;
  ctx.opaque = opaque;

  /* ctx.dir, not dir: dirscan_walk_dirs holds this pointer across the whole
   * scan, calling bmfont_enumerate_entry (and its pathf call) between
   * reads -- passing the original dir would let that clobber it mid-walk
   * if it still pointed into pathf's shared buffer. */
  return dirscan_walk_dirs(ctx.dir, bmfont_enumerate_entry, &ctx);
}
