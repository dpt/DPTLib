/* box.h -- box type */

#ifndef GEOM_BOX_H
#define GEOM_BOX_H

typedef struct box
{
  int x0, y0, x1, y1;
}
box_t;

/* return the box "c" as the intersection of boxes "a" and "b" */
void box_intersection(const box_t *a, const box_t *b, box_t *c);

/* return if the specified box is empty */
int box_is_empty(const box_t *a);

#endif /* GEOM_BOX_H */
