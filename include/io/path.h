/* io/path.h -- filename path handling */

#ifndef DPTLIB_PATH
#define DPTLIB_PATH

#include <stddef.h>

#define DPTLIB_MAXPATH 256 /* not ideal */

/**
 * Join 'leaf' with extension 'ext' according to the host convention.
 *
 * Note: Returns a pointer to an internal static buffer of length
 * `DPTLIB_MAXPATH`.
 */
const char *path_join_leafname(const char *leaf, const char *ext);

/**
 * Join 'root' with `nbranches` directory names according to the host
 * convention.
 *
 * Note: Returns a pointer to an internal static buffer of length
 * `DPTLIB_MAXPATH`.
 */
const char *path_join_filename(const char *root, int nbranches, ...);

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
