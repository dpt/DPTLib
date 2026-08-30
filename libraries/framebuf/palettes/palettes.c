/* palettes.c */

#include "framebuf/colour.h"

#include "framebuf/palettes.h"

void define_pico8_palette(colour_t palette[palette_PICO8__LENGTH])
{
  palette[palette_PICO8_BLACK      ] = colour_rgb(0x00, 0x00, 0x00);
  palette[palette_PICO8_DARK_BLUE  ] = colour_rgb(0x1D, 0x2B, 0x53);
  palette[palette_PICO8_DARK_PURPLE] = colour_rgb(0x7E, 0x25, 0x53);
  palette[palette_PICO8_DARK_GREEN ] = colour_rgb(0x00, 0x87, 0x51);
  palette[palette_PICO8_BROWN      ] = colour_rgb(0xAB, 0x52, 0x36);
  palette[palette_PICO8_DARK_GREY  ] = colour_rgb(0x5F, 0x57, 0x4F);
  palette[palette_PICO8_LIGHT_GREY ] = colour_rgb(0xC2, 0xC3, 0xC7);
  palette[palette_PICO8_WHITE      ] = colour_rgb(0xFF, 0xF1, 0xE8);
  palette[palette_PICO8_RED        ] = colour_rgb(0xFF, 0x00, 0x4D);
  palette[palette_PICO8_ORANGE     ] = colour_rgb(0xFF, 0xA3, 0x00);
  palette[palette_PICO8_YELLOW     ] = colour_rgb(0xFF, 0xEC, 0x27);
  palette[palette_PICO8_GREEN      ] = colour_rgb(0x00, 0xE4, 0x36);
  palette[palette_PICO8_BLUE       ] = colour_rgb(0x29, 0xAD, 0xFF);
  palette[palette_PICO8_LAVENDER   ] = colour_rgb(0x83, 0x76, 0x9C);
  palette[palette_PICO8_PINK       ] = colour_rgb(0xFF, 0x77, 0xA8);
  palette[palette_PICO8_LIGHT_PEACH] = colour_rgb(0xFF, 0xCC, 0xAA);
}

/* The standard RISC OS desktop (Wimp) 16-colour palette in native Wimp index
 * order: a greyscale ramp (0-7) then eight colours. */
void define_wimp16_palette(colour_t palette[palette_WIMP16__LENGTH])
{
  palette[palette_WIMP16_WHITE     ] = colour_rgb(0xFF, 0xFF, 0xFF);
  palette[palette_WIMP16_GREY_87   ] = colour_rgb(0xDD, 0xDD, 0xDD);
  palette[palette_WIMP16_GREY_75   ] = colour_rgb(0xBB, 0xBB, 0xBB);
  palette[palette_WIMP16_GREY_62   ] = colour_rgb(0x99, 0x99, 0x99);
  palette[palette_WIMP16_GREY_50   ] = colour_rgb(0x77, 0x77, 0x77);
  palette[palette_WIMP16_GREY_37   ] = colour_rgb(0x55, 0x55, 0x55);
  palette[palette_WIMP16_GREY_25   ] = colour_rgb(0x33, 0x33, 0x33);
  palette[palette_WIMP16_BLACK     ] = colour_rgb(0x00, 0x00, 0x00);
  palette[palette_WIMP16_DARK_BLUE ] = colour_rgb(0x00, 0x44, 0x99);
  palette[palette_WIMP16_YELLOW    ] = colour_rgb(0xEE, 0xEE, 0x00);
  palette[palette_WIMP16_GREEN     ] = colour_rgb(0x00, 0xCC, 0x00);
  palette[palette_WIMP16_RED       ] = colour_rgb(0xDD, 0x00, 0x00);
  palette[palette_WIMP16_CREAM     ] = colour_rgb(0xEE, 0xEE, 0xBB);
  palette[palette_WIMP16_DARK_GREEN] = colour_rgb(0x55, 0x88, 0x00);
  palette[palette_WIMP16_ORANGE    ] = colour_rgb(0xFF, 0xBB, 0x00);
  palette[palette_WIMP16_LIGHT_BLUE] = colour_rgb(0x00, 0xBB, 0xFF);
}
