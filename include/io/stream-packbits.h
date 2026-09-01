/* io/stream-packbits.h -- PackBits compression */

#ifndef STREAM_PACKBITS_H
#define STREAM_PACKBITS_H

#ifdef __cplusplus
extern "C"
{
#endif

#include "base/result.h"
#include "io/stream.h"

/**
 * Create a PackBits compression stream.
 *
 * \param[in]  input  Input stream.
 * \param[in]  bufsz  Buffer size in bytes (0 for a sensible default).
 * \param[out] s      Pointer to a `stream_t` pointer to store the created
 *                    stream.
 * \return \ref result_OK on success, or appropriate result code otherwise.
 */
result_t stream_packbitscomp_create(stream_t  *input,
                                    int        bufsz,
                                    stream_t **s);

/**
 * Create a PackBits decompression stream.
 *
 * \param[in]  input  Input stream.
 * \param[in]  bufsz  Buffer size in bytes (0 for a sensible default).
 * \param[out] s      Pointer to a `stream_t` pointer to store the created
 *                    stream.
 * \return \ref result_OK on success, or appropriate result code otherwise.
 */
result_t stream_packbitsdecomp_create(stream_t  *input,
                                      int        bufsz,
                                      stream_t **s);

#ifdef __cplusplus
}
#endif

#endif /* STREAM_PACKBITS_H */
