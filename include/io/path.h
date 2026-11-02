/* io/path.h -- filename path handling */

#ifndef DPTLIB_PATH
#define DPTLIB_PATH

#include <stddef.h>

#define DPTLIB_MAXPATH 256 /* not ideal */

/* Prefix for scratch files written by tests: /tmp on POSIX, the current
 * directory elsewhere. */
#if defined(__riscos) || defined(_WIN32)
#define DPTLIB_TMPPREFIX ""
#else
#define DPTLIB_TMPPREFIX "/tmp/"
#endif

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
 * matches unchanged (a straight copy into 'name'), except one still holding
 * a foreign "/ext" suffix (e.g. "Bold/map"), which never matches.
 *
 * \return Nonzero on a match, zero if "leaf" lacks 'ext' or does not fit
 *         'cap'.
 */
int path_leaf_strip_ext(const char *leaf,
                        const char *ext,
                        char       *name,
                        size_t      cap);

/**
 * Tests whether 'leaf' is safe to use as a single directory entry name
 * supplied by another task (e.g. a drag-and-drop save's leafname): non-NULL,
 * non-empty, containing no host path separator ('/', plus '\\' on Windows,
 * '.' on RISC OS), and not "." or "..".
 *
 * Rejecting a leafname with a separator stops it reaching outside the
 * intended directory (e.g. "../x" or "a/b").
 *
 * \return Nonzero if 'leaf' is a valid single entry name, zero otherwise.
 */
int path_leaf_valid(const char *leaf);

/**
 * Returns the leafname of 'path': whatever follows its last host path
 * separator ('/', plus '\\' on Windows; '.' or ':' on RISC OS), or 'path'
 * itself if it has none.
 *
 * \return Pointer into 'path'.
 */
const char *path_leaf(const char *path);

/**
 * Tests whether 'path' is already a full (rooted) path under the host
 * convention, rather than a bare leafname or a relative path.
 *
 * Desktop: starts with '/' (also a drive letter, e.g. "C:\\", on Windows).
 * RISC OS: contains a filing system specifier ("<fs>::disc.$.") or starts
 * with '$' (the current directory's root, "$.").
 *
 * \return Nonzero if 'path' is a full path, zero if NULL, empty, or
 *         relative.
 */
int path_is_full(const char *path);

/**
 * Writes the directory holding 'path' into 'out' (capacity 'cap'): 'path' up
 * to its last host path separator ('/' on the desktop, '.' on RISC OS). On
 * the desktop the parent of "/a" is "/".
 *
 * \return Nonzero on success, zero if 'path' is the root, has no separator
 *         or its parent does not fit 'cap'.
 */
int path_parent(const char *path, char *out, size_t cap);

/**
 * Tests whether anything (a file or a directory) is at 'path'.
 *
 * \return Nonzero if 'path' exists, zero if not.
 */
int path_exists(const char *path);

/**
 * Tests whether 'path' is a directory. On RISC OS an image file (such as an
 * archive) is not a directory.
 *
 * \return Nonzero if 'path' is a directory, zero if not or it is absent.
 */
int path_is_dir(const char *path);

/**
 * Writes the per-user data directory for application 'app' into 'out'
 * (capacity 'cap'), creating it and any missing parents. That is
 * `~/Library/Application Support/<app>` on macOS, `%APPDATA%\<app>` on
 * Windows and `<Choices$Write>.<app>` on RISC OS. Elsewhere it is
 * `$XDG_DATA_HOME/<app>`, or `~/.local/share/<app>` when that is unset, with
 * 'app' in lower case.
 *
 * \return Nonzero on success, zero if the base is unknown, the path does not
 *         fit 'cap' or the directory cannot be made.
 */
int path_user_data(const char *app, char *out, size_t cap);

#endif /* DPTLIB_PATH */
