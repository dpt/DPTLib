/* colour.h -- colour type */

#ifndef FRAMEBUF_COLOUR_H
#define FRAMEBUF_COLOUR_H

#include "framebuf/pixelfmt.h"

typedef struct colour colour_t;

struct colour
{
  pixelfmt_rgba8888_t primary;
};

/**
 * Create a colour from RGB components.
 *
 * \param[in] r Red component.
 * \param[in] g Green component.
 * \param[in] b Blue component.
 * \return Colour.
 */
colour_t colour_rgb(int r, int g, int b);

/**
 * Create a colour from RGBA components.
 *
 * \param[in] r Red component.
 * \param[in] g Green component.
 * \param[in] b Blue component.
 * \param[in] a Alpha component.
 * \return Colour.
 */
colour_t colour_rgba(int r, int g, int b, int a);

/**
 * Return colour `c` as a pixel of format `fmt`.
 *
 * \param[in] palette   Palette to use.
 * \param[in] nentries  Number of entries in the palette.
 * \param[in] required  Required colour.
 * \param[in] fmt       Pixel format.
 * \return Pixel.
 */
pixelfmt_any_t colour_to_pixel(const colour_t *palette,
                               int             nentries,
                               colour_t        required,
                               pixelfmt_t      fmt);

/**
 * Return the alpha component of the specified colour.
 *
 * \param[in] c Colour.
 * \return Alpha component.
 */
unsigned int colour_get_alpha(const colour_t *c);

#endif /* FRAMEBUF_COLOUR_H */
