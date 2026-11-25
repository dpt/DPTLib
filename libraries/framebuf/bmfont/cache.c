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
  const alloc_t       *alloc; /* for the cache, its entries and its fonts */
  bmfontcache_entry_t *entries;
  int                   used;
  int                   allocated;
};

/* ----------------------------------------------------------------------- */

result_t bmfontcache_create(const alloc_t *alloc, bmfontcache_t **cache)
{
  bmfontcache_t *c;

  if (alloc == NULL)
    alloc = &alloc_default;

  c = alloc->malloc(sizeof(*c));
  if (c == NULL)
    return result_OOM;
  memset(c, 0, sizeof(*c));

  c->alloc = alloc;

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
    cache->alloc->free(cache->entries[i].path);
  }

  cache->alloc->free(cache->entries);
  cache->alloc->free(cache);
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
  size_t               len;
  char                *owned_path;
  bmfont_t            *font;

  entry = bmfontcache_find_by_path(cache, path);
  if (entry != NULL)
  {
    entry->refcount++;
    *bmfont = entry->font;
    return result_OK;
  }

  len        = strlen(path) + 1;
  owned_path = cache->alloc->malloc(len);
  if (owned_path == NULL)
    return result_OOM;
  memcpy(owned_path, path, len);

  rc = bmfont_create(cache->alloc, path, &font);
  if (rc != result_OK)
  {
    cache->alloc->free(owned_path);
    return rc;
  }

  if (array_grow_alloc(cache->alloc,
                       (void **) &cache->entries, sizeof(*cache->entries),
                       cache->used, &cache->allocated, 1, 4))
  {
    bmfont_destroy(font);
    cache->alloc->free(owned_path);
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
  cache->alloc->free(entry->path);

  array_delete_element(cache->entries, sizeof(*cache->entries), cache->used,
                       (int) (entry - cache->entries));
  cache->used--;
}
