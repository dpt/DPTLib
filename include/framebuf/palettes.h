/* palettes.h */

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

void define_pico8_palette(colour_t palette[palette_PICO8__LENGTH]);

#endif /* PALETTES_H */
