/* framebuf/bmfont/cache.c -- shared bitmap font loader */

#include <stdlib.h>
#include <string.h>

#include "base/result.h"
#include "framebuf/bmfontcache.h"
#include "utils/array.h"

/* ----------------------------------------------------------------------- */

typedef struct bmfontcache_entry
{
  char      *path; /* owned copy of the acquire path */
  bmfont_t  *font;
  int        refcount;
}
bmfontcache_entry_t;

struct bmfontcache
{
  bmfontcache_entry_t *entries;
  int                   used;
  int                   allocated;
};

/* ----------------------------------------------------------------------- */

result_t bmfontcache_create(bmfontcache_t **cache)
{
  bmfontcache_t *c;

  c = calloc(1, sizeof(*c));
  if (c == NULL)
    return result_OOM;

  *cache = c;

  return result_OK;
}

/* ----------------------------------------------------------------------- */

void bmfontcache_destroy(bmfontcache_t *cache)
{
  int i;

  if (cache == NULL)
    return;

  for (i = 0; i < cache->used; i++)
  {
    bmfont_destroy(cache->entries[i].font);
    free(cache->entries[i].path);
  }

  free(cache->entries);
  free(cache);
}

/* ----------------------------------------------------------------------- */

static bmfontcache_entry_t *bmfontcache_find_by_path(bmfontcache_t *cache,
                                                     const char    *path)
{
  int i;

  for (i = 0; i < cache->used; i++)
    if (strcmp(cache->entries[i].path, path) == 0)
      return &cache->entries[i];

  return NULL;
}

static bmfontcache_entry_t *bmfontcache_find_by_font(bmfontcache_t  *cache,
                                                     const bmfont_t *font)
{
  int i;

  for (i = 0; i < cache->used; i++)
    if (cache->entries[i].font == font)
      return &cache->entries[i];

  return NULL;
}

/* ----------------------------------------------------------------------- */

result_t bmfontcache_acquire(bmfontcache_t *cache,
                             const char    *path,
                             bmfont_t     **bmfont)
{
  result_t             rc;
  bmfontcache_entry_t *entry;
  char                *owned_path;
  bmfont_t            *font;

  entry = bmfontcache_find_by_path(cache, path);
  if (entry != NULL)
  {
    entry->refcount++;
    *bmfont = entry->font;
    return result_OK;
  }

  owned_path = strdup(path);
  if (owned_path == NULL)
    return result_OOM;

  rc = bmfont_create(path, &font);
  if (rc != result_OK)
  {
    free(owned_path);
    return rc;
  }

  if (array_grow((void **) &cache->entries, sizeof(*cache->entries),
                 cache->used, &cache->allocated, 1, 4))
  {
    bmfont_destroy(font);
    free(owned_path);
    return result_OOM;
  }

  entry           = &cache->entries[cache->used++];
  entry->path     = owned_path;
  entry->font     = font;
  entry->refcount = 1;

  *bmfont = font;

  return result_OK;
}

/* ----------------------------------------------------------------------- */

void bmfontcache_release(bmfontcache_t *cache, bmfont_t *bmfont)
{
  bmfontcache_entry_t *entry;

  entry = bmfontcache_find_by_font(cache, bmfont);
  if (entry == NULL)
    return;

  if (--entry->refcount > 0)
    return;

  bmfont_destroy(entry->font);
  free(entry->path);

  array_delete_element(cache->entries, sizeof(*cache->entries), cache->used,
                       (int) (entry - cache->entries));
  cache->used--;
}
