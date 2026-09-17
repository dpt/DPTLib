/* framebuf/bmfont/enumerate.c -- list the bitmap fonts in a directory */

#include <stddef.h>
#include <stdio.h>

#include "base/result.h"
#include "framebuf/bmfont.h"
#include "io/dirscan.h"
#include "io/path.h"

/* ----------------------------------------------------------------------- */

#define BMFONT_EXT ".png"

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
  char                    name[256];
  char                    path[512];

  ctx = opaque;

  if (!path_leaf_strip_ext(leaf, BMFONT_EXT, name, sizeof(name)))
    return result_OK;

  /* Copied out of pathf's shared static buffer immediately: one call per
   * entry here would otherwise repeatedly clobber it from under the
   * caller's own "dir" pointer. */
  snprintf(path, sizeof(path), "%s", pathf("%s/%s", ctx->dir, leaf));

  return ctx->fn(name, path, ctx->opaque);
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

  /* ctx.dir, not dir: dirscan_walk holds this pointer across the whole
   * scan, calling bmfont_enumerate_entry (and its pathf call) between
   * reads -- passing the original dir would let that clobber it mid-walk
   * if it still pointed into pathf's shared buffer. */
  return dirscan_walk(ctx.dir, bmfont_enumerate_entry, &ctx);
}
