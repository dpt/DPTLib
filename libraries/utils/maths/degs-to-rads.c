/* degs-to-rads.c -- degrees to radians */

#include "utils/maths.h"

#define TWICEPI 6.2831853071795864769252867665590

double degs_to_rads(double degs)
{
  return degs * TWICEPI / 360.0;
}
