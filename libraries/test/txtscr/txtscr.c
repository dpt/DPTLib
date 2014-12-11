/* txtscr.c -- text format 'screen' */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef FORTIFY
#include "fortify/fortify.h"
#endif

#include "geom/box.h"

#include "test/txtscr.h"

struct txtscr_t
{
  int   width, height;
  char *screen;
};

txtscr_t *txtscr_create(int width, int height)
{
  txtscr_t *scr;

  scr = malloc(sizeof(*scr));
  if (scr == NULL)
    return NULL;

  scr->screen = malloc(width * height * sizeof(*scr->screen));
  if (scr->screen == NULL)
  {
    free(scr);
    return NULL;
  }

  scr->width  = width;
  scr->height = height;

  return scr;
}

void txtscr_destroy(txtscr_t *doomed)
{
  if (!doomed)
    return;

  free(doomed->screen);
  free(doomed);
}

void txtscr_clear(txtscr_t *scr)
{
  memset(scr->screen, 0, scr->width * scr->height * sizeof(*scr->screen));
}

/* adds to the pixels, rather than overwriting */
void txtscr_addbox(txtscr_t *scr, const box_t *box)
{
  int x,y;

  for (y = box->y0; y < box->y1; y++)
    for (x = box->x0; x < box->x1; x++)
      scr->screen[y * scr->width + x]++;
}

void txtscr_print(txtscr_t *scr)
{
  static const char map[] = ".123456789ABCDEF";

  int x,y;

  for (y = scr->height - 1; y >= 0; y--)
  {
    for (x = 0; x < scr->width; x++)
    {
      int c;

      c = scr->screen[y * scr->width + x];
      c = (c >= sizeof(map) - 1) ? '?' : map[c];

      printf("%c", c);
    }
    printf("\n");
  }
}
