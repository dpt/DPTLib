/* framebuf/bmfont/family.c -- bitmap font families and style selection */

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "base/result.h"
#include "framebuf/bmfontfamily.h"
#include "io/dirscan.h"
#include "io/path.h"

/* ----------------------------------------------------------------------- */

#define BMFONT_EXT ".png"

#ifdef TARGET_RISCOS
#define DIR_SEPARATORS "/\\."
#else
#define DIR_SEPARATORS "/\\"
#endif

struct bmfontfamily
{
  char                *name;
  bmfontface_t *faces;
  int                  nfaces;
  int                  cap;
};

typedef struct
{
  char            dir[512]; /* own copy: see bmfontfamily_scan */
  bmfontfamily_t *family;
}
scan_ctx_t;

/* ----------------------------------------------------------------------- */

/* Set *weight and *slant from a style name: an optional weight word then an
 * optional "Italic". Anything unrecognised counts as Regular. */
static void parse_style(const char            *style,
                        bmfontfamily_weight_t *weight,
                        bmfontfamily_slant_t  *slant)
{
  static const struct
  {
    const char            *name;
    bmfontfamily_weight_t  weight;
  }
  weights[] =
  {
    { "Thin",    bmfontfamily_WEIGHT_THIN    },
    { "Light",   bmfontfamily_WEIGHT_LIGHT   },
    { "Regular", bmfontfamily_WEIGHT_REGULAR },
    { "Medium",  bmfontfamily_WEIGHT_MEDIUM  },
    { "Bold",    bmfontfamily_WEIGHT_BOLD    },
    { "Black",   bmfontfamily_WEIGHT_BLACK   }
  };
  static const char italic[] = "Italic";

  size_t len;
  size_t italic_len;
  size_t i;

  len        = strlen(style);
  italic_len = sizeof(italic) - 1;

  *slant = bmfontfamily_SLANT_UPRIGHT;
  if (len >= italic_len && strcmp(style + len - italic_len, italic) == 0)
  {
    *slant = bmfontfamily_SLANT_ITALIC;
    len   -= italic_len;
  }

  *weight = bmfontfamily_WEIGHT_REGULAR;
  for (i = 0; i < sizeof(weights) / sizeof(weights[0]); i++)
  {
    if (strlen(weights[i].name) == len &&
        strncmp(style, weights[i].name, len) == 0)
    {
      *weight = weights[i].weight;
      break;
    }
  }
}

static char *dup_string(const char *s)
{
  size_t len;
  char  *copy;

  len  = strlen(s) + 1;
  copy = malloc(len);
  if (copy != NULL)
    memcpy(copy, s, len);

  return copy;
}

static int face_cmp(const void *a, const void *b)
{
  const bmfontface_t *fa;
  const bmfontface_t *fb;

  fa = a;
  fb = b;

  if (fa->weight != fb->weight)
    return (int) fa->weight - (int) fb->weight;
  if (fa->slant != fb->slant)
    return (int) fa->slant - (int) fb->slant;

  return strcmp(fa->style, fb->style);
}

static result_t scan_entry(const char *leaf, void *opaque)
{
  scan_ctx_t     *ctx;
  bmfontfamily_t *family;
  bmfontface_t   *grown;
  bmfontface_t   *face;
  char            style[256];
  int             cap;

  ctx    = opaque;
  family = ctx->family;

  if (!path_leaf_strip_ext(leaf, BMFONT_EXT, style, sizeof(style)))
    return result_OK;

  if (family->nfaces == family->cap)
  {
    cap = family->cap ? family->cap * 2 : 4;

    grown = realloc(family->faces, (size_t) cap * sizeof(*grown));
    if (grown == NULL)
      return result_OOM;
    family->faces = grown;
    family->cap   = cap;
  }

  face = &family->faces[family->nfaces];

  face->style = dup_string(style);
  /* copied out of pathf's shared static buffer straight away */
  face->path  = dup_string(pathf("%s/%s", ctx->dir, leaf));
  if (face->style == NULL || face->path == NULL)
  {
    free((void *) face->style); /* discard const */
    free((void *) face->path);  /* discard const */
    return result_OOM;
  }

  parse_style(style, &face->weight, &face->slant);
  family->nfaces++;

  return result_OK;
}

/* ----------------------------------------------------------------------- */

