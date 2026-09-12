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
  const char          *dir;
  bmfont_enumerate_fn *fn;
  void                *opaque;
}
bmfont_enumerate_ctx_t;

static result_t bmfont_enumerate_entry(const char *leaf, void *opaque)
{
  bmfont_enumerate_ctx_t *ctx;
  char                     name[256];
  char                     path[512];

  ctx = opaque;

  if (!path_leaf_strip_ext(leaf, BMFONT_EXT, name, sizeof(name)))
    return result_OK;

  /* Built locally rather than via path_join_filename: that returns a single
   * static buffer, which one call per entry here would repeatedly clobber
   * from under the caller's own "dir" pointer. */
#ifdef __riscos
  snprintf(path, sizeof(path), "%s.%s", ctx->dir, leaf);
#else
  snprintf(path, sizeof(path), "%s/%s", ctx->dir, leaf);
#endif

  return ctx->fn(name, path, ctx->opaque);
}

result_t bmfont_enumerate(const char          *dir,
                          bmfont_enumerate_fn *fn,
                          void                *opaque)
{
  bmfont_enumerate_ctx_t ctx;

  if (dir == NULL || fn == NULL)
    return result_NULL_ARG;

  ctx.dir    = dir;
  ctx.fn     = fn;
  ctx.opaque = opaque;

  return dirscan_walk(dir, bmfont_enumerate_entry, &ctx);
}
