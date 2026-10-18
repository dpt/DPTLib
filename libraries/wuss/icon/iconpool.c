/* wuss/icon/iconpool.c -- wuss-wide icon set loaded from a directory of PNGs */

#include <assert.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef FORTIFY
#include "fortify/fortify.h"
#endif

#include "base/result.h"
#include "datastruct/atom.h"
#include "framebuf/bitmap.h"
#include "framebuf/pixelfmt.h"
#include "io/dirscan.h"
#include "io/path.h"

/* pathf returns a single static buffer, so the per-entry join in the
 * callback would clobber a caller's dir pointer if we held it directly --
 * keep our own copy. Bounded by that buffer's own DPTLIB_MAXPATH. */
#define ICONS_DIR_MAX 256

#include "../core/types.h"
#include "../core/alloc.h"
#include "../core/windows.h"

#ifdef WUSS_ICONS

/* State threaded through the dirscan callback while a set is being built. The
 * three arrays grow in lockstep via wuss__array_grow. */
typedef struct icons_load
{
  wuss_t          *wuss;
  char             dir[ICONS_DIR_MAX]; /* own copy; see ICONS_DIR_MAX */
  struct atom_set *names;   /* owned until handed to the pool */
  int             *atoms;   /* owned; atoms[i] is entry i's atom in names */
  bitmap_t *bitmaps; /* owned */
  int       n;
  int       cap;     /* shared allocation length of atoms[] and bitmaps[] */
  result_t  rc;      /* first failure; stops the walk */
}
icons_load_t;

/* The RLE blit (screen_copy_bitmap_rle) decodes straight into the screen's
 * byte layout -- no per-pixel format convert -- so a compressed icon must
 * already be in screen order. bitmap_load_png yields rgb(x|a)8888; the screen
 * is usually bgr(x|a)8888. Swap R and B in place (byte 0 <-> byte 2; the
 * alpha/pad byte is untouched, so this maps rgbx<->bgrx and rgba<->bgra) and
 * relabel the format. Bitmap must be uncompressed and 32bpp.
 * ponytail: lives here until a second caller needs rgb<->bgr; then fold into
 * bitmap_convert. */
static void icons_swap_rb(bitmap_t *bm)
{
  unsigned char *row;
  int            x, y;

  assert(!pixelfmt_is_rle(bm->format));
  assert(pixelfmt_log2bpp(bm->format) == 5);

  for (y = 0; y < bm->size.h; y++)
  {
    row = (unsigned char *) bm->base + (size_t) y * bm->rowbytes;
    for (x = 0; x < bm->size.w; x++)
    {
      unsigned char t;

      t          = row[x * 4 + 0];
      row[x * 4 + 0] = row[x * 4 + 2];
      row[x * 4 + 2] = t;
    }
  }

  switch (bm->format)
  {
  case pixelfmt_rgbx8888: bm->format = pixelfmt_bgrx8888; break;
  case pixelfmt_rgba8888: bm->format = pixelfmt_bgra8888; break;
  case pixelfmt_bgrx8888: bm->format = pixelfmt_rgbx8888; break;
  case pixelfmt_bgra8888: bm->format = pixelfmt_rgba8888; break;
  default: break;
  }
}

/* The RLE codec only turns wholly-zero pixels into skip runs, and the 32bpp
 * RLE blit writes every other pixel as is, alpha and all. A colour-keyed PNG
 * loads with the key colour kept under alpha 0 (e.g. magenta), which would
 * then be drawn. Zero every alpha-0 pixel of an rgba/bgra bitmap so it
 * compresses to a skip. Bitmap must be uncompressed. */
static void icons_clear_transparent(bitmap_t *bm)
{
  unsigned char *row;
  int            x, y;

  assert(!pixelfmt_is_rle(bm->format));

  if (bm->format != pixelfmt_rgba8888 && bm->format != pixelfmt_bgra8888)
    return;

  for (y = 0; y < bm->size.h; y++)
  {
    row = (unsigned char *) bm->base + (size_t) y * bm->rowbytes;
    for (x = 0; x < bm->size.w; x++)
    {
      if (row[x * 4 + 3] == 0)
        memset(&row[x * 4], 0, 4);
    }
  }
}

