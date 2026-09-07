/* io/namelist.c -- collect matching dir leafnames into a fixed table */

#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include "base/result.h"
#include "io/dirscan.h"

#include "io/namelist.h"

/* ----------------------------------------------------------------------- */

typedef struct namelist_ctx
{
  char       *names;   /* base of the cap-by-stride table */
  size_t      stride;
  int         cap;
  const char *ext;     /* NULL or "" for "match everything" */
  size_t      extlen;
  int         count;
}
namelist_ctx_t;

static result_t namelist_entry(const char *leaf, void *opaque)
{
  namelist_ctx_t *ctx;
  size_t          leaflen;
  size_t          namelen;

  ctx     = opaque;
  leaflen = strlen(leaf);

  if (ctx->count >= ctx->cap)
    return result_STOP_WALK;

  if (ctx->extlen > 0)
  {
    if (leaflen <= ctx->extlen ||
        strcmp(leaf + leaflen - ctx->extlen, ctx->ext) != 0)
      return result_OK;
    namelen = leaflen - ctx->extlen;
  }
  else
  {
    namelen = leaflen;
  }

  if (namelen + 1 > ctx->stride)
    return result_OK; /* would not fit its slot */

  memcpy(ctx->names + (size_t) ctx->count * ctx->stride, leaf, namelen);
  ctx->names[(size_t) ctx->count * ctx->stride + namelen] = '\0';
  ctx->count++;

  return result_OK;
}

static int namelist_cmp(const void *a, const void *b)
{
  return strcmp(a, b);
}

result_t namelist_scan(const char *dir,
                       const char *ext,
                       char       *names,
                       size_t      stride,
                       int         cap,
                       int         sorted,
                       int        *pcount)
{
  result_t       rc;
  namelist_ctx_t ctx;

  if (dir == NULL || names == NULL || pcount == NULL)
    return result_NULL_ARG;
  if (stride < 2 || cap < 0)
    return result_BAD_ARG;

  ctx.names  = names;
  ctx.stride = stride;
  ctx.cap    = cap;
  ctx.ext    = ext;
  ctx.extlen = (ext != NULL) ? strlen(ext) : 0;
  ctx.count  = 0;

  rc = dirscan_walk(dir, namelist_entry, &ctx);
  if (rc != result_OK)
    return rc;

  if (sorted && ctx.count > 1)
    qsort(names, (size_t) ctx.count, stride, namelist_cmp);

  *pcount = ctx.count;

  return result_OK;
}
