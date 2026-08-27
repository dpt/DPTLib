/* span-registry.h -- registry of plotting methods */

#ifndef SPAN_REGISTRY_H
#define SPAN_REGISTRY_H

#include "framebuf/pixelfmt.h"
#include "framebuf/span.h"

/**
 * Find an appropriate span for the specified pixel format.
 *
 * \param[in] fmt Required pixel format.
 * \return A span, or NULL if no span is available for the specified pixel
 *         format.
 */
const span_t *spanregistry_get(pixelfmt_t fmt);

#endif /* SPAN_REGISTRY_H */
