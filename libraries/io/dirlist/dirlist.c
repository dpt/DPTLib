/* io/dirlist/dirlist.c -- sorted directory listing */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef TARGET_RISCOS
/* SharedCLibrary (-mlibscl) has no stat(): read the catalogue with OS_File */
#include "oslib/osfile.h"
#else
#include <sys/stat.h>
#endif

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
  e->size   = 0;
  e->mtime  = 0;

  /* ponytail: the type comes from the extension only. On RISC OS that misses
   * the filesystem's own filetype (OS_File 23), so every file is &FFD and a
   * Filer double-click finds no app; read it there if that matters. */
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

/* Third pass: each file's length and every entry's modification time.
 * ponytail: one stat per entry. dirscan's walk (OS_GBPB 10 on RISC OS,
 * FindFirstFile on Windows) already has these; thread them out through it
 * if large directories scan slowly. */
#ifdef TARGET_RISCOS

/* seconds from the RISC OS epoch (1900) to the Unix one (1970) */
#define DIRLIST_EPOCH_DELTA 2208988800u

static void dirlist_stat(dirlist_t *list, const char *dir)
{
  char                   path[DPTLIB_MAXPATH];
  int                    i;
  fileswitch_object_type type;
  bits                   load;
  bits                   exec;
  int                    size;
  unsigned long long     secs;

  for (i = 0; i < list->count; i++)
  {
    if (snprintf(path, sizeof(path), "%s.%s", dir,
                 list->entries[i].leaf) >= (int) sizeof(path))
      continue;

    if (xosfile_read_no_path(path, &type, &load, &exec, &size, NULL) != NULL ||
        type == fileswitch_NOT_FOUND)
      continue;

    if (!list->entries[i].is_dir)
      list->entries[i].size = size;

    /* a stamped object holds 5-byte centiseconds since 1900 */
    if ((load & 0xFFF00000u) == 0xFFF00000u)
    {
      secs = (((unsigned long long) (load & 0xFFu) << 32) | exec) / 100;
      if (secs >= DIRLIST_EPOCH_DELTA)
        list->entries[i].mtime = (time_t) (secs - DIRLIST_EPOCH_DELTA);
    }
  }
}

#else

static void dirlist_stat(dirlist_t *list, const char *dir)
{
  char        path[DPTLIB_MAXPATH];
  struct stat st;
  int         i;

  for (i = 0; i < list->count; i++)
  {
    if (snprintf(path, sizeof(path), "%s/%s", dir,
                 list->entries[i].leaf) >= (int) sizeof(path))
      continue;

    if (stat(path, &st) != 0)
      continue;

    if (!list->entries[i].is_dir)
      list->entries[i].size = (long long) st.st_size;
    list->entries[i].mtime = st.st_mtime;
  }
}

#endif

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

  dirlist_stat(l, dir);

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
