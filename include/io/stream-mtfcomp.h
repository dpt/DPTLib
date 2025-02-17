/* stream-mtfcomp.h -- "move to front" adaptive compression stream */

#ifndef STREAM_MTFCOMP_H
#define STREAM_MTFCOMP_H

#ifdef __cplusplus
extern "C"
{
#endif

#include "base/result.h"
#include "io/stream.h"

/**
 * Create a "move to front" adaptive compression stream.
 *
 * \param[in]  input  The input stream.
 * \param[in]  bufsz  The buffer size.
 * \param[out] s      Pointer to a `stream_t` pointer to store the created stream.
 * \return \ref result_OK on success, or appropriate result code otherwise.
 */
result_t stream_mtfcomp_create(stream_t *input, int bufsz, stream_t **s);

/**
 * Create a "move to front" adaptive decompression stream.
 *
 * \param[in]  input  The input stream.
 * \param[in]  bufsz  The buffer size.
 * \param[out] s      Pointer to a `stream_t` pointer to store the created stream.
 * \return \ref result_OK on success, or appropriate result code otherwise.
 */
result_t stream_mtfdecomp_create(stream_t *input, int bufsz, stream_t **s);

#ifdef __cplusplus
}
#endif

#endif /* STREAM_MTFCOMP_H */
