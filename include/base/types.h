/* types.h -- fixed-width integer types */

#ifndef BASE_TYPES_H
#define BASE_TYPES_H

#if defined(__STDC__) && __STDC__ && __STDC_VERSION__ >= 199901L

#include <stdint.h>

#else

typedef signed   char      int8_t;
typedef signed   short     int16_t;
typedef signed   int       int32_t;
typedef signed   long long int64_t;

typedef unsigned char      uint8_t;
typedef unsigned short     uint16_t;
typedef unsigned int       uint32_t;
typedef unsigned long long uint64_t;

#endif

#endif /* BASE_TYPES_H */