/* True when "a" and "b" are 32bpp RGB(A/X) formats of opposite R/B order. */
static int icons_rb_swapped(pixelfmt_t a, pixelfmt_t b)
{
  return (a == pixelfmt_rgbx8888 && b == pixelfmt_bgrx8888) ||
         (a == pixelfmt_bgrx8888 && b == pixelfmt_rgbx8888) ||
         (a == pixelfmt_rgba8888 && b == pixelfmt_bgra8888) ||
         (a == pixelfmt_bgra8888 && b == pixelfmt_rgba8888) ||
         (a == pixelfmt_rgbx8888 && b == pixelfmt_bgra8888) ||
         (a == pixelfmt_rgba8888 && b == pixelfmt_bgrx8888) ||
         (a == pixelfmt_bgrx8888 && b == pixelfmt_rgba8888) ||
         (a == pixelfmt_bgra8888 && b == pixelfmt_rgbx8888);
}

/* The channel order a 32bpp icon must hold to blit onto screen format scr:
 * the screen's own on a 32bpp screen, else rgba -- the paletted RLE blits
 * decode into an rgba8888 scratch row. */
static pixelfmt_t icons_order_for(pixelfmt_t scr)
{
  return (pixelfmt_log2bpp(scr) == 5) ? scr : pixelfmt_rgba8888;
}

static result_t icons_load_entry(const char *leaf, void *opaque)
{
  result_t      rc;
  icons_load_t *st = opaque;
  char          name[128];
  const char   *path;
  bitmap_t      bm;
  atom_t        atom;
  int           oldcap;

  if (!path_leaf_strip_ext(leaf, ".png", name, sizeof(name)))
    return result_OK; /* not a ".png", or name too long -- skip */

  path = pathf("%s/%s", st->dir, leaf);

  rc = bitmap_load_png(&bm, path);
  if (rc != result_OK)
  {
    st->rc = rc;
    return result_STOP_WALK;
  }

  /* bring the pixels into screen byte order before compressing -- see
   * icons_swap_rb. Only the R/B-swapped 32bpp case is handled; a matching
   * format needs nothing, anything else is left for the blit to reject. */
  if (pixelfmt_log2bpp(bm.format) == 5 &&
      icons_rb_swapped(bm.format, icons_order_for(st->wuss->scr->format)))
    icons_swap_rb(&bm);

  icons_clear_transparent(&bm);

  /* paletted formats can't be RLE-compressed: keep those uncompressed */
  rc = bitmap_compress(&bm);
  if (rc != result_OK && rc != result_NOT_SUPPORTED)
  {
    free(bm.base);
    st->rc = rc;
    return result_STOP_WALK;
  }

  /* Intern the name. atom_new returns result_ATOM_NAME_EXISTS (with the
   * existing atom) for a repeat -- a directory scan can't repeat a leaf, so
   * treat only a hard error as failure. */
  rc = atom_new(st->names,
                (const unsigned char *) name, strlen(name) + 1, &atom);
  if (rc != result_OK && rc != result_ATOM_NAME_EXISTS)
  {
    free(bm.base);
    st->rc = rc;
    return result_STOP_WALK;
  }

  oldcap = st->cap;
  if (wuss__array_grow(&st->wuss->alloc, (void **) &st->bitmaps,
                       sizeof(*st->bitmaps), st->n, &st->cap, 1, 8) != 0)
  {
    free(bm.base);
    st->rc = result_OOM;
    return result_STOP_WALK;
  }
  if (st->cap != oldcap) /* bitmaps[] grew; bring atoms[] to the same length */
  {
    int *grown;

    grown = wuss__realloc(st->wuss, st->atoms,
                          (size_t) st->cap * sizeof(*st->atoms));
    if (grown == NULL)
    {
      free(bm.base);
      st->rc = result_OOM;
      return result_STOP_WALK;
    }
    st->atoms = grown;
  }

  st->atoms[st->n]     = (int) atom;
  st->bitmaps[st->n++] = bm;
  return result_OK;
}

void wuss__iconpool_clear(wuss_t *wuss, struct wuss_iconpool *pool)
{
  int i;

  assert(wuss != NULL);
  assert(pool != NULL);

  for (i = 0; i < pool->nbitmaps; i++)
    free(pool->bitmaps[i].base);

  wuss__free(wuss, pool->bitmaps);
  wuss__free(wuss, pool->atoms);
  pool->bitmaps  = NULL;
  pool->atoms    = NULL;
  pool->nbitmaps = 0;

  if (pool->names != NULL)
  {
    atom_destroy(pool->names);
    pool->names = NULL;
  }
}

