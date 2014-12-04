/* likely.h -- hints to compiler of probable execution path */

#ifndef UTILS_LIKELY_H
#define UTILS_LIKELY_H

#ifdef __GNUC__
#define likely(expr)   __builtin_expect(!!(expr), 1)
#define unlikely(expr) __builtin_expect(!!(expr), 0)
#else
#define likely(expr)   (expr)
#define unlikely(expr) (expr)
#endif

#endif /* UTILS_LIKELY_H */
