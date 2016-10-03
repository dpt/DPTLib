/* stream-mtfcomp.h -- "move to front" adaptive compression stream */

#ifndef STREAM_MTFCOMP_H
#define STREAM_MTFCOMP_H

#ifdef __cplusplus
extern "C"
{
#endif

#include "base/result.h"
#include "io/stream.h"

result_t stream_mtfcomp_create(stream_t *input, int bufsz, stream_t **s);
result_t stream_mtfdecomp_create(stream_t *input, int bufsz, stream_t **s);

#ifdef __cplusplus
}
#endif

#endif /* STREAM_MTFCOMP_H */
