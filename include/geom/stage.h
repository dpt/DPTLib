/* geom/stage.h -- page background layout: pasteboard, border, margin,
 * shadow */

#ifndef GEOM_STAGE_H
#define GEOM_STAGE_H

#ifdef __cplusplus
extern "C"
{
#endif

#include <stddef.h>

#include "geom/box.h"

/**
 * \file stage.h
 *
 * Lays out the background of a document: a content area (the page) inside a
 * margin, surrounded by a stroked border and an optional drop shadow, all
 * floating on a pasteboard that fills the rest of the work area. The page is
 * centred in the work area. The output is a list of boxes, each tagged with
 * what to draw there, ready to be filled by the caller.
 */

/** Maximum number of boxes stage_generate will emit. */
#define stage_MAX_BOXES (17)

typedef unsigned int stageflags_t;
#define stage_FLAG_SHADOW (1u << 0) /**< Draw a drop shadow. */

/** What to draw in a stage box. */
typedef enum stageboxkind
{
  stageboxkind_PASTEBOARD, /**< Area around the page. */
  stageboxkind_STROKE,     /**< Border outline of the page. */
  stageboxkind_MARGIN,     /**< Space between border and content. */
  stageboxkind_CONTENT,    /**< The page itself; always the first box. */
  stageboxkind_SHADOW,     /**< Drop shadow right and below the page. */
  stageboxkind__LIMIT
}
stageboxkind_t;

/** One rectangle of the background and what to draw in it. */
typedef struct stagebox
{
  stageboxkind_t kind;
  box_t          box;
}
stagebox_t;

/** Stage settings. All sizes are in the caller's units (e.g. pixels). */
typedef struct stageconfig
{
  stageflags_t flags;

  int          pasteboard_min; /**< Pasteboard minimum size on each axis. */
  int          stroke;         /**< Border outline thickness. */
  int          margin;         /**< Space between the content and border. */
  int          shadow;         /**< Drop shadow offset. */

  int          log2_pixel_x;   /**< log2 of the size of a device pixel in the
                                    caller's units; sizes are rounded to
                                    whole pixels. 0 if units are pixels. */
  int          log2_pixel_y;   /**< As log2_pixel_x, vertically. */
}
stageconfig_t;

/**
 * Set a config to the defaults: a shadow, 16 pasteboard, a 1 stroke, 8
 * margin, 4 shadow and 1:1 pixels.
 *
 * \param[out] config Config to initialise.
 */
void stageconfig_init(stageconfig_t *config);

/**
 * Return the sum of the fixed size elements that the stage wants to present.
 *
 * \param[in]  config Stage settings.
 * \param[out] width  Fixed width.
 * \param[out] height Fixed height.
 */
void stage_get_fixed(const stageconfig_t *config,
                     int                 *width,
                     int                 *height);

/**
 * Given work area and page dimensions, emit the boxes to draw. The content
 * box is first. The rest are sorted from the top left. Content is rounded
 * down to whole pixels and other dimensions up.
 *
 * \param[in]  config              Stage settings.
 * \param[in]  workarea_width      Work area width.
 * \param[in]  workarea_height     Work area height.
 * \param[in]  content_width       Page width.
 * \param[in]  content_height      Page height.
 * \param[out] min_workarea_width  Smallest work area width that fits the
 *                                 stage, or NULL.
 * \param[out] min_workarea_height Smallest work area height that fits the
 *                                 stage, or NULL.
 * \param[out] boxes               Receives up to stage_MAX_BOXES boxes.
 * \param[out] nboxes              Receives the number of boxes written.
 */
void stage_generate(const stageconfig_t *config,
                    int                  workarea_width,
                    int                  workarea_height,
                    int                  content_width,
                    int                  content_height,
                    int                 *min_workarea_width,
                    int                 *min_workarea_height,
                    stagebox_t          *boxes,
                    size_t              *nboxes);

#ifdef __cplusplus
}
#endif

#endif /* GEOM_STAGE_H */
