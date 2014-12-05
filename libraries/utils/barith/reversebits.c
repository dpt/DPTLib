/* reversebits.c -- returns the argument reversed bitwise */

#include "utils/barith.h"

unsigned int reversebits(unsigned int x)
{
  unsigned int y;

  y = 0x55555555; x = ((x >> 1) & y) | ((x & y) << 1);
  y = 0x33333333; x = ((x >> 2) & y) | ((x & y) << 2);
  y = 0x0f0f0f0f; x = ((x >> 4) & y) | ((x & y) << 4);
  y = 0x00ff00ff; x = ((x >> 8) & y) | ((x & y) << 8);

  return (x >> 16) | (x << 16);
}
