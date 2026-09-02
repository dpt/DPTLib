/* text/txtfmt/impl.h -- text formatting */

#ifndef IMPL_H
#define IMPL_H

typedef struct span
{
  unsigned short start;
  unsigned short length;
}
span;

struct txtfmt
{
  char *s;             /* the string we're formatting */
  int   length;        /* string length excluding terminator */

  span *spans;
  int   nspans;
  int   allocated;

  int   width;         /* requested width */

  int   wrapped_width; /* actual minimum width after formatting */
};

enum
{
  StartAt = 1,
  GrowBy  = 2
};

#endif /* IMPL_H */
