/* span-registry.h -- registry of plotting methods */

#ifndef SPAN_REGISTRY_H
#define SPAN_REGISTRY_H

#include "framebuf/pixelfmt.h"
#include "framebuf/span.h"

const span_t *spanregistry_get(pixelfmt_t fmt);

#endif /* SPAN_REGISTRY_H */
