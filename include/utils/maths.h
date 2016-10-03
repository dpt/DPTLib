/* maths.h -- math utils */

#ifndef UTILS_MATHS_H
#define UTILS_MATHS_H

#ifdef __cplusplus
extern "C"
{
#endif

/* Greatest common divisor of m,n */
int gcd(int m, int n);

/* Degrees to radians */
double degs_to_rads(double degs);

#ifdef __cplusplus
}
#endif

#endif /* UTILS_MATHS_H */
