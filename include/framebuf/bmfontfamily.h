/* framebuf/bmfontfamily.h -- bitmap font families and style selection */

/**
 * \file bmfontfamily.h
 *
 * A font family is one directory under resources/bmfonts holding one PNG per
 * face, named for its style: "Regular.png", "Bold.png", "Italic.png",
 * "BoldItalic.png" and so on. The directory's leafname is the family name.
 * Scanning a family reads only the directory listing; no PNG is loaded, so
 * selection queries are cheap. Load the chosen face's path with
 * bmfontcache_acquire().
 *
 * A style name is an optional weight (Thin, Light, Regular, Medium, Bold or
 * Black) followed by an optional "Italic". A name with no recognised weight
 * counts as Regular. Size is not an axis: a sized cut of a design is a
 * separate family, e.g. "DPT-Digits" and "DPT-DigitsLg".
 */

#ifndef DPTLIB_BMFONTFAMILY_H
#define DPTLIB_BMFONTFAMILY_H

#include <stddef.h>

#include "base/result.h"

/** Font weight, lightest first. Order is significant. */
typedef enum bmfontfamily_weight
{
  bmfontfamily_WEIGHT_THIN,
  bmfontfamily_WEIGHT_LIGHT,
  bmfontfamily_WEIGHT_REGULAR,
  bmfontfamily_WEIGHT_MEDIUM,
  bmfontfamily_WEIGHT_BOLD,
  bmfontfamily_WEIGHT_BLACK
}
bmfontfamily_weight_t;

/** Font slant. */
typedef enum bmfontfamily_slant
{
  bmfontfamily_SLANT_UPRIGHT,
  bmfontfamily_SLANT_ITALIC
}
bmfontfamily_slant_t;

/**
 * One face of a family. Owned by the family; valid until it is destroyed.
 */
typedef struct bmfontface
{
  const char            *style;  /**< Leafname less ".png", e.g. "Bold". */
  const char            *path;   /**< Path for bmfont_create(). */
  bmfontfamily_weight_t  weight;
  bmfontfamily_slant_t   slant;
}
bmfontface_t;

/** A scanned font family handle. */
typedef struct bmfontfamily bmfontfamily_t;

/**
 * Scan a family directory for its faces.
 *
 * \param[in]  dir     The family's directory, e.g. "resources/bmfonts/Tiny".
 * \param[out] family  Newly allocated family. Free it with
 *                     bmfontfamily_destroy().
 * \return \ref result_OK on success, \ref result_FILE_NOT_FOUND if \p dir
 *         cannot be opened, \ref result_NOT_FOUND if it holds no faces, or
 *         appropriate result code otherwise.
 */
result_t bmfontfamily_scan(const char *dir, bmfontfamily_t **family);

/**
 * Build the path for a face from its "Family Style" label, as reported by
 * bmfont_enumerate() (the style is whatever follows the last space).
 *
 * \param[in]  dir    Directory holding the family directories.
 * \param[in]  label  Label such as "GrongyUI Bold".
 * \param[out] path   Receives the path for bmfont_create().
 * \param[in]  cap    Capacity of \p path.
 * \return \ref result_OK on success, \ref result_BAD_ARG if \p label has no
 *         space, or \ref result_BUFFER_OVERFLOW if it does not fit.
 */
result_t bmfontfamily_label_path(const char *dir,
                                 const char *label,
                                 char       *path,
                                 size_t      cap);

/**
 * Destroy a family and every face it owns.
 *
 * \param[in] family  Family to destroy; NULL is ignored.
 */
void bmfontfamily_destroy(bmfontfamily_t *family);

/**
 * \return The family name (the directory's leafname). Borrowed.
 */
const char *bmfontfamily_name(const bmfontfamily_t *family);

/**
 * \return The number of faces in the family.
 */
int bmfontfamily_count(const bmfontfamily_t *family);

/**
 * Get a face by index. Faces are ordered by weight, then slant (upright
 * first), then style name.
 *
 * \param[in] family  Family to look in.
 * \param[in] index   0 to bmfontfamily_count() - 1.
 * \return The face, or NULL if \p index is out of range.
 */
const bmfontface_t *bmfontfamily_face(const bmfontfamily_t *family,
                                      int                   index);

/**
 * Find the face with exactly this weight and slant.
 *
 * \return The face, or NULL if the family has none.
 */
const bmfontface_t *bmfontfamily_find(const bmfontfamily_t *family,
                                      bmfontfamily_weight_t weight,
                                      bmfontfamily_slant_t  slant);

/**
 * Get the next heavier face of the same slant.
 *
 * \return The nearest face of greater weight, or \p face itself if the
 *         family has none heavier.
 */
const bmfontface_t *bmfontfamily_heavier(const bmfontfamily_t *family,
                                         const bmfontface_t   *face);

/**
 * Get the next lighter face of the same slant.
 *
 * \return The nearest face of lesser weight, or \p face itself if the family
 *         has none lighter.
 */
const bmfontface_t *bmfontfamily_lighter(const bmfontfamily_t *family,
                                         const bmfontface_t   *face);

/**
 * Switch slant, keeping weight where possible.
 *
 * \return The face of the requested slant nearest in weight to \p face (the
 *         lighter one on a tie), or \p face itself if the family has no face
 *         of that slant.
 */
const bmfontface_t *bmfontfamily_with_slant(const bmfontfamily_t *family,
                                            const bmfontface_t   *face,
                                            bmfontfamily_slant_t  slant);

#endif /* DPTLIB_BMFONTFAMILY_H */
