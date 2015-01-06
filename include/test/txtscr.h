/* txtscr.h -- text format 'screen' */

#ifndef TEST_TXTSCR_H
#define TEST_TXTSCR_H

#include "geom/box.h"

#define T txtscr_t

typedef struct txtscr T;

T *txtscr_create(int width, int height);
void txtscr_destroy(T *doomed);

void txtscr_clear(T *scr);

/* adds to the pixels, rather than overwriting */
void txtscr_addbox(T *scr, const box_t *box);

void txtscr_print(T *scr);

#undef T

#endif /* TEST_TXTSCR_H */
