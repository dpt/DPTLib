/* result.h -- generic function return values */

#ifndef BASE_RESULT_H
#define BASE_RESULT_H

typedef int result_t;

#define result_BASE_GENERIC      0x0000
#define result_BASE_STREAM       0x0100
#define result_BASE_PLAYER       0x0200 // move to momask code

#define result_OK                0
#define result_OOM               1 /* out of memory */
#define result_FILE_NOT_FOUND    2
#define result_BAD_ARG           3
#define result_BUFFER_OVERFLOW   4

#endif /* BASE_RESULT_H */
