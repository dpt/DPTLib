/* util.h -- utils for reversing bytesex */

#include <limits.h>

#ifndef BYTESEX_UTIL_H
#define BYTESEX_UTIL_H

/* Rotate 'x' right by 'n' bits. */
#define ROR(type, x, n) ( ((x) >> (n)) | ((x) << ((sizeof(type) * CHAR_BIT) - (n))) )

#endif /* BYTESEX_UTIL_H */
