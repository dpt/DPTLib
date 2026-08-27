/* layout.h -- laying out elements using the packer */

#ifndef GEOM_LAYOUT_H
#define GEOM_LAYOUT_H

#ifdef __cplusplus
extern "C"
{
#endif

#include "base/result.h"
#include "geom/box.h"
#include "geom/packer.h"

/* ----------------------------------------------------------------------- */

#define result_LAYOUT_BUFFER_FULL (result_BASE_LAYOUT + 0)

/* ----------------------------------------------------------------------- */

/** A layout element. */
typedef struct layout_element
{
  enum
  {
    layout_BOX,
    layout_NEWLINE,
  }
  type;

  union
  {
    /** Box layout parameters. */
    struct
    {
      int min_width, max_width; /**< Minimum and maximum width of the box. */
      int height;               /**< Height of the box. */
    }
    box;
  }
  data;
}
layout_element_t;

/** A layout specification. */
typedef struct layout_spec
 {
  packer_t         *packer;  /**< Packer to use.   */
  packer_loc_t      loc;     /**< Packer location. */
  packer_cleardir_t clear;   /**< Clear direction. */
  int               spacing; /**< Spacing. */
  int               leading; /**< Leading. */
}
layout_spec_t;

/**
 * Place layout elements into boxes.
 *
 * \param[in]  spec      Layout specification.
 * \param[in]  elements  Array of layout elements.
 * \param[in]  nelements Number of layout elements given.
 * \param[out] boxes     An array of boxes to be populated.
 * \param[in]  nboxes    Number of boxes available.
 * \return \ref result_OK on success, result_LAYOUT_BUFFER_FULL if too few boxes
 *         were supplied, or appropriate result code otherwise.
 */
result_t layout_place(const layout_spec_t    *spec,
                      const layout_element_t *elements,
                      int                     nelements,
                      box_t                  *boxes,
                      int                     nboxes);

#ifdef __cplusplus
}
#endif

#endif /* GEOM_LAYOUT_H */
