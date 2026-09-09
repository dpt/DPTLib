/* wuss/icon/registry.c -- wuss-wide icon set loaded from a directory of PNGs */

#include <assert.h>
#include <stddef.h>
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

/* path_join_filename returns a single static buffer, so the per-entry join in
 * the callback would clobber a caller's dir pointer if we held it directly --
 * keep our own copy. Bounded by that buffer's own DPTLIB_MAXPATH. */
#define ICONS_DIR_MAX 256

#include "../core/impl.h"

#ifdef WUSS_ICONS

/* State threaded through the dirscan callback while a set is being built. The
 * three arrays grow in lockstep via wuss__array_grow. */
typedef struct icons_load
{
  wuss_t   *wuss;
  char      dir[ICONS_DIR_MAX]; /* own copy; see ICONS_DIR_MAX */
  int      *atoms;   /* owned; atoms[i] is entry i's atom in wuss->icon.names */
  bitmap_t *bitmaps; /* owned */
  int       n;
  int       cap;     /* shared allocation length of atoms[] and bitmaps[] */
  result_t  rc;      /* first failure; stops the walk */
}
icons_load_t;

/* True when "leaf" ends in ".png" (case-sensitive, matching the fixtures).
 * Writes the leaf sans that suffix into "name" (capacity "cap"). */
static int png_leaf(const char *leaf, char *name, size_t cap)
{
  size_t len;

  len = strlen(leaf);
  if (len < 4 || len - 4 >= cap)
    return 0;
  if (strcmp(leaf + len - 4, ".png") != 0)
    return 0;

  memcpy(name, leaf, len - 4);
  name[len - 4] = '\0';
  return 1;
}

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

/* True when "a" and "b" are 32bpp RGB(A/X) formats of opposite R/B order. */
static int icons_rb_swapped(pixelfmt_t a, pixelfmt_t b)
{
  return (a == pixelfmt_rgbx8888 && b == pixelfmt_bgrx8888) ||
         (a == pixelfmt_bgrx8888 && b == pixelfmt_rgbx8888) ||
         (a == pixelfmt_rgba8888 && b == pixelfmt_bgra8888) ||
         (a == pixelfmt_bgra8888 && b == pixelfmt_rgba8888) ||
         (a == pixelfmt_rgbx8888 && b == pixelfmt_bgra8888) ||
         (a == pixelfmt_rgba8888 && b == pixelfmt_bgrx8888);
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

  if (!png_leaf(leaf, name, sizeof(name)))
    return result_OK; /* not a ".png", or name too long -- skip */

  path = path_join_filename(st->dir, 1, leaf);

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
      icons_rb_swapped(bm.format, st->wuss->scr->format))
    icons_swap_rb(&bm);

  rc = bitmap_compress(&bm);
  if (rc != result_OK)
  {
    free(bm.base);
    st->rc = rc;
    return result_STOP_WALK;
  }

  /* Intern the name. atom_new returns result_ATOM_NAME_EXISTS (with the
   * existing atom) for a repeat -- a directory scan can't repeat a leaf, so
   * treat only a hard error as failure. */
  rc = atom_new(st->wuss->icon.names,
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

void wuss__icons_registry_free(wuss_t *wuss)
{
  int i;

  assert(wuss != NULL);

  for (i = 0; i < wuss->icon.nbitmaps; i++)
    free(wuss->icon.bitmaps[i].base);

  wuss__free(wuss, wuss->icon.bitmaps);
  wuss__free(wuss, wuss->icon.atoms);
  wuss->icon.bitmaps  = NULL;
  wuss->icon.atoms    = NULL;
  wuss->icon.nbitmaps = 0;

  if (wuss->icon.names != NULL)
  {
    atom_destroy(wuss->icon.names);
    wuss->icon.names = NULL;
  }
}

result_t wuss_icons_load(wuss_t *wuss, const char *dir)
{
  result_t     rc;
  icons_load_t st;
  int          i;

  assert(wuss != NULL);

  if (dir == NULL)
    return result_NULL_ARG;

  wuss__icons_registry_free(wuss);

  wuss->icon.names = atom_create();
  if (wuss->icon.names == NULL)
    return result_OOM;

  if (strlen(dir) >= sizeof(st.dir))
  {
    atom_destroy(wuss->icon.names);
    wuss->icon.names = NULL;
    return result_BAD_ARG;
  }

  st.wuss    = wuss;
  strcpy(st.dir, dir);
  st.atoms   = NULL;
  st.bitmaps = NULL;
  st.n       = 0;
  st.cap     = 0;
  st.rc      = result_OK;

  rc = dirscan_walk(dir, icons_load_entry, &st);

  /* A missing directory just yields an empty set. */
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
    atom_destroy(wuss->icon.names);
    wuss->icon.names = NULL;
    return rc;
  }

  wuss->icon.atoms    = st.atoms;
  wuss->icon.bitmaps  = st.bitmaps;
  wuss->icon.nbitmaps = st.n;
  return result_OK;
}

int wuss_icons_count(const wuss_t *wuss)
{
  assert(wuss != NULL);
  return wuss->icon.nbitmaps;
}

int wuss_icons_lookup(const wuss_t *wuss, const char *name)
{
  atom_t atom;
  int    i;

  assert(wuss != NULL);

  if (name == NULL || wuss->icon.names == NULL)
    return -1;

  atom = atom_for_block((atom_set_t *) wuss->icon.names,
                        (const unsigned char *) name, strlen(name) + 1);
  if (atom == atom_NOT_FOUND)
    return -1;

  /* atom values are not array indices -- find which entry holds it. A handful
   * of icons, so a linear scan is cheaper than a second index structure. */
  for (i = 0; i < wuss->icon.nbitmaps; i++)
    if (wuss->icon.atoms[i] == (int) atom)
      return i;

  return -1;
}

const bitmap_t *wuss_icons_bitmap(const wuss_t *wuss, int index)
{
  assert(wuss != NULL);

  if (index < 0 || index >= wuss->icon.nbitmaps)
    return NULL;

  return &wuss->icon.bitmaps[index];
}

#endif /* WUSS_ICONS */
