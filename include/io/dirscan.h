/* io/dirscan.h -- platform-independent flat directory scan */

#ifndef DPTLIB_DIRSCAN_H
#define DPTLIB_DIRSCAN_H

#include "base/result.h"

/**
 * Callback for dirscan_walk(), invoked once per directory entry.
 *
 * \param[in] leaf    The entry's leafname (no path, extension not stripped).
 *                    Borrowed; copy it if it must outlive the call.
 * \param[in] opaque  The pointer passed to dirscan_walk().
 * \return \ref result_OK to continue, \ref result_STOP_WALK to stop early
 *         (dirscan_walk() then also returns \ref result_OK), or any other
 *         code to abort with that code.
 */
typedef result_t (dirscan_fn)(const char *leaf, void *opaque);

/**
 * Walk the entries of a directory (non-recursive, order unspecified),
 * reporting each leafname to \p fn.
 *
 * \param[in] dir     Directory to scan.
 * \param[in] fn      Called once per entry; see dirscan_fn.
 * \param[in] opaque  Passed through to \p fn.
 * \return \ref result_OK on success (including a stop-walk), \ref
 *         result_FILE_NOT_FOUND if \p dir cannot be opened, \ref
 *         result_NULL_ARG, or a code propagated from \p fn.
 */
result_t dirscan_walk(const char *dir, dirscan_fn *fn, void *opaque);

#endif /* DPTLIB_DIRSCAN_H */
