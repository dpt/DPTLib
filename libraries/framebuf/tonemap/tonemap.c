/* framebuf/tonemap/tonemap.c -- per-channel tone maps */

#include <assert.h>
#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "base/utils.h"

#include "framebuf/tonemap.h"

/* ----------------------------------------------------------------------- */

enum
{
  MaxComponents = 4, /* red, green, blue, alpha */
};

typedef unsigned int tonemap_internal_flags_t;

#define FLAG_RGB_IN_SYNC       (1u << 0) /* set if RGB components in sync */
#define FLAG_TABLES_UP_TO_DATE (1u << 1)

/* Tone map component */
typedef struct tonemap_comp
{
  tonemap_spec_t  spec;
  tonemap_table_t table;
}
tonemap_comp_t;

struct tonemap
{
  tonemap_internal_flags_t flags;
  int                      ncomponents;
  tonemap_comp_t           comp[MaxComponents];
};

/* ----------------------------------------------------------------------- */

tonemap_t *tonemap_create(void)
{
  tonemap_t *map;

  map = malloc(sizeof(*map));
  if (map == NULL)
    return NULL;

  /* tonemap_reset() initialises the structure for us */

  tonemap_reset(map);

  return map;
}

void tonemap_destroy(tonemap_t *map)
{
  free(map);
}

void tonemap_reset(tonemap_t *map)
{
  tonemap_comp_t *comp;

  map->flags       = FLAG_RGB_IN_SYNC; /* zero UP_TO_DATE flag to build */
  map->ncomponents = 3;

  for (comp = map->comp; comp < map->comp + MaxComponents; comp++)
  {
    comp->spec.flags      = 0;   /* invert, reflect */
    comp->spec.gamma      = 100;
    comp->spec.brightness = 100;
    comp->spec.midd       = 50;
    comp->spec.contrast   = 100;
    comp->spec.bias       = 50;
    comp->spec.gain       = 50;
  }
}

tonemap_t *tonemap_copy(const tonemap_t *map)
{
  tonemap_t *new_map;

  new_map = malloc(sizeof(*new_map));
  if (new_map == NULL)
    return NULL;

  memcpy(new_map, map, sizeof(*new_map));

  return new_map;
}

/* ----------------------------------------------------------------------- */

static void calc(tonemap_t *map)
{
  tonemap_comp_t *comp;

  if (map->flags & FLAG_TABLES_UP_TO_DATE)
    return;

  for (comp = map->comp; comp < map->comp + map->ncomponents; comp++)
  {
    double   igamma; /* inverse */
    int      brightness;
    int      contrast;
    int      midd;
    double   dbias, dgain;
    uint8_t *tab;
    int      i;

    /* inverse of gamma, hence gamma correction */
    igamma     = 100.0 / comp->spec.gamma;

    brightness = comp->spec.brightness - 50;
    brightness = (int) (brightness * 2.56);

    contrast   = (int) (comp->spec.contrast * 2.56);

    midd       = comp->spec.midd;

    dbias = comp->spec.bias / 100.0;
    dbias = CLAMP(dbias, 0.00001, 0.99999);

    dgain = comp->spec.gain / 100.0;
    dgain = CLAMP(dgain, 0.00001, 0.99999);

    tab = comp->table.level;

    for (i = 0; i < 256; i++)
    {
      int j;

      j = (int) ceil(pow(i / 255.0, igamma) * 255.0);

      j = contrast * (j - 128) / 256;

      j += brightness;

      if (i < 128)
        j = j * midd / 50;
      else
        j = ((midd * 255 + 50) / 100) + (j - 128) * (100 - midd) / 50;

      if (comp->spec.bias != 50 || comp->spec.gain != 50)
      {
        double x, t, b, g;

        /* gamma/constrast/brightness result is input to bias */

        x = j / 255.0;
        t = (1.0 / dbias - 2) * (1.0 - x);
        b = x / (t + 1);

        /* bias result is input to gain */

        x = b;
        t = (1.0 / dgain - 2) * (1.0 - 2.0 * x);
        if (x < 0.5)
          g = x / (t + 1.0);
        else
          g = (t - x) / (t - 1.0);

        j = (int) (g * 255.0 + 0.5);
      }

      j = CLAMP(j, 0, 255);
      tab[i] = (uint8_t) j;
    }

    if (comp->spec.flags & tonemap_FLAG_INVERT)
    {
      for (i = 0; i < 127; i++)
      {
        uint8_t t;

        t = tab[i];
        tab[i] = tab[255 - i];
        tab[255 - i] = t;
      }
    }

    if (comp->spec.flags & tonemap_FLAG_REFLECT)
    {
      for (i = 0; i < 127; i++)
        tab[255 - i] = tab[i];
    }

    if (comp == map->comp && (map->flags & FLAG_RGB_IN_SYNC))
      comp = map->comp + 2; /* skip forward to alpha stage (note comp++) */
  }

  if (map->flags & FLAG_RGB_IN_SYNC)
  {
    /* R, G and B are in sync so we skipped over the redundant table
     * generation. Copy the table computed for R into G and B. */

    map->comp[1].table = map->comp[0].table;
    map->comp[2].table = map->comp[0].table;
  }

  map->flags |= FLAG_TABLES_UP_TO_DATE;
}

