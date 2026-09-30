/* io/dirlist.h -- sorted directory listing */

#ifndef DPTLIB_DIRLIST_H
#define DPTLIB_DIRLIST_H

#include "base/result.h"

#include "io/filetype.h"

/**
 * \file dirlist.h
 *
 * A directory's entries, scanned in full (no cap, nothing silently dropped),
 * sorted by leafname and with dotfiles filtered out. Built on dirscan_walk /
 * dirscan_walk_dirs; unlike io/namelist (a fixed-size table for a single
 * extension) this grows to fit and reports both files and directories, each
 * with a filetype_t.
 */

/** One directory entry. */
typedef struct dirlist_entry
{
  char      *leaf;     /* owned copy of the leafname */
  int        is_dir;   /* non-zero if a subdirectory */
  filetype_t type;     /* filetype_DATA for a directory */
}
dirlist_entry_t;

/** Opaque handle onto a scanned, sorted directory listing. */
typedef struct dirlist dirlist_t;

/**
 * Scan 'dir' and build a sorted listing of its entries. Entries whose
 * leafname starts with '.' are omitted. The listing is sorted by leafname,
 * case-insensitively.
 *
 * \param[in]  dir    Directory to scan.
 * \param[out] list   Receives the new listing. Owned by the caller; free
 *                    with dirlist_destroy.
 * \return \ref result_OK, \ref result_NULL_ARG if 'dir' or 'list' is NULL,
 *         \ref result_FILE_NOT_FOUND if 'dir' could not be opened, or \ref
 *         result_OOM.
 */
result_t dirlist_scan(const char *dir, dirlist_t **list);

/** Free a listing returned by dirlist_scan. Passing NULL is a no-op. */
void dirlist_destroy(dirlist_t *list);

/** Number of entries in 'list'. */
int dirlist_count(const dirlist_t *list);

/**
 * Fetch entry 'index' (0-based, in sorted order).
 *
 * \param[in] list  Listing, as returned by dirlist_scan.
 * \param[in] index Entry index; must be in [0, dirlist_count(list)).
 * \return Borrowed pointer to the entry, valid until 'list' is destroyed or
 *         re-scanned. NULL if 'index' is out of range.
 */
const dirlist_entry_t *dirlist_get(const dirlist_t *list, int index);

#endif /* DPTLIB_DIRLIST_H */
