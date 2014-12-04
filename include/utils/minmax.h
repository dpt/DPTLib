/* minmax.h -- clamping numbers */

#ifndef UTILS_MINMAX_H
#define UTILS_MINMAX_H

#define MIN(a,b) (((a) < (b)) ? (a) : (b))
#define MAX(a,b) (((a) > (b)) ? (a) : (b))
#define CLAMP(a,b,c) MIN(MAX(a,b),c)

#endif /* UTILS_MINMAX_H */
