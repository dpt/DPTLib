/* countbits.c -- returns the number of bits set in the argument */

#include "utils/barith.h"

/* Source: Graphics Gems II: 'Of Integers, Fields, and Bit Counting', p371.
 */
int countbits(unsigned int x)
{
  x -=  (x >> 1) & 0x55555555;
  x  = ((x >> 2) & 0x33333333) + (x & 0x33333333);
  x  = ((x >> 4) + x) & 0x0f0f0f0f;
  x +=   x >> 8;
  x +=   x >> 16;

  return x & 0x3f;
}