static result_t icons_match_pool(pixelfmt_t            order,
                                 struct wuss_iconpool *pool)
{
  result_t  rc;
  bitmap_t *bm;
  int       i;

  for (i = 0; i < pool->nbitmaps; i++)
  {
    bm = &pool->bitmaps[i];
    if (pixelfmt_log2bpp(pixelfmt_base(bm->format)) != 5 ||
        !icons_rb_swapped(pixelfmt_base(bm->format), order))
      continue;

    /* in place: icons hold pointers to these bitmap_t structs, so only
     * their pixel stores may move */
    rc = bitmap_decompress(bm);
    if (rc != result_OK)
      return rc;

    icons_swap_rb(bm);

    rc = bitmap_compress(bm);
    if (rc != result_OK)
      return rc;
  }

  return result_OK;
}

result_t wuss__iconpool_match_screen(wuss_t *wuss)
{
  result_t   rc;
  pixelfmt_t order;
  list_t    *e;

  assert(wuss != NULL);

  order = icons_order_for(wuss->scr->format);

  rc = icons_match_pool(order, &wuss->icon);
  if (rc != result_OK)
    return rc;

  for (e = wuss->tasks.next; e != NULL; e = e->next)
  {
    rc = icons_match_pool(order, &wuss__task_from_link(e)->iconpool);
    if (rc != result_OK)
      return rc;
  }

  return result_OK;
}

/* Order the entries by name (strcmp) so indices don't depend on the platform's
 * directory scan order. Insertion sort: a handful of icons. atoms[] and
 * bitmaps[] are permuted in lockstep. */
static void icons_sort(struct atom_set *names,
                       int             *atoms,
                       bitmap_t        *bitmaps,
                       int              n)
{
  const char *name;
  bitmap_t    bm;
  int         atom;
  int         i, j;

  for (i = 1; i < n; i++)
  {
    atom = atoms[i];
    bm   = bitmaps[i];
    name = (const char *) atom_get(names, (atom_t) atom, NULL);

    for (j = i; j > 0; j--)
    {
      const char *prev;

      prev = (const char *) atom_get(names, (atom_t) atoms[j - 1],
                                     NULL);
      if (strcmp(prev, name) <= 0)
        break;

      atoms[j]   = atoms[j - 1];
      bitmaps[j] = bitmaps[j - 1];
    }

    atoms[j]   = atom;
    bitmaps[j] = bm;
  }
}

/* Build "pool" from every "*.png" in dir, replacing any previous contents. */
static result_t icons_load(wuss_t               *wuss,
                           struct wuss_iconpool *pool,
                           const char           *dir)
{
  result_t     rc;
  icons_load_t st;
  int          i;

  if (strlen(dir) >= sizeof(st.dir))
    return result_BAD_ARG;

  wuss__iconpool_clear(wuss, pool);

  st.wuss    = wuss;
  st.names   = atom_create();
  st.atoms   = NULL;
  st.bitmaps = NULL;
  st.n       = 0;
  st.cap     = 0;
  st.rc      = result_OK;
  strcpy(st.dir, dir);

  if (st.names == NULL)
    return result_OOM;

  rc = dirscan_walk(dir, icons_load_entry, &st);

  /* A missing directory just yields no entries. */
  if (rc == result_FILE_NOT_FOUND)
    rc = result_OK;

  if (rc == result_OK && st.rc != result_OK)
    rc = st.rc;

  if (rc != result_OK)
  {
    for (i = 0; i < st.n; i++)
      free(st.bitmaps[i].base);
    wuss__free(wuss, st.bitmaps);
    wuss__free(wuss, st.atoms);
    atom_destroy(st.names);
    return rc;
  }

  icons_sort(st.names, st.atoms, st.bitmaps, st.n);

  pool->names    = st.names;
  pool->atoms    = st.atoms;
  pool->bitmaps  = st.bitmaps;
  pool->nbitmaps = st.n;
  return result_OK;
}