void tonemap_set(tonemap_t            *map,
                 tonemap_channels_t    channels,
                 const tonemap_spec_t *spec)
{
  tonemap_internal_flags_t intflags;
  tonemap_internal_flags_t rgbflags;
  int                      ncomponents;
  tonemap_comp_t          *comp;
  int                      i;

  intflags = map->flags;

  /* if R + G + B are set simultaneously then remember that */

  rgbflags = channels & tonemap_CHANNEL_RGB;
  if (rgbflags == tonemap_CHANNEL_RGB)
    intflags |= FLAG_RGB_IN_SYNC;
  else if (rgbflags != 0)
    intflags &= ~FLAG_RGB_IN_SYNC;
  /* else r/g/b not being modified */

  /* see if we need to store alpha */

  ncomponents = (channels & tonemap_CHANNEL_ALPHA) ? 4 : 3;
  if (ncomponents > map->ncomponents)
    map->ncomponents = ncomponents;

  /* update all of the requested components */

  for (comp = map->comp, i = 0; i < map->ncomponents; i++, comp++)
  {
    if ((channels & (1u << i)) == 0)
      continue;

    comp->spec = *spec;
  }

  /* tables are now potentially stale */

  intflags &= ~FLAG_TABLES_UP_TO_DATE;

  map->flags = intflags;
}

/* ----------------------------------------------------------------------- */

void tonemap_get_tables(tonemap_t              *map,
                        const tonemap_table_t **red,
                        const tonemap_table_t **green,
                        const tonemap_table_t **blue,
                        const tonemap_table_t **alpha)
{
  static tonemap_table_t linear;
  static int             linear_ready;

  int                    i;

  if (!linear_ready)
  {
    for (i = 0; i < 256; i++)
      linear.level[i] = (uint8_t) i;

    linear_ready = 1;
  }

  calc(map);

  if (red)   *red   = &map->comp[0].table;
  if (green) *green = &map->comp[1].table;
  if (blue)  *blue  = &map->comp[2].table;
  if (alpha)
  {
    if (map->ncomponents >= 4)
      *alpha = &map->comp[3].table;
    else
      *alpha = &linear;
  }
}

/* ----------------------------------------------------------------------- */

void tonemap_get_values(const tonemap_t   *map,
                        tonemap_channels_t channels,
                        tonemap_spec_t    *spec)
{
  int i;

  switch (channels)
  {
  case tonemap_CHANNEL_RGB:
  case tonemap_CHANNEL_RED:   i = 0; break;
  case tonemap_CHANNEL_GREEN: i = 1; break;
  case tonemap_CHANNEL_BLUE:  i = 2; break;
  case tonemap_CHANNEL_ALPHA: i = 3; break;
  default:
    assert(0);
    return;
  }

  *spec = map->comp[i].spec;
}
