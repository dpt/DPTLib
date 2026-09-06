/* framebuf/bmfont/enumerate.c -- list the bitmap fonts in a directory */

#include <stddef.h>
#include <stdio.h>
#include <string.h>

#include "base/result.h"
#include "framebuf/bmfont.h"
#include "io/dirscan.h"

/* ----------------------------------------------------------------------- */

#define BMFONT_EXT     ".png"
#define BMFONT_EXT_LEN 4

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
  size_t                   leaflen;
  char                     name[256];
  char                     path[512];

  ctx     = opaque;
  leaflen = strlen(leaf);

  if (leaflen <= BMFONT_EXT_LEN || leaflen - BMFONT_EXT_LEN >= sizeof(name))
    return result_OK;
  if (strcmp(leaf + leaflen - BMFONT_EXT_LEN, BMFONT_EXT) != 0)
    return result_OK;

  memcpy(name, leaf, leaflen - BMFONT_EXT_LEN);
  name[leaflen - BMFONT_EXT_LEN] = '\0';

#ifdef TARGET_RISCOS
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
