/* fxp.h -- fixed point helpers */

#ifndef UTILS_FXP_H
#define UTILS_FXP_H

/* Signed multiply of two 16.16 fixed-point numbers. */
int smull_fxp16(int x, int y);

/* Unsigned multiply of two 16.16 fixed-point numbers. */
unsigned int umull_fxp16(unsigned int x, unsigned int y);

#endif /* UTILS_FXP_H */