result_t wuss_iconpool_load(wuss_t *wuss, const char *dir)
{
  assert(wuss != NULL);

  if (dir == NULL)
    return result_NULL_ARG;

  return icons_load(wuss, &wuss->icon, dir);
}

/* Copy "<resources>/resources/<sub>" into dir (size ICONS_DIR_MAX): pathf
 * hands back one shared static buffer, and the per-file joins would clobber
 * it mid-load. */
static void icons_resource_dir(char       *dir,
                               const char *resources,
                               const char *sub)
{
  strncpy(dir, pathf("%s/resources/%s", resources, sub), ICONS_DIR_MAX - 1);
  dir[ICONS_DIR_MAX - 1] = '\0';
}

result_t wuss_iconpool_load_resource(wuss_t *wuss, const char *resources)
{
  char dir[ICONS_DIR_MAX];

  assert(wuss != NULL);

  if (resources == NULL)
    return result_NULL_ARG;

  icons_resource_dir(dir, resources, "wuss/iconpool");

  return wuss_iconpool_load(wuss, dir);
}

/* Index of "name" in pool, or -1. */
static int icons_find(const struct wuss_iconpool *pool, const char *name)
{
  atom_t atom;
  int    i;

  if (name == NULL || pool->names == NULL)
    return -1;

  atom = atom_for_block(pool->names, (const unsigned char *) name,
                        strlen(name) + 1);
  if (atom == atom_NOT_FOUND)
    return -1;

  /* atom values are not array indices -- find which entry holds it. A handful
   * of icons, so a linear scan is cheaper than a second index structure. */
  for (i = 0; i < pool->nbitmaps; i++)
    if (pool->atoms[i] == (int) atom)
      return i;

  return -1;
}

int wuss_iconpool_count(const wuss_t *wuss)
{
  assert(wuss != NULL);
  return wuss->icon.nbitmaps;
}

int wuss_iconpool_lookup(const wuss_t *wuss, const char *name)
{
  assert(wuss != NULL);

  return icons_find(&wuss->icon, name);
}

const bitmap_t *wuss_iconpool_bitmap(const wuss_t *wuss, int index)
{
  assert(wuss != NULL);

  if (index < 0 || index >= wuss->icon.nbitmaps)
    return NULL;

  return &wuss->icon.bitmaps[index];
}

const char *wuss_iconpool_name(const wuss_t *wuss, int index)
{
  assert(wuss != NULL);

  if (index < 0 || index >= wuss->icon.nbitmaps)
    return NULL;

  return (const char *) atom_get(wuss->icon.names,
                                 (atom_t) wuss->icon.atoms[index], NULL);
}

/* ----------------------------------------------------------------------- */

result_t wuss_task_iconpool_load(wuss_task_t *task, const char *dir)
{
  assert(task != NULL);

  if (dir == NULL)
    return result_NULL_ARG;

  return icons_load(task->wuss, &task->iconpool, dir);
}

result_t wuss_task_iconpool_load_resource(wuss_task_t *task,
                                          const char  *resources)
{
  char sub[ICONS_DIR_MAX];
  char dir[ICONS_DIR_MAX];

  assert(task != NULL);

  if (resources == NULL)
    return result_NULL_ARG;

  if (task->name == NULL)
    return result_BAD_ARG;

  if ((size_t) snprintf(sub, sizeof(sub), "wuss-tasks/%s/iconpool",
                        task->name) >= sizeof(sub))
    return result_BAD_ARG;

  icons_resource_dir(dir, resources, sub);

  return wuss_task_iconpool_load(task, dir);
}

const bitmap_t *wuss__iconpool_find(const wuss_t      *wuss,
                                    const wuss_task_t *task,
                                    const char        *name)
{
  int idx;

  assert(wuss != NULL);

  if (task != NULL)
  {
    idx = icons_find(&task->iconpool, name);
    if (idx >= 0)
      return &task->iconpool.bitmaps[idx];
  }

  idx = icons_find(&wuss->icon, name);

  return (idx >= 0) ? &wuss->icon.bitmaps[idx] : NULL;
}

const bitmap_t *wuss_task_iconpool_find(const wuss_task_t *task,
                                        const char        *name)
{
  assert(task != NULL);

  return wuss__iconpool_find(task->wuss, task, name);
}

#endif /* WUSS_ICONS */
