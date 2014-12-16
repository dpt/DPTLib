/* stream-mtfcomp.h -- "move to front" adaptive compression stream */

#ifndef STREAM_MTFCOMP_H
#define STREAM_MTFCOMP_H

#include "base/result.h"
#include "io/stream.h"

result_t stream_mtfcomp_create(stream_t *input, int bufsz, stream_t **s);
result_t stream_mtfdecomp_create(stream_t *input, int bufsz, stream_t **s);

#endif /* STREAM_MTFCOMP_H */
