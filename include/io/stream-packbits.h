/* stream-packbits.h -- PackBits compression */

#ifndef STREAM_PACKBITS_H
#define STREAM_PACKBITS_H

#include "base/result.h"
#include "io/stream.h"

/* use 0 for a sensible default buffer size */

result_t stream_packbitscomp_create(stream_t *input, int bufsz, stream_t **s);
result_t stream_packbitsdecomp_create(stream_t *input, int bufsz, stream_t **s);

#endif /* STREAM_PACKBITS_H */
