/* io/dirlist/dirlist.c -- sorted directory listing */

#include <stdlib.h>
#include <string.h>

#include "base/result.h"
#include "base/utils.h"
#include "utils/array.h"

#include "io/dirscan.h"
#include "io/filetype.h"
#include "io/path.h"

#include "io/dirlist.h"

/* ----------------------------------------------------------------------- */

struct dirlist
{
  dirlist_entry_t *entries;
  int              count;
  int              cap;
};

/* First pass: every leafname, filed as a (not yet known to be a dir)
 * file entry. Dotfiles are dropped here. */
static result_t dirlist_add_entry(const char *leaf, void *opaque)
{
  dirlist_t       *list;
  dirlist_entry_t *e;
  const char      *dot;

  list = opaque;

  if (leaf[0] == '.')
    return result_OK;

  if (array_grow((void **) &list->entries, sizeof(*list->entries),
                 list->count, &list->cap, 1, 16))
    return result_OOM;

  e = &list->entries[list->count];

  e->leaf = strdup(leaf);
  if (e->leaf == NULL)
    return result_OOM;

  e->is_dir = 0;

  dot = strrchr(leaf, '.');
  e->type = (dot != NULL) ? filetype_from_ext(dot) : filetype_DATA;

  list->count++;

  return result_OK;
}

/* Second pass: mark the entries dirscan_walk_dirs reports as directories.
 * O(entries x dirs), fine for a directory listing's scale; simpler than
 * threading dirscan's platform-specific is-dir knowledge out through a
 * new callback shape shared with every other dirscan_walk caller. */
static result_t dirlist_mark_dir(const char *leaf, void *opaque)
{
  dirlist_t *list;
  int        i;

  list = opaque;

  for (i = 0; i < list->count; i++)
  {
    if (strcmp(list->entries[i].leaf, leaf) == 0)
    {
      list->entries[i].is_dir = 1;
      list->entries[i].type   = filetype_DATA;
      break;
    }
  }

  return result_OK;
}

static int dirlist_cmp(const void *a, const void *b)
{
  const dirlist_entry_t *ea = a;
  const dirlist_entry_t *eb = b;

  return strcasecmp(ea->leaf, eb->leaf);
}

/* ----------------------------------------------------------------------- */

result_t dirlist_scan(const char *dir, dirlist_t **list)
{
  result_t   rc;
  dirlist_t *l;

  if (dir == NULL || list == NULL)
    return result_NULL_ARG;

  l = calloc(1, sizeof(*l));
  if (l == NULL)
    return result_OOM;

  rc = dirscan_walk(dir, dirlist_add_entry, l);
  if (rc == result_OK)
    rc = dirscan_walk_dirs(dir, dirlist_mark_dir, l);

  if (rc != result_OK)
  {
    dirlist_destroy(l);
    return rc;
  }

  if (l->count > 1)
    qsort(l->entries, (size_t) l->count, sizeof(*l->entries), dirlist_cmp);

  *list = l;

  return result_OK;
}

void dirlist_destroy(dirlist_t *list)
{
  int i;

  if (list == NULL)
    return;

  for (i = 0; i < list->count; i++)
    free(list->entries[i].leaf);

  free(list->entries);
  free(list);
}

int dirlist_count(const dirlist_t *list)
{
  return (list == NULL) ? 0 : list->count;
}

const dirlist_entry_t *dirlist_get(const dirlist_t *list, int index)
{
  if (list == NULL || index < 0 || index >= list->count)
    return NULL;

  return &list->entries[index];
}
