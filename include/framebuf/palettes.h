/* framebuf/palettes.h -- standard palettes */

#ifndef PALETTES_H
#define PALETTES_H

#include "framebuf/colour.h"

/* PICO-8 palette */

#define palette_PICO8_BLACK        (0)
#define palette_PICO8_DARK_BLUE    (1)
#define palette_PICO8_DARK_PURPLE  (2)
#define palette_PICO8_DARK_GREEN   (3)
#define palette_PICO8_BROWN        (4)
#define palette_PICO8_DARK_GREY    (5)
#define palette_PICO8_LIGHT_GREY   (6)
#define palette_PICO8_WHITE        (7)
#define palette_PICO8_RED          (8)
#define palette_PICO8_ORANGE       (9)
#define palette_PICO8_YELLOW      (10)
#define palette_PICO8_GREEN       (11)
#define palette_PICO8_BLUE        (12)
#define palette_PICO8_LAVENDER    (13)
#define palette_PICO8_PINK        (14)
#define palette_PICO8_LIGHT_PEACH (15)
#define palette_PICO8__LENGTH     (16)

/* RISC OS desktop (Wimp) 16-colour palette, in native Wimp index order */

#define palette_WIMP16_WHITE       (0)
#define palette_WIMP16_GREY_87     (1)
#define palette_WIMP16_GREY_75     (2)
#define palette_WIMP16_GREY_62     (3)
#define palette_WIMP16_GREY_50     (4)
#define palette_WIMP16_GREY_37     (5)
#define palette_WIMP16_GREY_25     (6)
#define palette_WIMP16_BLACK       (7)
#define palette_WIMP16_DARK_BLUE   (8)
#define palette_WIMP16_YELLOW      (9)
#define palette_WIMP16_GREEN      (10)
#define palette_WIMP16_RED        (11)
#define palette_WIMP16_CREAM      (12)
#define palette_WIMP16_DARK_GREEN (13)
#define palette_WIMP16_ORANGE     (14)
#define palette_WIMP16_LIGHT_BLUE (15)
#define palette_WIMP16__LENGTH    (16)

/**
 * Define the PICO-8 palette.
 *
 * \param[out] palette PICO-8 palette.
 */
void define_pico8_palette(colour_t palette[palette_PICO8__LENGTH]);

/**
 * Define the RISC OS 16-colour desktop (Wimp) palette in native Wimp index
 * order, addressed by the `palette_WIMP16_*` names.
 *
 * \param[out] palette RISC OS 16-colour palette.
 */
void define_wimp16_palette(colour_t palette[palette_WIMP16__LENGTH]);

#endif /* PALETTES_H */
