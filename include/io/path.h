/* path.h -- filename path handling */

#ifndef DPTLIB_PATH
#define DPTLIB_PATH

#define DPTLIB_MAXPATH 256 /* not ideal */

/**
 * Join 'leaf' with extension 'ext' according to the host convention.
 *
 * Note: Returns a pointer to an internal static buffer of length `DPTLIB_MAXPATH`.
 */
const char *path_join_leafname(const char *leaf, const char *ext);

/**
 * Join 'root' with `nbranches` directory names according to the host convention.
 *
 * Note: Returns a pointer to an internal static buffer of length `DPTLIB_MAXPATH`.
 */
const char *path_join_filename(const char *root, int nbranches, ...);

#endif /* DPTLIB_PATH */
