/* geom/inset.h -- edge inset type */

#ifndef GEOM_INSET_H
#define GEOM_INSET_H

/** Per-edge insets, e.g. a container's padding. */
typedef struct inset
{
  int t, r, b, l;
}
inset_t;

/** Construct an inset_t initialiser; valid both as an expression and inside
 *  a static aggregate initialiser (a compound literal is not a constant
 *  expression on RISC OS's GCCSDK). */
#define INSET(t, r, b, l) { (t), (r), (b), (l) }

#endif /* GEOM_INSET_H */