result_t bmfontfamily_scan(const char *dir, bmfontfamily_t **family)
{
  result_t        rc;
  scan_ctx_t      ctx;
  bmfontfamily_t *f;
  size_t          len;
  size_t          i;
  const char     *leaf;

  if (dir == NULL || family == NULL)
    return result_NULL_ARG;

  f = calloc(1, sizeof(*f));
  if (f == NULL)
    return result_OOM;

  /* own copy: dir may point into pathf's shared static buffer, which
   * scan_entry's own pathf call would otherwise clobber mid-walk */
  snprintf(ctx.dir, sizeof(ctx.dir), "%s", dir);
  ctx.family = f;

  /* the family name is the last component, ignoring trailing separators */
  len = strlen(ctx.dir);
  while (len > 1 && strchr(DIR_SEPARATORS, ctx.dir[len - 1]) != NULL)
    ctx.dir[--len] = '\0';
  leaf = ctx.dir;
  for (i = 0; i < len; i++)
  {
    if (strchr(DIR_SEPARATORS, ctx.dir[i]) != NULL)
      leaf = ctx.dir + i + 1;
  }
  f->name = dup_string(leaf);
  if (f->name == NULL)
  {
    bmfontfamily_destroy(f);
    return result_OOM;
  }

  rc = dirscan_walk(ctx.dir, scan_entry, &ctx);
  if (rc == result_OK && f->nfaces == 0)
    rc = result_NOT_FOUND;
  if (rc != result_OK)
  {
    bmfontfamily_destroy(f);
    return rc;
  }

  qsort(f->faces, (size_t) f->nfaces, sizeof(f->faces[0]), face_cmp);

  *family = f;

  return result_OK;
}

result_t bmfontfamily_label_path(const char *dir,
                                 const char *label,
                                 char       *path,
                                 size_t      cap)
{
  const char *space;
  char        family[256];
  size_t      len;

  if (dir == NULL || label == NULL || path == NULL)
    return result_NULL_ARG;

  space = strrchr(label, ' ');
  if (space == NULL)
    return result_BAD_ARG;

  len = (size_t) (space - label);
  if (len >= sizeof(family))
    return result_BUFFER_OVERFLOW;
  memcpy(family, label, len);
  family[len] = '\0';

  /* pathf's buffer is shared, so a copy is unavoidable */
  if ((size_t) snprintf(path, cap, "%s",
                        pathf("%s/%s/%s" BMFONT_EXT, dir, family,
                              space + 1)) >= cap)
    return result_BUFFER_OVERFLOW;

  return result_OK;
}

void bmfontfamily_destroy(bmfontfamily_t *family)
{
  int i;

  if (family == NULL)
    return;

  for (i = 0; i < family->nfaces; i++)
  {
    free((void *) family->faces[i].style); /* discard const */
    free((void *) family->faces[i].path);  /* discard const */
  }
  free(family->faces);
  free(family->name);
  free(family);
}

const char *bmfontfamily_name(const bmfontfamily_t *family)
{
  return family->name;
}

int bmfontfamily_count(const bmfontfamily_t *family)
{
  return family->nfaces;
}

const bmfontface_t *bmfontfamily_face(const bmfontfamily_t *family,
                                      int                   index)
{
  if (index < 0 || index >= family->nfaces)
    return NULL;

  return &family->faces[index];
}

const bmfontface_t *bmfontfamily_find(const bmfontfamily_t *family,
                                      bmfontfamily_weight_t weight,
                                      bmfontfamily_slant_t  slant)
{
  int i;

  for (i = 0; i < family->nfaces; i++)
    if (family->faces[i].weight == weight && family->faces[i].slant == slant)
      return &family->faces[i];

  return NULL;
}

const bmfontface_t *bmfontfamily_heavier(const bmfontfamily_t *family,
                                         const bmfontface_t   *face)
{
  const bmfontface_t *best;
  int                 i;

  best = face;

  /* faces are sorted by weight, so the first heavier one is the nearest */
  for (i = 0; i < family->nfaces; i++)
  {
    if (family->faces[i].slant == face->slant &&
        family->faces[i].weight > face->weight)
    {
      best = &family->faces[i];
      break;
    }
  }

  return best;
}

const bmfontface_t *bmfontfamily_lighter(const bmfontfamily_t *family,
                                         const bmfontface_t   *face)
{
  const bmfontface_t *best;
  int                 i;

  best = face;

  /* the last lighter one in weight order is the nearest */
  for (i = 0; i < family->nfaces; i++)
  {
    if (family->faces[i].slant == face->slant &&
        family->faces[i].weight < face->weight)
      best = &family->faces[i];
  }

  return best;
}

const bmfontface_t *bmfontfamily_with_slant(const bmfontfamily_t *family,
                                            const bmfontface_t   *face,
                                            bmfontfamily_slant_t  slant)
{
  const bmfontface_t *best;
  int                 best_gap;
  int                 gap;
  int                 i;

  best     = face;
  best_gap = -1;

  /* ascending weight order: a strict "<" keeps the lighter face on a tie */
  for (i = 0; i < family->nfaces; i++)
  {
    if (family->faces[i].slant != slant)
      continue;

    gap = (int) family->faces[i].weight - (int) face->weight;
    if (gap < 0)
      gap = -gap;

    if (best_gap < 0 || gap < best_gap)
    {
      best     = &family->faces[i];
      best_gap = gap;
    }
  }

  return best;
}
