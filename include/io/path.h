/* io/path.h -- filename path handling */

#ifndef DPTLIB_PATH
#define DPTLIB_PATH

#include <stddef.h>

#define DPTLIB_MAXPATH 256 /* not ideal */

/**
 * Build a path from a printf-like format string, substituting `%s` from the
 * varargs exactly as printf would, then rewriting the result for the host
 * path convention: every literal '/' becomes the host directory separator,
 * and on RISC OS the final ".ext" suffix (the dotted extension after the
 * last '/'), if present, is dropped -- file type there is separate
 * filesystem metadata, never part of the pathname string.
 *
 * Only `%s` conversions are supported.
 *
 * Note: Returns a pointer to an internal static buffer of length
 * `DPTLIB_MAXPATH`.
 */
const char *pathf(const char *fmt, ...);

/**
 * Tests whether a directory leafname (as returned by dirscan_walk) has the
 * dotted extension 'ext', e.g. ".png", under the host convention. On a
 * match, writes the leaf with that extension stripped into 'name' (capacity
 * 'cap') and returns nonzero.
 *
 * On RISC OS a leafname carries no extension in the string at all -- file
 * type is separate filesystem metadata, not a suffix -- so every leaf
 * matches unchanged (a straight copy into 'name').
 *
 * \return Nonzero on a match, zero if "leaf" lacks 'ext' or does not fit
 *         'cap'.
 */
int path_leaf_strip_ext(const char *leaf,
                        const char *ext,
                        char       *name,
                        size_t      cap);

#endif /* DPTLIB_PATH */
