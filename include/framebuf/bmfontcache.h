/* framebuf/bmfontcache.h -- shared bitmap font loader */

/**
 * \file bmfontcache.h
 *
 * Loads bmfont_t fonts by path, handing back an existing instance instead of
 * reading the same PNG twice when it is already held. Each acquire must be
 * matched by a release; a font is only actually destroyed once its last
 * holder releases it.
 */

#ifndef DPTLIB_BMFONTCACHE_H
#define DPTLIB_BMFONTCACHE_H

#include "base/result.h"
#include "framebuf/bmfont.h"

/** A bitmap font cache handle. */
typedef struct bmfontcache bmfontcache_t;

/**
 * Create a new, empty font cache.
 *
 * \param[out] cache  Newly allocated font cache.
 * \return \ref result_OK on success, or appropriate result code otherwise.
 */
result_t bmfontcache_create(bmfontcache_t **cache);

/**
 * Destroy a font cache and every font still held in it, regardless of
 * outstanding acquires. Callers must not use any bmfont_t obtained from it
 * afterwards.
 *
 * \param[in] cache    Font cache to destroy.
 */
void bmfontcache_destroy(bmfontcache_t *cache);

/**
 * Fetch the bitmap font for \p path, loading it if this is the first request
 * for that exact path string (see below), or handing back the already-loaded
 * instance and bumping its reference count otherwise.
 *
 * \p path is matched by exact string comparison, not resolved on disk, so
 * two different spellings of the same file (relative vs absolute, a trailing
 * slash, etc) are treated as different fonts and loaded twice.
 *
 * \param[in]  cache   Font cache to acquire from.
 * \param[in]  path    Filename of the font file to load.
 * \param[out] bmfont  The bitmap font for \p path.
 * \return \ref result_OK on success, or appropriate result code otherwise.
 */
result_t bmfontcache_acquire(bmfontcache_t *cache,
                             const char    *path,
                             bmfont_t     **bmfont);

/**
 * Release a bitmap font previously obtained from bmfontcache_acquire(). Once
 * every acquirer of a given font has released it, the font is destroyed and
 * \p bmfont must not be used again.
 *
 * \param[in] cache   Font cache \p bmfont was acquired from.
 * \param[in] bmfont  Bitmap font to release.
 */
void bmfontcache_release(bmfontcache_t *cache, bmfont_t *bmfont);

#endif /* DPTLIB_BMFONTCACHE_H */
