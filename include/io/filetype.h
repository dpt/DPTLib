/* io/filetype.h -- file type identification and RISC OS filetype mapping */

#ifndef DPTLIB_FILETYPE_H
#define DPTLIB_FILETYPE_H

#include "base/result.h"

/**
 * A file's type, carried in both forms a caller might need: the RISC OS
 * filetype number (used as filesystem metadata there, and in the drag-save
 * protocol's DataSave message) and the host extension (used to build a
 * leafname off RISC OS, dotted, e.g. ".png"). Either representation can be
 * derived from the other via filetype_from_ext / filetype_from_riscos.
 *
 * The table covers every type on RISC OS Open's File Types list, except
 * those marked unregistered or "do not use". Where a number has more than
 * one claimant, the OS-defined or registered one is kept.
 */
typedef struct filetype
{
  unsigned short riscos;   /* RISC OS filetype number, e.g. 0xB60 */
  const char    *ext;      /* dotted host extension, e.g. ".png", or "" */
  const char    *name;     /* RISC OS type name, e.g. "PNG", or "" */
}
filetype_t;

/** RISC OS filetype numbers of the types in the table. */
#define filetype_RISCOS_PNG      0xB60
#define filetype_RISCOS_TEXT     0xFFF
/** Markdown has no allocated RISC OS filetype yet; this is a placeholder in
 *  the user range (&100-&3FF) until RISC OS Open allocates one. */
#define filetype_RISCOS_MARKDOWN 0x1D0

/** Unknown/data filetype: RISC OS &FFD, no host extension. */
extern const filetype_t filetype_DATA;

/**
 * Look up a filetype by its host extension.
 * \param[in] ext Dotted extension, e.g. ".png". Matched case-insensitively.
 *                Any extension the type is known by is matched, so ".jpeg"
 *                gives the JPEG type, whose 'ext' is ".jpg". NULL or
 *                unrecognised yields filetype_DATA.
 * \return The matching filetype_t (by value; the table owns 'ext' and
 *         'name', which are always string literals, so the result is safe to
 *         copy and keep).
 */
filetype_t filetype_from_ext(const char *ext);

/**
 * Look up a filetype by its RISC OS filetype number.
 * \param[in] riscos RISC OS filetype number.
 * \return The matching filetype_t, or filetype_DATA if riscos is not in the
 *         table (still carrying riscos through unchanged, so round-tripping
 *         an unrecognised number does not lose it, and with an empty name).
 */
filetype_t filetype_from_riscos(unsigned short riscos);

/**
 * Set a file's type as filesystem metadata. A no-op returning result_OK
 * everywhere except RISC OS, where the host extension carries no type
 * information and this is the only way to record one.
 * \param[in] path Full host path of an existing file.
 * \param[in] type Type to set.
 * \return \ref result_OK, \ref result_NULL_ARG if 'path' is NULL, or \ref
 *         result_FILE_NOT_FOUND if the file could not be found to set the
 *         type on (RISC OS only).
 */
result_t filetype_set(const char *path, filetype_t type);

#endif /* DPTLIB_FILETYPE_H */
