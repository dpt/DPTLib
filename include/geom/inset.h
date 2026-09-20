/* geom/inset.h -- edge inset type */

#ifndef GEOM_INSET_H
#define GEOM_INSET_H

/** Per-edge insets, e.g. a container's padding. */
typedef struct inset
{
  int t, r, b, l;
}
inset_t;

/** Construct an inset_t compound literal. */
#define INSET(t, r, b, l) ((inset_t) { (t), (r), (b), (l) })

#endif /* GEOM_INSET_H */
